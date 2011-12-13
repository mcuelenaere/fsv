if DEBUG
DEBUG_LIBS = libdebug.a
else
DEBUG_LIBS =
endif

INCLUDES = @GTK_CFLAGS@

noinst_LIBRARIES = $(DEBUG_LIBS)

noinst_HEADERS = debug.h
libdebug_a_SOURCES = debug.c
