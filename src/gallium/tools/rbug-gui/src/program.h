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

#ifndef _RBUG_GUI_PROGRAM_H_
#define _RBUG_GUI_PROGRAM_H_

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#include "rbug/rbug.h"

struct program;
struct texture_action_read;
struct shader_action_info;

struct rbug_event
{
	gboolean (*func)(struct rbug_event *, struct rbug_header *, struct program *);
};

enum columns {
	COLUMN_ID = 0,
	COLUMN_TYPE,
	COLUMN_TYPENAME,
	COLUMN_PIXBUF,
	COLUMN_INFO_SHORT, /* used for info feild */
	COLUMN_INFO_LONG,  /* used for statusbar */
};

enum types {
	TYPE_NONE = 0,
	TYPE_SCREEN,
	TYPE_CONTEXT,
	TYPE_TEXTURE,
	TYPE_SHADER,
};

enum ctx_view_id {
	CTX_VIEW_FRAGMENT = 0,
	CTX_VIEW_VERTEX,
	CTX_VIEW_GEOM,
	CTX_VIEW_ZS,
	CTX_VIEW_COLOR0,
	CTX_VIEW_COLOR1,
	CTX_VIEW_COLOR2,
	CTX_VIEW_COLOR3,
	CTX_VIEW_COLOR4,
	CTX_VIEW_COLOR5,
	CTX_VIEW_COLOR6,
	CTX_VIEW_COLOR7,
	CTX_VIEW_TEXTURE0,
	CTX_VIEW_TEXTURE1,
	CTX_VIEW_TEXTURE2,
	CTX_VIEW_TEXTURE3,
	CTX_VIEW_TEXTURE4,
	CTX_VIEW_TEXTURE5,
	CTX_VIEW_TEXTURE6,
	CTX_VIEW_TEXTURE7,
	CTX_VIEW_TEXTURE8,
	CTX_VIEW_TEXTURE9,
	CTX_VIEW_TEXTURE10,
	CTX_VIEW_TEXTURE11,
	CTX_VIEW_TEXTURE12,
	CTX_VIEW_TEXTURE13,
	CTX_VIEW_TEXTURE14,
	CTX_VIEW_TEXTURE15,
	CTX_VIEW_NUM,
};

struct program
{
	struct {
		GtkWidget *window;
		GtkWidget *entry_host;
		GtkWidget *entry_port;
		GtkAdjustment *adjustment_port;

		char *host;
		uint16_t port;
	} ask;

	struct {
		uint32_t width;
		uint32_t height;
		GdkGLConfig *config;
	} draw;

	struct {
		GtkWidget *window;
		GtkWidget *context_view;
		GtkTextView *textview;
		GtkWidget *textview_scrolled;
		GtkTreeView *treeview;
		GtkTreeStore *treestore;
		GtkDrawingArea *draw;
		GtkStatusbar *statusbar;

		/* status bar context id */
		guint sb_id;

		GtkTreeIter top;
	} main;

	struct {
		GtkTreeIter iter;
		enum types type;
		guint64 parent;
		guint64 id;
	} selected;

	struct {
		GtkTreeIter iter;
		enum types type;
		guint64 parent;
		guint64 id;
	} viewed;

	struct {
		GtkWidget *break_before;
		GtkWidget *break_after;
		GtkWidget *step;
		GtkWidget *flush;
		GtkWidget *separator;

		GtkWidget *back;
		GtkWidget *forward;
		GtkWidget *background;
		GtkWidget *alpha;
		GtkWidget *automatic;

		GtkWidget *enable;
		GtkWidget *disable;
		GtkWidget *save;
		GtkWidget *revert;
	} tool;

	struct {
		/* signal ids */
		gulong sid[6 + CTX_VIEW_NUM];

		GObject *ra[CTX_VIEW_NUM];

		enum ctx_view_id view_id;

		struct rbug_event blocked_event;
	} context;

	struct {
		gulong id[8];

		struct shader_action_info *info;
	} shader;

	struct {
		struct texture_action_read *read;

		/* current texture loaded */
		gboolean alpha;
		rbug_texture_t id;
		unsigned width;
		unsigned height;

		gulong tid[3];
		gboolean automatic;
		int back;

		int levels[16];
	} texture;

	struct {
		int socket;
		struct rbug_connection *con;
		GIOChannel *channel;
		gint event;
		GHashTable *hash_event;
		GHashTable *hash_reply;
	} rbug;

	struct {
		GHashTable *hash;
	} icon;
};


/* src/ask.c */
void ask_window_create(struct program *p);
gboolean ask_connect(struct program *p);


/* src/main.c */
void main_window_create(struct program *p);
void main_quit(struct program *p);
gboolean main_find_id(guint64 id, GtkTreeIter *out, struct program *p);
void main_set_viewed(GtkTreeIter *iter, gboolean force_update, struct program *p);
void icon_add(const char *filename, const char *name, struct program *p);
GdkPixbuf* icon_get(const char *name, struct program *p);


/* src/rbug.c */
void rbug_add_reply(struct rbug_event *e, uint32_t serial, struct program *p);
void rbug_add_event(struct rbug_event *e, int16_t op, struct program *p);
void rbug_glib_io_watch(struct program *p);
void rbug_finish_and_emit_events(struct program *p);


/* src/context.c */
void context_unselected(struct program *p);
void context_selected(struct program *p);
void context_init(struct program *p);
void context_list(GtkTreeStore *store,
                  GtkTreeIter *parent,
                  struct program *p);


/* src/texture.c */
void texture_list(GtkTreeStore *store, GtkTreeIter *parent, struct program *p);
void texture_unselected(struct program *p);
void texture_selected(struct program *p);
void texture_unviewed(struct program *p);
void texture_viewed(struct program *p);
void texture_refresh(struct program *p);
void texture_draw(struct program *p);


/* src/shader.c */
void shader_unselected(struct program *p);
void shader_selected(struct program *p);
void shader_unviewed(struct program *p);
void shader_viewed(struct program *p);
void shader_refresh(struct program *p);
void shader_list(GtkTreeStore *store,
                 GtkTreeIter *parent,
                 rbug_context_t ctx,
                 struct program *p);


/* src/draw.c */
void draw_setup(GtkDrawingArea *draw, struct program *p);
gboolean draw_gl_begin(struct program *p);
void draw_gl_end(struct program *p);
void draw_checker(guint width, guint height, struct program *p);
void draw_ortho_top_left(struct program *p);


#endif
