/*
 * Copyright 2009 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * VMWARE AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "program.h"

#include "GL/gl.h"

#include "pipe/p_format.h"
#include "util/u_format.h"

/* needed for u_tile */
#include "pipe/p_state.h"
#include "util/u_tile.h"

enum {
	BACK_MIN = 0,
	BACK_CHECKER = 0,
	BACK_BLACK,
	BACK_WHITE,
	BACK_MAX,
};


/*
 * Actions
 */


static void texture_stop_read_action(struct texture_action_read *action,
                                     struct program *p);
static void texture_start_if_new_read_action(rbug_texture_t t,
                                             GtkTreeIter *iter,
                                             struct program *p);
static struct texture_action_read *
texture_start_read_action(rbug_texture_t t,
                          GtkTreeIter *iter,
                          struct program *p);

static void texture_start_list_action(GtkTreeStore *store,
                                      GtkTreeIter *parent,
                                      struct program *p);


/*
 * Private
 */


static void alpha(GtkWidget *widget, struct program *p)
{
	(void)widget;

	p->texture.alpha = !p->texture.alpha;

	gtk_widget_queue_draw(GTK_WIDGET(p->main.draw));
}

static void automatic(GtkWidget *widget, struct program *p)
{
	(void)widget;

	p->texture.automatic = !p->texture.automatic;

	if (p->texture.automatic)
		texture_start_if_new_read_action(p->viewed.id, &p->viewed.iter, p);
}

static void background(GtkWidget *widget, struct program *p)
{
	(void)widget;
	(void)p;

	if (++p->texture.back >= BACK_MAX)
		p->texture.back = BACK_MIN;

	gtk_widget_queue_draw(GTK_WIDGET(p->main.draw));
}


/*
 * Exported
 */


void texture_list(GtkTreeStore *store, GtkTreeIter *parent, struct program *p)
{
	texture_start_list_action(store, parent, p);
}

void texture_refresh(struct program *p)
{
	texture_start_if_new_read_action(p->viewed.id, &p->viewed.iter, p);
}

void texture_draw(struct program *p)
{
	uint32_t w, h;

	draw_ortho_top_left(p);

	switch (p->texture.back) {
	case BACK_CHECKER:
		draw_checker(60, 60, p);
		break;
	case BACK_BLACK:
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		break;
	case BACK_WHITE:
		glClearColor(1.0, 1.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		break;
	default:
		break;
	}

	if (p->texture.id != p->viewed.id)
		return;

	w = p->texture.width;
	h = p->texture.height;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (p->texture.alpha)
		glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);

	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_QUADS);
	glTexCoord2f(     0,      1);
	glVertex2f  (10    , 10 + h);
	glTexCoord2f(     1,      1);
	glVertex2f  (10 + w, 10 + h);
	glTexCoord2f(     1,      0);
	glVertex2f  (10 + w, 10    );
	glTexCoord2f(     0,      0);
	glVertex2f  (10    , 10    );
	glEnd();
	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	if (p->texture.automatic)
		texture_start_if_new_read_action(p->viewed.id, &p->viewed.iter, p);
}

void texture_unviewed(struct program *p)
{
	(void)p;

	gtk_widget_hide(p->tool.alpha);
	gtk_widget_hide(p->tool.automatic);
	gtk_widget_hide(p->tool.background);
	gtk_widget_hide(GTK_WIDGET(p->main.draw));

	p->texture.automatic = FALSE;
	p->texture.back = BACK_CHECKER;

	g_signal_handler_disconnect(p->tool.alpha, p->texture.tid[0]);
	g_signal_handler_disconnect(p->tool.automatic, p->texture.tid[1]);
	g_signal_handler_disconnect(p->tool.background, p->texture.tid[2]);
}

void texture_viewed(struct program *p)
{
	g_assert(p->viewed.type == TYPE_TEXTURE);

	texture_start_if_new_read_action(p->viewed.id, &p->viewed.iter, p);

	gtk_widget_show(p->tool.alpha);
	gtk_widget_show(p->tool.automatic);
	gtk_widget_show(p->tool.background);
	gtk_widget_show(GTK_WIDGET(p->main.draw));

	p->texture.automatic = FALSE;
	p->texture.back = BACK_CHECKER;
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.alpha), p->texture.alpha);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.automatic), FALSE);

	p->texture.tid[0] = g_signal_connect(p->tool.alpha, "clicked", G_CALLBACK(alpha), p);
	p->texture.tid[1] = g_signal_connect(p->tool.automatic, "clicked", G_CALLBACK(automatic), p);
	p->texture.tid[2] = g_signal_connect(p->tool.background, "clicked", G_CALLBACK(background), p);
}

