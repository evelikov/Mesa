include $(top_srcdir)/src/gallium/Automake.inc

AM_CFLAGS = \
	-I$(srcdir)/src \
	$(GALLIUM_CFLAGS) \
	$(VISIBILITY_CFLAGS) \
	$(GTK_CFLAGS)

noinst_PROGRAMS = rbug-gui

rbug_gui_SOURCES = \
	src/ask.c \
	src/context.c \
	src/draw.c \
	src/main.c \
	src/program.h \
	src/rbug.c \
	src/shader.c \
	src/texture.c

rbug_gui_LDADD = \
	$(top_builddir)/src/gallium/auxiliary/libgallium.la \
	$(top_builddir)/src/util/libmesautil.la \
	$(GALLIUM_COMMON_LIB_DEPS) \
	$(GTK_LIBS)

EXTRA_DIST = README todo.txt res
