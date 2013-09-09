# dmenu version
VERSION = 4.5-tip

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama, comment if you don't want it
XINERAMALIBS  = -lXinerama
XINERAMAFLAGS = -DXINERAMA

# Pango, comment if you don't want it
PANGOINC := $(shell pkg-config --cflags pangoxft)
PANGOLIBS := $(shell pkg-config --libs pangoxft)
PANGOFLAGS := -DPANGO

# includes and libs
INCS = -I${X11INC} ${PANGOINC}
LIBS = -L${X11LIB} -lX11 ${XINERAMALIBS} ${PANGOLIBS}

# flags
CPPFLAGS = -D_BSD_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS} ${PANGOFLAGS}
CFLAGS   = -ansi -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

# compiler and linker
CC = cc
