include $(top_srcdir)/src/gallium/Automake.inc

AM_CFLAGS = \
	$(GALLIUM_CFLAGS) \
	-I$(top_srcdir)/include/D3D \
	-I$(top_srcdir)/include/D3D9

noinst_LTLIBRARIES = libninetracker.la

ADAPTER = adapter/drm.c

libninetracker_la_SOURCES = \
    $(ADAPTER) \
    adapter9.c \
    authenticatedchannel9.c \
    basetexture9.c \
    cryptosession9.c \
    cubetexture9.c \
    device9.c \
    device9ex.c \
    device9video.c \
    guid.c \
    indexbuffer9.c \
    iunknown.c \
    nine_debug.c \
    nine_pipe.c \
    nine_state.c \
    nineexoverlayextension.c \
    nine_quirk.c \
    pixelshader9.c \
    query9.c \
    resource9.c \
    stateblock9.c \
    surface9.c \
    swapchain9.c \
    swapchain9ex.c \
    texture9.c \
    vertexbuffer9.c \
    vertexdeclaration9.c \
    vertexshader9.c \
    volume9.c \
    volumetexture9.c
