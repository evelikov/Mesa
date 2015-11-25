# Copyright © 2012 Jon TURNEY
# Copyright (C) 2015 Intel Corporation
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
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

include Makefile.sources

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/mapi \
	-I$(top_srcdir)/src/mesa/ \
	-I$(top_srcdir)/src/gallium/include \
	-I$(top_srcdir)/src/gallium/auxiliary \
	-I$(top_srcdir)/src/gtest/include \
	$(DEFINES)

AM_CFLAGS = \
	$(VISIBILITY_CFLAGS) \
	$(MSVC2013_COMPAT_CFLAGS)

AM_CXXFLAGS = \
	$(VISIBILITY_CXXFLAGS) \
	$(MSVC2013_COMPAT_CXXFLAGS)

noinst_LTLIBRARIES = libnir.la

libnir_la_SOURCES = \
	$(NIR_FILES) \
	$(NIR_GENERATED_FILES)

libnir_la_LIBADD = $(top_builddir)/src/compiler/libcompiler.la


check_PROGRAMS = tests/control_flow_tests
TESTS = $(check_PROGRAMS)

tests_control_flow_tests_SOURCES = tests/control_flow_tests.cpp
tests_control_flow_tests_CFLAGS = $(PTHREAD_CFLAGS)
tests_control_flow_tests_LDADD = \
	libnir.la \
	$(top_builddir)/src/gtest/libgtest.la \
	$(top_builddir)/src/util/libmesautil.la \
	$(PTHREAD_LIBS)


BUILT_SOURCES = $(NIR_GENERATED_FILES)
CLEANFILES = $(BUILT_SOURCES)


MKDIR_GEN = $(AM_V_at)$(MKDIR_P) $(@D)
PYTHON_GEN = $(AM_V_GEN)$(PYTHON2) $(PYTHON_FLAGS)

nir_builder_opcodes.h: nir_opcodes.py nir_builder_opcodes_h.py
	$(MKDIR_GEN)
	$(PYTHON_GEN) $(srcdir)/nir_builder_opcodes_h.py > $@

nir_constant_expressions.c: nir_opcodes.py nir_constant_expressions.py
	$(MKDIR_GEN)
	$(PYTHON_GEN) $(srcdir)/nir_constant_expressions.py > $@

nir_opcodes.h: nir_opcodes.py nir_opcodes_h.py
	$(MKDIR_GEN)
	$(PYTHON_GEN) $(srcdir)/nir_opcodes_h.py > $@

nir_opcodes.c: nir_opcodes.py nir_opcodes_c.py
	$(MKDIR_GEN)
	$(PYTHON_GEN) $(srcdir)/nir_opcodes_c.py > $@

nir_opt_algebraic.c: nir_opt_algebraic.py nir_algebraic.py
	$(MKDIR_GEN)
	$(PYTHON_GEN) $(srcdir)/nir_opt_algebraic.py > $@


EXTRA_DIST = \
	nir_algebraic.py \
	nir_builder_opcodes_h.py \
	nir_constant_expressions.py \
	nir_opcodes.py \
	nir_opcodes_c.py \
	nir_opcodes_h.py \
	nir_opt_algebraic.py \
	tests
