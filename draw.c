/* See LICENSE file for copyright and license details. */
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#ifdef PANGO
#include <pango/pango.h>
#include <pango/pangoxft.h>
#endif
#include "draw.h"

#ifndef MAX
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif

void
drawrect(DC *dc, int x, int y, unsigned int w, unsigned int h, Bool fill, unsigned long color) {
	XSetForeground(dc->dpy, dc->gc, color);
	if(fill)
		XFillRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x, dc->y + y, w, h);
	else
		XDrawRectangle(dc->dpy, dc->canvas, dc->gc, dc->x + x, dc->y + y, w-1, h-1);
}

void
drawtext(DC *dc, const char *text, ColorSet *col) {
	char buf[BUFSIZ];
	size_t mn, n = strlen(text);

	/* shorten text if necessary */
	for(mn = MIN(n, sizeof buf); textnw(dc, text, mn) + dc->font.height/2 > dc->w; mn--)
		if(mn == 0)
			return;
	memcpy(buf, text, mn);
	if(mn < n)
		for(n = MAX(mn-3, 0); n < mn; buf[n++] = '.');

	drawrect(dc, 0, 0, dc->w, dc->h, True, col->BG);
	drawtextn(dc, buf, mn, col);
}

void
drawtextn(DC *dc, const char *text, size_t n, ColorSet *col) {
	int x = dc->x + dc->font.height/2;
	int y = dc->y + dc->font.ascent+1;

	XSetForeground(dc->dpy, dc->gc, col->FG);
#ifdef PANGO
	if(dc->pango_layout) {
		if (!dc->xftdraw)
			eprintf("error, xft drawable does not exist");
		pango_layout_set_text(dc->pango_layout, text, n);
		pango_xft_render_layout_line(dc->xftdraw, &col->FG_xft,
			pango_layout_get_line_readonly(dc->pango_layout, 0),
			x*PANGO_SCALE, y*PANGO_SCALE);
	}
	else
#endif
	if(dc->font.set)
		XmbDrawString(dc->dpy, dc->canvas, dc->font.set, dc->gc, x, y, text, n);
	else {
		XSetFont(dc->dpy, dc->gc, dc->font.xfont->fid);
		XDrawString(dc->dpy, dc->canvas, dc->gc, x, y, text, n);
	}
}

void
eprintf(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if(fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	}
	exit(EXIT_FAILURE);
}

void
freedc(DC *dc) {
#ifdef PANGO
	if(dc->pango_layout) {
		XftDrawDestroy(dc->xftdraw);
		pango_xft_shutdown_display(dc->dpy, DefaultScreen(dc->dpy));
		g_object_unref(dc->pango_layout);
	}
#endif
	if(dc->font.set)
		XFreeFontSet(dc->dpy, dc->font.set);
	if(dc->font.xfont)
		XFreeFont(dc->dpy, dc->font.xfont);
	if(dc->canvas)
		XFreePixmap(dc->dpy, dc->canvas);
	XFreeGC(dc->dpy, dc->gc);
	XCloseDisplay(dc->dpy);
	free(dc);
}