void texture_unselected(struct program *p)
{
	main_set_viewed(NULL, FALSE, p);
}

void texture_selected(struct program *p)
{
	main_set_viewed(&p->selected.iter, FALSE, p);
}

/*
 * Actions
 */

struct texture_action_read
{
	struct rbug_event e;

	rbug_texture_t id;

	GtkTreeIter iter;

	gboolean running;
	gboolean pending;

	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned size;
	enum pipe_format format;
	void *data;
};

static void texture_action_read_clean(struct texture_action_read *action,
                                      struct program *p)
{
	if (!action)
		return;

	if (p->texture.read == action)
		p->texture.read = NULL;

	g_free(action->data);
	g_free(action);
}

static void texture_action_read_upload(struct texture_action_read *action,
                                       struct program *p)
{
#if 0
	GLint format, internal_format, type;
	unsigned needed_stride;
	uint32_t w, h, i;

	if (!action)
		return;

	w = action->width;
	h = action->height;

	if (action->block.size != 4)
		return;

	if (pf_has_alpha(action->format)) {
		internal_format = 4;
		format = GL_BGRA;
		type = GL_UNSIGNED_INT_8_8_8_8_REV;
	} else {
		internal_format = 3;
		format = GL_BGR;
		type = GL_UNSIGNED_INT_8_8_8_8_REV;
	}

	needed_stride = pf_get_nblocksx(&action->block, w) * action->block.size;

	if (action->stride % action->block.size)
		g_print("warning stride not mupltiple of block.size\n");
	if (w % action->block.width)
		g_print("warning width not multiple of block.width\n");
	if (h % action->block.height)
		g_print("warning height not multiple of block.height\n");

	/* bad stride */
	if (action->stride > needed_stride) {
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
		             w, h, 0,
		             format, type, NULL);

		for (i = 0; i < h; i += action->block.height) {
			glTexSubImage2D(GL_TEXTURE_2D, 0,
			                0, i, w, action->block.height,
			                format, type, action->data + action->stride * i);
		}
	} else if (action->stride == needed_stride) {
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
		             w, h, 0,
		             format, type, action->data);
	} else {
		g_assert(0);
	}
#endif
	GLint internal_format;
	uint32_t w, h;
	uint32_t src_stride;
	void *data;

	if (!action)
		return;

	data = action->data;
	w = action->width;
	h = action->height;

	src_stride = action->stride;

	if (!data)
		return;

	if (!util_format_is_s3tc(action->format)) {
		uint32_t dst_stride = 4 * 4 * w;
		uint32_t step_h = util_format_description(action->format)->block.height;
		float *rgba = g_malloc(dst_stride * h);
		GLint format, type;
		unsigned i;


		for (i = 0; i < h; i += step_h) {
			pipe_tile_raw_to_rgba(action->format, data + src_stride * i,
			                      w, step_h,
			                      &rgba[w * 4 * i], dst_stride);
		}

		internal_format = 4;
		format = GL_RGBA;
		type = GL_FLOAT;

		glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
		             w, h, 0,
		             format, type, rgba);

		g_free(rgba);
	} else if (util_format_is_s3tc(action->format)) {

		if (action->format == PIPE_FORMAT_DXT1_RGB)
			internal_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		else if (action->format == PIPE_FORMAT_DXT1_RGBA)
			internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		else if (action->format == PIPE_FORMAT_DXT3_RGBA)
			internal_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		else if (action->format == PIPE_FORMAT_DXT5_RGBA)
			internal_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		else
			g_assert(0);

		glCompressedTexImage2D(GL_TEXTURE_2D, 0, internal_format,
		                       w, h, 0,
		                       action->size, data);
	}

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	p->texture.id = action->id;
	p->texture.width = action->width;
	p->texture.height = action->height;
}

static gboolean texture_action_read_read(struct rbug_event *e,
                                         struct rbug_header *header,
                                         struct program *p)
{
	struct rbug_proto_texture_read_reply *read;
	struct texture_action_read *action;
	size_t size;

	read = (struct rbug_proto_texture_read_reply *)header;
	action = (struct texture_action_read *)e;

	/* ack pending message */
	action->pending = FALSE;

	if (header->opcode != RBUG_OP_TEXTURE_READ_REPLY) {
		g_print("warning failed to read from texture\n");
		goto error;
	}

	/* no longer interested in this action */
	if (!action->running)
		goto error;

	if (util_format_is_s3tc(action->format)) {
		size = read->data_len;
	} else {
		/* calculate needed size */
		size = util_format_get_nblocksx(action->format, action->height) * read->stride;

		if (read->data_len < size)
			goto error;
	}

