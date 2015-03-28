# Mesa 3-D graphics library
#
# Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
# Copyright (C) 2010-2011 LunarG Inc.
# Copyright (C) 2015 Emil Velikov <emil.l.velikov@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

LOCAL_PATH := $(call my-dir)

# get common_SOURCES, dri2_SOURCES, drisw_SOURCES
include $(LOCAL_PATH)/Makefile.sources

include $(CLEAR_VARS)

intermediates := $(call local-intermediates-dir)

LOCAL_SRC_FILES := \
	$(common_SOURCES)

LOCAL_C_INCLUDES := \
	$(MESA_TOP)/src/mapi \
	$(MESA_TOP)/src/mesa \
	$(MESA_TOP)/src/mesa/drivers/dri/common \
	$(intermediates)/src/mesa/drivers/dri/common

LOCAL_CFLAGS := \
        -DGALLIUM_STATIC_TARGETS=1

# TODO: Needed for dri-kms support
#if HAVE_GALLIUM_SOFTPIPE
#LOCAL_CFLAGS += \
#        -DGALLIUM_SOFTPIPE
#endif # HAVE_GALLIUM_SOFTPIPE

# has swrast
# XXX: Add the conditional
# swrast only
ifeq ($(MESA_GPU_DRIVERS),swrast)
LOCAL_CFLAGS += -D__NOT_HAVE_DRM_H
endif

LOCAL_SRC_FILES += $(drisw_SOURCES)
endif

# has non-swrast
# XXX: Add the conditional
LOCAL_SRC_FILES += $(dri2_SOURCES)
LOCAL_SHARED_LIBRARIES := libdrm
endif

LOCAL_MODULE := libmesa_st_dri

include $(GALLIUM_COMMON_MK)
include $(BUILD_STATIC_LIBRARY)
