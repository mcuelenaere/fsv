# Process this file with autoconf to produce a configure script.
AC_INIT(src/fsv.c)
AM_CONFIG_HEADER(config.h)

PACKAGE=fsv
VERSION=0.9
RELEASE=1

AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AC_SUBST(RELEASE)

AC_DEFINE(_GNU_SOURCE)
AC_DEFINE(GTK_DISABLE_COMPAT_H)

# Programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB

AC_ISC_POSIX

# Header files
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(strings.h sys/time.h unistd.h)

# Typedefs, structures
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPE(comparison_fn_t, , AC_DEFINE_UNQUOTED(comparison_fn_t, int (*)( const void *, const void * )))
AC_STRUCT_ST_BLOCKS
AC_STRUCT_TM

# Library functions
AC_FUNC_ALLOCA
AC_FUNC_FNMATCH
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(getcwd getopt_long gettimeofday mktime scandir strcspn strdup \
	strspn strtod strtoul)
AC_CHECK_LIB([m], [hypot])

# Debugging
AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging])
if test "$enable_debug" = "yes" ; then
	AC_MSG_WARN([debugging enabled, CFLAGS=\"$CFLAGS\"])
	AM_CONDITIONAL(DEBUG, true)
	AC_DEFINE(DEBUG)
	AC_DEFINE(G_ENABLE_DEBUG)
else
	AM_CONDITIONAL(DEBUG, false)
	AC_DEFINE(G_DISABLE_ASSERT)
	AC_DEFINE(G_DISABLE_CHECKS)
	AC_DEFINE(GTK_NO_CHECK_CASTS)
fi

# Internationalization
ALL_LINGUAS=""
AM_GNU_GETTEXT

# Locale directory
AC_ARG_WITH(locale-dir, [  --with-locale-dir=DIR   install locale files in DIR [DATADIR/locale]])
if test "$USE_NLS" = "yes" ; then
	if test -n "$with_locale_dir" ; then
		localedir=$with_locale_dir
	else
		localedir=${datadir}/locale
	fi
else
	localedir="none"
fi
AC_SUBST(localedir)

# Documentation directory
AC_ARG_WITH(doc-dir, [  --with-doc-dir=DIR      install documentation files in DIR [DATADIR/fsv]])
if test -n "$with_doc_dir" ; then
	docdir=$with_doc_dir
else
	docdir=${datadir}/fsv
fi
AC_SUBST(docdir)

# GTK+ libraries
PKG_CHECK_MODULES([GTK], [gtk+-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])

#### OpenGL/Mesa3D libraries ####
AC_ARG_WITH(GL-prefix,  [  --with-GL-prefix=PFX    Prefix where GL/MesaGL is installed (optional)])
AC_ARG_WITH(lib-GL,     [  --with-lib-GL           use '-lGL' (default)])
AC_ARG_WITH(lib-MesaGL, [  --with-lib-MesaGL       use '-lMesaGL'])

if test -n "$with_GL_prefix" ; then
	GL_CFLAGS=-I$with_GL_prefix/include
	GL_LDOPTS=-L$with_GL_prefix/lib
else
	GL_CFLAGS=""
	GL_LDOPTS=""
fi

AC_CHECK_LIB(MesaGL, glBegin, have_MesaGL=yes, , $GTK_LIBS $GL_LDOPTS)
AC_CHECK_HEADER(GL/glu.h, have_GLU="yes", have_GLU="no")
AC_CHECK_LIB(GL,     glBegin, have_GL=yes,     , $GTK_LIBS $GL_LDOPTS)

if test "$have_GLU" = "no" ; then
                AC_MSG_ERROR([Missing OpenGL utility library])
        fi

if test "$with_lib_GL" = "yes" ; then
	# Want to use '-lGL'
	if test "$have_GL" = "yes" ; then
		GL_LIBS="$GL_LDOPTS -lGL -lGLU"
	else
		AC_MSG_ERROR([Missing GL library])
	fi
elif test "$with_lib_MesaGL" = "yes" ; then
	# Want to use '-lMesaGL'
	if test "$have_MesaGL" = "yes" ; then
		GL_LIBS="$GL_LDOPTS -lMesaGL -lMesaGLU"
	else
		AC_MSG_ERROR([Missing Mesa3D library])
	fi
else
	# Use whatever is available, preferably '-lGL'
	if test "$have_GL" = "yes" ; then
		GL_LIBS="$GL_LDOPTS -lGL -lGLU"
	elif test "$have_MesaGL" = "yes" ; then
		GL_LIBS="$GL_LDOPTS -lMesaGL -lMesaGLU"
	else
		AC_MSG_ERROR([You need GL or MesaGL libraries])
	fi
fi
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)
#### end of OpenGL/Mesa3D configuration ####

#### GtkGLArea library ####
PKG_CHECK_MODULES([GTKGL], [gtkgl-2.0])

# Check for 'file' command
AC_PATH_PROG(FILE_CMD, file)
if test -n "$FILE_CMD" ; then
	AC_DEFINE(HAVE_FILE_COMMAND)
	AC_DEFINE_UNQUOTED(FILE_COMMAND, "$FILE_CMD %s")
fi

# That's a wrap!
AC_OUTPUT(fsv.spec
	Makefile
	debug/Makefile
	doc/Makefile
	intl/Makefile
	lib/Makefile
	po/Makefile.in
	src/Makefile)
