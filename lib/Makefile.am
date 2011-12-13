noinst_LIBRARIES = libgnomeui.a libmisc.a

INCLUDES = @GTK_CFLAGS@

noinst_HEADERS = \
	gnome-i18nP.h \
	gnome-color-picker.h \
	gnome-dateedit.h \
	getopt.h \
	nvstore.h
libgnomeui_a_SOURCES = gnome-color-picker.c gnome-dateedit.c
libmisc_a_SOURCES = getopt.c getopt1.c nvstore.c
libmisc_a_LIBADD = @LIBOBJS@

EXTRA_DIST = README.gnome