	action->stride = read->stride;
	action->data = g_malloc(size);
	action->size = size;
	memcpy(action->data, read->data, size);

	if (draw_gl_begin(p)) {
		texture_action_read_upload(action, p);
		draw_gl_end(p);
		gtk_widget_queue_draw(GTK_WIDGET(p->main.draw));

		texture_action_read_clean(action, p);
	} else {
		g_assert(0);
		texture_action_read_clean(action, p);
	}

	return FALSE;

error:
	texture_stop_read_action(action, p);

	return FALSE;
}

static gboolean texture_action_read_info(struct rbug_event *e,
                                         struct rbug_header *header,
                                         struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct rbug_proto_texture_info_reply *info;
	struct texture_action_read *action;
	uint32_t serial = 0;
	char info_short_string[64];
	char info_long_string[128];
	GdkPixbuf *buf = NULL;
	const struct util_format_description *format_description;

	info = (struct rbug_proto_texture_info_reply *)header;
	action = (struct texture_action_read *)e;

	/* ack pending message */
	action->pending = FALSE;

	if (header->opcode != RBUG_OP_TEXTURE_INFO_REPLY) {
		g_print("warning failed to get info from texture\n");
		goto error;
	}


	format_description = util_format_description(info->format);
	switch (info->format) {
	case PIPE_FORMAT_NONE: break;
	case PIPE_FORMAT_B8G8R8A8_UNORM:	buf = icon_get("bgra", p); break;
	case PIPE_FORMAT_B8G8R8X8_UNORM:	buf = icon_get("bgrx", p); break;
	case PIPE_FORMAT_A8R8G8B8_UNORM:	buf = icon_get("argb", p); break;
	case PIPE_FORMAT_X8R8G8B8_UNORM:	buf = icon_get("xrgb", p); break;
	case PIPE_FORMAT_B5G5R5A1_UNORM:	buf = icon_get("bgra", p); break;
	case PIPE_FORMAT_B4G4R4A4_UNORM:	buf = icon_get("bgra", p); break;
	case PIPE_FORMAT_B5G6R5_UNORM:		buf = icon_get("bgr", p); break;
	case PIPE_FORMAT_R10G10B10A2_UNORM:	buf = icon_get("argb", p); break;
	case PIPE_FORMAT_L8_UNORM:		buf = icon_get("l", p); break;
	case PIPE_FORMAT_A8_UNORM:		buf = icon_get("a", p); break;
	case PIPE_FORMAT_I8_UNORM:		buf = icon_get("i", p); break;
	case PIPE_FORMAT_L8A8_UNORM:		buf = icon_get("al", p); break; /* TODO invert */
	case PIPE_FORMAT_L16_UNORM:		buf = icon_get("l", p); break;
	case PIPE_FORMAT_UYVY:			buf = icon_get("ycbr", p); break;
	case PIPE_FORMAT_YUYV:			buf = icon_get("ycbr_rev", p); break;
	case PIPE_FORMAT_Z16_UNORM:		buf = icon_get("z", p); break;
	case PIPE_FORMAT_Z32_UNORM:		buf = icon_get("z", p); break;
	case PIPE_FORMAT_Z32_FLOAT:		buf = icon_get("z", p); break;
	case PIPE_FORMAT_Z24_UNORM_S8_USCALED:	buf = icon_get("s8z24", p); break;
	case PIPE_FORMAT_S8_USCALED_Z24_UNORM:	buf = icon_get("z24s8", p); break;
	case PIPE_FORMAT_Z24X8_UNORM:		buf = icon_get("x8z24", p); break;
	case PIPE_FORMAT_X8Z24_UNORM:		buf = icon_get("z24x8", p); break;
	case PIPE_FORMAT_S8_USCALED:		buf = icon_get("s", p); break;
	case PIPE_FORMAT_R64_FLOAT:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R64G64_FLOAT:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R64G64B64_FLOAT:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R64G64B64A64_FLOAT:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_FLOAT:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_FLOAT:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_FLOAT:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_FLOAT:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_UNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_UNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_UNORM:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_UNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_USCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_USCALED:	buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_USCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_USCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_SNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_SNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_SNORM:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_SNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_SSCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_SSCALED:	buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_SSCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_SSCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R16_UNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R16G16_UNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R16G16B16_UNORM:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R16G16B16A16_UNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R16_USCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R16G16_USCALED:	buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R16G16B16_USCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R16G16B16A16_USCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R16_SNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R16G16_SNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R16G16B16_SNORM:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R16G16B16A16_SNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R16_SSCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R16G16_SSCALED:	buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R16G16B16_SSCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R16G16B16A16_SSCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R8_UNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R8G8_UNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R8G8B8_UNORM:		buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R8G8B8A8_UNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_X8B8G8R8_UNORM:	buf = icon_get("xbgr", p); break;
	case PIPE_FORMAT_R8_USCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R8G8_USCALED:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R8G8B8_USCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R8G8B8A8_USCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R8_SNORM:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R8G8_SNORM:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R8G8B8_SNORM:		buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R8G8B8A8_SNORM:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R8_SSCALED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R8G8_SSCALED:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R8G8B8_SSCALED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R8G8B8A8_SSCALED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R32_FIXED:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R32G32_FIXED:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R32G32B32_FIXED:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R32G32B32A32_FIXED:	buf = icon_get("rgba", p); break;
	case PIPE_FORMAT_R16_FLOAT:		buf = icon_get("r", p); break;
	case PIPE_FORMAT_R16G16_FLOAT:		buf = icon_get("rg", p); break;
	case PIPE_FORMAT_R16G16B16_FLOAT:	buf = icon_get("rgb", p); break;
	case PIPE_FORMAT_R16G16B16A16_FLOAT:	buf = icon_get("rgba", p); break;

	/* sRGB formats */
	case PIPE_FORMAT_L8_SRGB:
	case PIPE_FORMAT_L8A8_SRGB:		buf = icon_get("l", p); break;
	case PIPE_FORMAT_R8G8B8_SRGB:		buf = icon_get("al", p); break; /* TODO fix */
	case PIPE_FORMAT_A8B8G8R8_SRGB:		buf = icon_get("abgr", p); break;
	case PIPE_FORMAT_X8B8G8R8_SRGB:		buf = icon_get("xbgr", p); break;
	case PIPE_FORMAT_B8G8R8A8_SRGB:		buf = icon_get("bgra", p); break;
	case PIPE_FORMAT_B8G8R8X8_SRGB:		buf = icon_get("bgrx", p); break;
	case PIPE_FORMAT_A8R8G8B8_SRGB:		buf = icon_get("argb", p); break;
	case PIPE_FORMAT_X8R8G8B8_SRGB:		buf = icon_get("xrgb", p); break;
	case PIPE_FORMAT_R8G8B8A8_SRGB:		buf = icon_get("rgba", p); break;

	/* compressed formats */
	case PIPE_FORMAT_DXT1_RGB:		buf = icon_get("dxt1_rgb", p); break;
	case PIPE_FORMAT_DXT1_RGBA:		buf = icon_get("dxt1_rgba", p); break;
	case PIPE_FORMAT_DXT3_RGBA:		buf = icon_get("dxt3_rgba", p); break;
	case PIPE_FORMAT_DXT5_RGBA:		buf = icon_get("dxt5_rgba", p); break;

	/* sRGB, compressed */
	case PIPE_FORMAT_DXT1_SRGB:		buf = icon_get("dxt1_rgb", p); break;
	case PIPE_FORMAT_DXT1_SRGBA:		buf = icon_get("dxt1_rgba", p); break;
	case PIPE_FORMAT_DXT3_SRGBA:		buf = icon_get("dxt3_rgba", p); break;
	case PIPE_FORMAT_DXT5_SRGBA:		buf = icon_get("dxt5_rgba", p); break;

	/* rgtc compressed */
	case PIPE_FORMAT_RGTC1_UNORM: break;
	case PIPE_FORMAT_RGTC1_SNORM: break;
	case PIPE_FORMAT_RGTC2_UNORM: break;
	case PIPE_FORMAT_RGTC2_SNORM: break;

	case PIPE_FORMAT_R8G8_B8G8_UNORM: break;
	case PIPE_FORMAT_G8R8_G8B8_UNORM: break;

	/* mixed formats */
	case PIPE_FORMAT_R8SG8SB8UX8U_NORM: break;
	case PIPE_FORMAT_R5SG5SB6U_NORM: break;

	/* TODO: re-order these */
	case PIPE_FORMAT_A8B8G8R8_UNORM: break;
	case PIPE_FORMAT_B5G5R5X1_UNORM: break;
	case PIPE_FORMAT_R10G10B10A2_USCALED: break;
	case PIPE_FORMAT_R11G11B10_FLOAT: break;
	case PIPE_FORMAT_R9G9B9E5_FLOAT: break;
	case PIPE_FORMAT_Z32_FLOAT_S8X24_USCALED: break;
	case PIPE_FORMAT_R1_UNORM: break;
	case PIPE_FORMAT_R10G10B10X2_USCALED: break;
	case PIPE_FORMAT_R10G10B10X2_SNORM: break;
	case PIPE_FORMAT_L4A4_UNORM: break;
	case PIPE_FORMAT_B10G10R10A2_UNORM: break;
	case PIPE_FORMAT_R10SG10SB10SA2U_NORM: break;
	case PIPE_FORMAT_R8G8Bx_SNORM: break;
	case PIPE_FORMAT_R8G8B8X8_UNORM: break;
	case PIPE_FORMAT_B4G4R4X4_UNORM: break;
	}

	snprintf(info_short_string, 64, "%ux%ux%u", info->width[0], info->height[0], info->depth[0]);
	snprintf(info_long_string, 128, "%s (%ux%ux%u) %u", util_format_name(info->format), info->width[0], info->height[0], info->depth[0], info->last_level);

	gtk_tree_store_set(p->main.treestore, &action->iter,
	                   COLUMN_PIXBUF, buf,
	                   COLUMN_INFO_SHORT, info_short_string,
	                   COLUMN_INFO_LONG, info_long_string, -1);

	/* no longer interested in this action */
	if (!action->running || p->texture.read != action)
		goto error;

	action->width = info->width[0];
	action->height = info->height[0];
	action->format = info->format;

	rbug_send_texture_read(con, action->id,
	                       0, 0, 0,
	                       0, 0, action->width, action->height,
	                       &serial);
	/* new message pending */
	action->pending = TRUE;

	/* hock up event callback */
	action->e.func = texture_action_read_read;
	rbug_add_reply(&action->e, serial, p);

	return FALSE;

error:
	texture_stop_read_action(action, p);

	return FALSE;
}