unsigned long
getcolor(DC *dc, const char *colstr) {
	Colormap cmap = DefaultColormap(dc->dpy, DefaultScreen(dc->dpy));
	XColor color;

	if(!XAllocNamedColor(dc->dpy, cmap, colstr, &color, &color))
		eprintf("cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

ColorSet *
initcolor(DC *dc, const char *foreground, const char *background) {
	ColorSet *col = (ColorSet *)malloc(sizeof(ColorSet));
	if(!col)
		eprintf("error, cannot allocate memory for color set");
	col->BG = getcolor(dc, background);
	col->FG = getcolor(dc, foreground);
#ifdef PANGO
	if(dc->pango_layout)
		if(!XftColorAllocName(dc->dpy,
				DefaultVisual(dc->dpy, DefaultScreen(dc->dpy)),
				DefaultColormap(dc->dpy, DefaultScreen(dc->dpy)),
				foreground, &col->FG_xft))
			eprintf("error, cannot allocate xft font color '%s'\n",
				foreground);
#endif
	return col;
}

DC *
initdc(void) {
	DC *dc;

	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("no locale support\n", stderr);
	if(!(dc = calloc(1, sizeof *dc)))
		eprintf("cannot malloc %u bytes:", sizeof *dc);
	if(!(dc->dpy = XOpenDisplay(NULL)))
		eprintf("cannot open display\n");

	dc->gc = XCreateGC(dc->dpy, DefaultRootWindow(dc->dpy), 0, NULL);
	XSetLineAttributes(dc->dpy, dc->gc, 1, LineSolid, CapButt, JoinMiter);
	return dc;
}

void
initfont(DC *dc, const char *fontstr) {
	char *def, **missing, **names;
	int i, n;
	XFontStruct **xfonts;
#ifdef PANGO
	PangoContext *pango_ctx;
	PangoFontDescription *pango_fdesc;
	PangoFontMetrics *pango_fmetrics;
#endif

	missing = NULL;
	if((dc->font.xfont = XLoadQueryFont(dc->dpy, fontstr))) {
		dc->font.ascent = dc->font.xfont->ascent;
		dc->font.descent = dc->font.xfont->descent;
		dc->font.width   = dc->font.xfont->max_bounds.width;
	}
	else if((dc->font.set = XCreateFontSet(dc->dpy, fontstr, &missing, &n, &def))) {
		n = XFontsOfFontSet(dc->font.set, &xfonts, &names);
		for(i = 0; i < n; i++) {
			dc->font.ascent  = MAX(dc->font.ascent,  xfonts[i]->ascent);
			dc->font.descent = MAX(dc->font.descent, xfonts[i]->descent);
			dc->font.width   = MAX(dc->font.width,   xfonts[i]->max_bounds.width);
		}
	}
	else {
#ifdef PANGO
		if (!(pango_ctx = pango_font_map_create_context(
				pango_xft_get_font_map(dc->dpy,
					DefaultScreen(dc->dpy)))))
			eprintf("error, cannot create pango context\n");
		if (!(dc->pango_layout = pango_layout_new(pango_ctx)))
			eprintf("error, cannot create pango layout\n");

		if (!(pango_fdesc = pango_font_description_from_string(
				fontstr)))
			eprintf("error, cannot create pango font description\n");
		pango_layout_set_font_description(dc->pango_layout,
			pango_fdesc);
		if (!(pango_fmetrics = pango_context_get_metrics(pango_ctx,
				pango_fdesc, NULL)))
			eprintf("error, cannot get pango font metrics\n");
		dc->font.ascent = PANGO_PIXELS(pango_font_metrics_get_ascent(
			pango_fmetrics));
		dc->font.descent = PANGO_PIXELS(pango_font_metrics_get_descent(
			pango_fmetrics));
		dc->font.width = PANGO_PIXELS(pango_font_metrics_get_approximate_char_width(
			pango_fmetrics));

		pango_font_metrics_unref(pango_fmetrics);
		pango_font_description_free(pango_fdesc);
#endif
	}
	if(missing)
		XFreeStringList(missing);
	dc->font.height = dc->font.ascent + dc->font.descent;
	return;
}

void
mapdc(DC *dc, Window win, unsigned int w, unsigned int h) {
	XCopyArea(dc->dpy, dc->canvas, win, dc->gc, 0, 0, w, h, 0, 0);
}

void
resizedc(DC *dc, unsigned int w, unsigned int h) {
	int screen = DefaultScreen(dc->dpy);
	if(dc->canvas)
		XFreePixmap(dc->dpy, dc->canvas);

	dc->w = w;
	dc->h = h;
	dc->canvas = XCreatePixmap(dc->dpy, DefaultRootWindow(dc->dpy), w, h,
	                           DefaultDepth(dc->dpy, screen));
#ifdef PANGO
	if(dc->pango_layout && !(dc->xftdraw)) {
		dc->xftdraw = XftDrawCreate(dc->dpy, dc->canvas,
			DefaultVisual(dc->dpy, screen),
			DefaultColormap(dc->dpy, screen));
		if(!(dc->xftdraw))
			eprintf("error, cannot create xft drawable\n");
	}
#endif
}

int
textnw(DC *dc, const char *text, size_t len) {
#ifdef PANGO
	if(dc->pango_layout) {
		int width;
		pango_layout_set_text(dc->pango_layout, text, len);
		pango_layout_get_pixel_size(dc->pango_layout, &width, NULL);
		return width;
	}
	else
#endif
	if(dc->font.set) {
		XRectangle r;

		XmbTextExtents(dc->font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc->font.xfont, text, len);
}

int
textw(DC *dc, const char *text) {
	return textnw(dc, text, strlen(text)) + dc->font.height;
}
