# Copyright © 2012 Intel Corporation
# Copyright © 2013 LunarG, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

include $(top_srcdir)/src/gallium/Automake.inc

AM_CFLAGS = \
	$(GALLIUM_CFLAGS) \
	$(PTHREAD_CFLAGS) \
	$(LIBDRM_CFLAGS)
AM_CPPFLAGS = \
	-I$(top_srcdir)/src/gallium/drivers \
	-I$(top_srcdir)/src/gallium/winsys \
	-DGALLIUM_RBUG \
	-DGALLIUM_TRACE \
	-DGALLIUM_GALAHAD

ninedir = $(NINE_DRIVER_INSTALL_DIR)
nine_LTLIBRARIES = ilo_nine.la

nodist_EXTRA_ilo_nine_la_SOURCES = dummy.cpp
ilo_nine_la_SOURCES = target.c

ilo_nine_la_LDFLAGS = -module -avoid-version -shared -no-undefined -fvisibility=hidden

ilo_nine_la_LIBADD = \
	$(top_builddir)/src/gallium/auxiliary/libgallium.la \
	$(top_builddir)/src/gallium/state_trackers/nine/libninetracker.la \
	$(top_builddir)/src/gallium/winsys/intel/drm/libintelwinsys.la \
	$(top_builddir)/src/gallium/winsys/sw/wrapper/libwsw.la \
	$(top_builddir)/src/gallium/drivers/galahad/libgalahad.la \
	$(top_builddir)/src/gallium/drivers/trace/libtrace.la \
	$(top_builddir)/src/gallium/drivers/rbug/librbug.la \
	$(top_builddir)/src/gallium/drivers/ilo/libilo.la \
	$(INTEL_LIBS)

if HAVE_MESA_LLVM
AM_CFLAGS += -DGALLIUM_LLVMPIPE
ilo_nine_la_LDFLAGS += $(LLVM_LDFLAGS)
ilo_nine_la_LIBADD += $(LLVM_LIBS)
else
AM_CFLAGS += -DGALLIUM_SOFTPIPE
ilo_nine_la_LIBADD += $(top_builddir)/src/gallium/drivers/softpipe/libsoftpipe.la
endif

# Provide compatibility with scripts for the old Mesa build system for
# a while by putting a link to the driver into /lib of the build tree.
all-local: ilo_nine.la
	$(MKDIR_P) $(top_builddir)/$(LIB_DIR)/gallium
	ln -f .libs/ilo_nine.so $(top_builddir)/$(LIB_DIR)/gallium/ilo_nine.so
	ln -sf ilo_nine.so $(top_builddir)/$(LIB_DIR)/gallium/i965_nine.so
