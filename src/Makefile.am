bin_PROGRAMS = xjackfreak
xjackfreak_SOURCES = xjackfreak1.c
xjackfreak_CPPFLAGS=-Wall -march=native -O4 -fno-strict-aliasing -D_FILE_OFFSET_BITS=64 -I../includes -I/usr/X11R6/include -I/usr/X11R6/include/X11/fonts -L/usr/X11R6/lib ${AM_CFLAGS}
xjackfreak_LDFLAGS=-lX11 -lXext ${jack_LIBS}
EXTRA_DIST = fftlib1.c jack.c jfgraplib1.c load_ppm.c plist.c visualx11.c signals.c