static void texture_stop_read_action(struct texture_action_read *action, struct program *p)
{
	if (p->texture.read == action)
		p->texture.read = NULL;

	action->running = FALSE;

	/* no actions pending that will call into this action clean it */
	if (!action->pending)
		texture_action_read_clean(action, p);
}

static void texture_start_if_new_read_action(rbug_texture_t t,
                                             GtkTreeIter *iter,
                                             struct program *p)
{
	/* are we currently trying download anything? */
	if (p->texture.read) {
		/* ok we are downloading something, but is it the one we want? */
		if (p->texture.read->id == p->viewed.id) {
			/* don't need to do anything */
			return;
		} else {
			texture_stop_read_action(p->texture.read, p);
		}
	}

	p->texture.read = texture_start_read_action(t, iter, p);
}

static struct texture_action_read *
texture_start_read_action(rbug_texture_t t, GtkTreeIter *iter, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct texture_action_read *action;
	uint32_t serial = 0;

	action = g_malloc(sizeof(*action));
	memset(action, 0, sizeof(*action));

	rbug_send_texture_info(con, t, &serial);

	action->e.func = texture_action_read_info;
	action->id = t;
	action->iter = *iter;
	action->pending = TRUE;
	action->running = TRUE;

	rbug_add_reply(&action->e, serial, p);

	return action;
}

