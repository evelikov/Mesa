include $(top_srcdir)/src/gallium/Automake.inc

PIPE_SRC_DIR = $(top_builddir)/src/gallium/targets/pipe-loader

AM_CFLAGS = \
	$(GALLIUM_CFLAGS)

LDADD = \
	$(top_builddir)/src/gallium/auxiliary/pipe-loader/libpipe_loader_dynamic.la \
	$(top_builddir)/src/gallium/auxiliary/libgallium.la \
	$(top_builddir)/src/util/libmesautil.la \
	$(GALLIUM_COMMON_LIB_DEPS)

TESTS = compute tri quad-tex
check_PROGRAMS = compute tri quad-tex

TESTS_ENVIRONMENT= \
	export GALLIUM_PIPE_DRIVERS_PATH=\"$(PIPE_SRC_DIR)/.libs\"

compute_SOURCES = compute.c

tri_SOURCES = tri.c

quad_tex_SOURCES = quad-tex.c

clean-local:
	-rm -f result.bmp
