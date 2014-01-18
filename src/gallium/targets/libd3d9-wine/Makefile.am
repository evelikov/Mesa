# Copyright © 2012 Intel Corporation
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
	$(VISIBILITY_CFLAGS) \
	-I$(top_srcdir)/include/D3D9 \
	-I$(top_srcdir)/../wine/include \
	-DPIPE_SUBSYSTEM_WINDOWS_USER=1 \
	-DWIN32_LEAN_AND_MEAN=1

# use winegcc and a custom libtool to build/link
CC = $(WINEGCC)
LIBTOOL = $(SHELL) $(srcdir)/autoconf/libtool

lib_LTLIBRARIES = d3d9.dll.la

d3d9_dll_la_SOURCES = d3d9.c debug.c guid.c x11/driver.c x11/present.c
d3d9_dll_la_LDFLAGS = \
    -module \
    -avoid-version \
    -shared \
    -L$(top_srcdir)/lib/gallium \
    -XCClinker d3d9.spec
d3d9_dll_la_LIBADD = -lgdi32 $(NINE_LIBS) -ldl -ld3dadapter9

# Provide compatibility with scripts for the old Mesa build system for
# a while by putting a link to the driver into /lib of the build tree.
all-local: d3d9.dll.la
	$(MKDIR_P) $(top_builddir)/$(LIB_DIR)/gallium
	ln -f .libs/d3d9.dll.so* $(top_builddir)/$(LIB_DIR)/gallium/