struct texture_action_list
{
	struct rbug_event e;

	GtkTreeStore *store;
	GtkTreeIter parent;
};

static gboolean texture_action_list_list(struct rbug_event *e,
                                         struct rbug_header *header,
                                         struct program *p)
{

	struct rbug_proto_texture_list_reply *list;
	struct texture_action_list *action;
	GtkTreeStore *store;
	GtkTreeIter *parent;
	uint32_t i;

	action = (struct texture_action_list *)e;
	list = (struct rbug_proto_texture_list_reply *)header;
	parent = &action->parent;
	store = action->store;

	for (i = 0; i < list->textures_len; i++) {
		GtkTreeIter iter;
		gtk_tree_store_insert_with_values(store, &iter, parent, -1,
		                                  COLUMN_ID, list->textures[i],
		                                  COLUMN_TYPE, TYPE_TEXTURE,
		                                  COLUMN_TYPENAME, "texture",
		                                  COLUMN_INFO_SHORT, "(?x?x?) ?",
		                                  COLUMN_INFO_LONG, "PIPE_FORMAT_UNKNOWN (?x?x?) ?",
		                                  -1);
#if 1
		texture_start_read_action(list->textures[i], &iter, p);
#else
		(void)p;
#endif
	}

	g_free(action);

	return FALSE;
}

static void texture_start_list_action(GtkTreeStore *store, GtkTreeIter *parent, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct texture_action_list *action;
	uint32_t serial = 0;

	action = g_malloc(sizeof(*action));
	memset(action, 0, sizeof(*action));

	rbug_send_texture_list(con, &serial);

	action->e.func = texture_action_list_list;
	action->store = store;
	action->parent = *parent;

	rbug_add_reply(&action->e, serial, p);
}
