bin_PROGRAMS = xjackfreak
xjackfreak_SOURCES = xjackfreak1.c
xjackfreak_CPPFLAGS=-Wall -march=native -fno-strict-aliasing -D_FILE_OFFSET_BITS=64 ${AM_CFLAGS}
xjackfreak_LDFLAGS=$(x11_LIBS) $(xext_LIBS) ${jack_LIBS}
EXTRA_DIST = fftlib1.c jack.c jfgraplib1.c load_ppm.c plist.c visualx11.c signals.c
