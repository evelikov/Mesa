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
#include "pipe/p_format.h"

static gboolean main_idle(gpointer data)
{
	struct program *p = (struct program *)data;

	if (!ask_connect(p))
		main_quit(p);

	return false;
}

int main(int argc, char *argv[])
{
	struct program *p = g_malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));

	gtk_init(&argc, &argv);
	gtk_gl_init(&argc, &argv);

	p->draw.config = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB |
	                                           GDK_GL_MODE_ALPHA |
	                                           GDK_GL_MODE_DEPTH |
	                                           GDK_GL_MODE_DOUBLE);

	/* connect to first non gnome argument */
	if (argc > 1) {
		int len = strlen(argv[1]) + 1;
		p->ask.host = g_malloc(len);
		memcpy(p->ask.host, argv[1], len);
		p->ask.port = 13370;
		gtk_idle_add(main_idle, p);
	} else {
		ask_window_create(p);
	}

	gtk_main();

	g_free(p);

	return 0;
}

static void destroy(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	(void)widget;

	main_quit(p);
}

static void foreach(GtkTreeModel *model,
                    GtkTreePath *path,
                    GtkTreeIter *iter,
                    gpointer data)
{
	struct program *p = (struct program *)data;
	GtkTreeIter parent;
	GValue id;
	GValue type;
	GValue typename;
	(void)path;
	(void)data;

	memset(&id, 0, sizeof(id));
	memset(&type, 0, sizeof(type));
	memset(&typename, 0, sizeof(typename));

	gtk_tree_model_get_value(model, iter, COLUMN_ID, &id);
	gtk_tree_model_get_value(model, iter, COLUMN_TYPE, &type);
	gtk_tree_model_get_value(model, iter, COLUMN_TYPENAME, &typename);

	g_assert(G_VALUE_HOLDS_UINT64(&id));
	g_assert(G_VALUE_HOLDS_INT(&type));
	g_assert(G_VALUE_HOLDS_STRING(&typename));

	p->selected.iter = *iter;
	p->selected.id = g_value_get_uint64(&id);
	p->selected.type = g_value_get_int(&type);

	if (gtk_tree_model_iter_parent(model, &parent, iter)) {
		g_value_unset(&id);
		gtk_tree_model_get_value(model, &parent, COLUMN_ID, &id);
		g_assert(G_VALUE_HOLDS_UINT64(&id));

		p->selected.parent = g_value_get_uint64(&id);
	}
}

static void changed(GtkTreeSelection *s, gpointer data)
{
	struct program *p = (struct program *)data;
	enum types old_type;
	uint64_t old_id;
	(void)s;
	(void)p;

	old_id = p->selected.id;
	old_type = p->selected.type;

	/* reset selected data */
	memset(&p->selected, 0, sizeof(p->selected));

	gtk_tree_selection_selected_foreach(s, foreach, p);

	if (p->selected.id != old_id ||
	    p->selected.type != old_type) {
		if (old_id) {
			if (old_type == TYPE_SHADER)
				shader_unselected(p);
			else if (old_type == TYPE_TEXTURE)
				texture_unselected(p);
			else if (old_type == TYPE_CONTEXT)
				context_unselected(p);
		}
		if (p->selected.id) {
			if (p->selected.type == TYPE_SHADER)
				shader_selected(p);
			else if (p->selected.type == TYPE_TEXTURE)
				texture_selected(p);
			else if (p->selected.type == TYPE_CONTEXT)
				context_selected(p);
		}
	}
}

static void refresh(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	GtkTreeStore *store = p->main.treestore;
	(void)widget;

	if (p->selected.id != 0) {
		if (p->selected.type == TYPE_TEXTURE)
			texture_refresh(p);

		return;
	}

	gtk_tree_store_clear(p->main.treestore);

	gtk_tree_store_insert_with_values(store, &p->main.top, NULL, -1,
	                                  COLUMN_ID, (guint64)0,
	                                  COLUMN_TYPE, TYPE_SCREEN,
	                                  COLUMN_TYPENAME, "screen",
	                                  COLUMN_PIXBUF, icon_get("screen", p),
	                                  -1);

	/* contexts */
	context_list(store, &p->main.top, p);

	/* textures */
	texture_list(store, &p->main.top, p);

	/* expend all rows */
	gtk_tree_view_expand_all(p->main.treeview);
}

static void setup_cols(GtkBuilder *builder, GtkTreeView *view, struct program *p)
{
	GtkTreeViewColumn *col;
	GtkCellRenderer *renderer;
	(void)view;
	(void)p;

	/* column id */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_id"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_ID);

	/* column type */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_type"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_TYPENAME);

	/* column icon */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_icon"));
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "pixbuf", COLUMN_PIXBUF);

	/* column info */
	col = GTK_TREE_VIEW_COLUMN(gtk_builder_get_object(builder, "col_info"));
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", COLUMN_INFO_SHORT);

	g_object_set(G_OBJECT(renderer), "xalign", (gfloat)0.0f, NULL);
}

static void icon_setup(struct program *p)
{
	p->icon.hash = g_hash_table_new(g_str_hash, g_str_equal);

	icon_add("res/rgba.png", "rgba", p);
	icon_add("res/rgbx.png", "rgbx", p);
	icon_add("res/argb.png", "argb", p);
	icon_add("res/xrgb.png", "xrgb", p);

	icon_add("res/s8z24.png", "s8z24", p);
	icon_add("res/x8z24.png", "x8z24", p);
	icon_add("res/z24s8.png", "z24s8", p);
	icon_add("res/z24x8.png", "z24x8", p);

	icon_add("res/rgb.png", "rgb", p);
	icon_add("res/rgb.png", "bgr", p);

	icon_add("res/dxt1_rgb.png", "dxt1_rgb", p);
	icon_add("res/dxt1_rgba.png", "dxt1_rgba", p);
	icon_add("res/dxt3_rgba.png", "dxt3_rgba", p);
	icon_add("res/dxt5_rgba.png", "dxt5_rgba", p);

	icon_add("res/screen.png", "screen", p);

	icon_add("res/shader_on_normal.png", "shader_on_normal", p);
	icon_add("res/shader_on_replaced.png", "shader_on_replaced", p);
	icon_add("res/shader_off_normal.png", "shader_off_normal", p);
	icon_add("res/shader_off_replaced.png", "shader_off_replaced", p);
}

struct find_struct
{
	GtkTreeIter *out;
	guint64 id;
	gboolean result;
};

static gboolean find_foreach(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data)
{
	struct find_struct *find = (struct find_struct *)data;
	guint64 id;
	(void)path;

	gtk_tree_model_get(model, iter,
	                   COLUMN_ID, &id,
	                  -1);

	if (id == find->id) {
		*find->out = *iter;
		find->result = TRUE;
		return TRUE;
	}

	return FALSE;
}

gboolean main_find_id(guint64 id, GtkTreeIter *out, struct program *p)
{
	GtkTreeModel *model = GTK_TREE_MODEL(p->main.treestore);
	struct find_struct find;

	find.out = out;
	find.id = id;
	find.result = FALSE;

	gtk_tree_model_foreach(model, find_foreach, &find);

	return find.result;
}

void main_set_viewed(GtkTreeIter *iter, gboolean force_update, struct program *p)
{
	GtkTreeModel *model = GTK_TREE_MODEL(p->main.treestore);
	GtkTreeIter parent;
	enum types old_type;
	uint64_t old_id;
	GValue typename;
	GValue type;
	GValue id;

	old_id = p->viewed.id;
	old_type = p->viewed.type;

	if (iter) {
		memset(&id, 0, sizeof(id));
		memset(&type, 0, sizeof(type));
		memset(&typename, 0, sizeof(typename));

		gtk_tree_model_get_value(model, iter, COLUMN_ID, &id);
		gtk_tree_model_get_value(model, iter, COLUMN_TYPE, &type);
		gtk_tree_model_get_value(model, iter, COLUMN_TYPENAME, &typename);

		g_assert(G_VALUE_HOLDS_UINT64(&id));
		g_assert(G_VALUE_HOLDS_INT(&type));
		g_assert(G_VALUE_HOLDS_STRING(&typename));

		p->viewed.iter = *iter;
		p->viewed.id = g_value_get_uint64(&id);
		p->viewed.type = g_value_get_int(&type);

		if (gtk_tree_model_iter_parent(model, &parent, iter)) {
			g_value_unset(&id);
			gtk_tree_model_get_value(model, &parent, COLUMN_ID, &id);
			g_assert(G_VALUE_HOLDS_UINT64(&id));
			p->viewed.parent = g_value_get_uint64(&id);
		} else {
			/* Everything else then the screen must have a parent */
			g_assert(p->viewed.id != 0);
		}
	} else {
		memset(&p->viewed, 0, sizeof(p->viewed));
	}

	if (p->viewed.id != old_id ||
	    p->viewed.type != old_type) {
		if (old_id) {
			if (old_type == TYPE_SHADER)
				shader_unviewed(p);
			else if (old_type == TYPE_TEXTURE)
				texture_unviewed(p);
		}

		if (p->viewed.id) {
			if (p->viewed.type == TYPE_SHADER)
				shader_viewed(p);
			else if (p->viewed.type == TYPE_TEXTURE)
				texture_viewed(p);
		}
	} else {
		if (force_update || p->viewed.id) {
			if (p->viewed.type == TYPE_SHADER)
				shader_refresh(p);
			else if (p->viewed.type == TYPE_TEXTURE)
				texture_refresh(p);
		}
	}
}

void main_window_create(struct program *p)
{
	GtkBuilder *builder;

	GtkWidget *window;
	GObject *selection;
	GtkDrawingArea *draw;
	GtkTextView *textview;
	GtkWidget *context_view;
	GtkWidget *textview_scrolled;
	GtkTreeView *treeview;
	GtkTreeStore *treestore;

	GObject *tool_quit;
	GObject *tool_refresh;

	GObject *tool_break_before;
	GObject *tool_break_after;
	GObject *tool_step;
	GObject *tool_flush;
	GObject *tool_separator;

	GObject *tool_back;
	GObject *tool_forward;
	GObject *tool_background;
	GObject *tool_alpha;
	GObject *tool_automatic;

	GObject *tool_disable;
	GObject *tool_enable;
	GObject *tool_save;
	GObject *tool_revert;

	builder = gtk_builder_new();

	gtk_builder_add_from_file(builder, "res/main.xml", NULL);
	gtk_builder_connect_signals(builder, NULL);

	draw = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "draw"));
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	textview = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "textview"));
	treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview"));
	treestore = GTK_TREE_STORE(gtk_builder_get_object(builder, "treestore"));
	selection = G_OBJECT(gtk_tree_view_get_selection(treeview));
	context_view = GTK_WIDGET(gtk_builder_get_object(builder, "context_view"));
	textview_scrolled = GTK_WIDGET(gtk_builder_get_object(builder, "textview_scrolled"));


	tool_quit = gtk_builder_get_object(builder, "tool_quit");
	tool_refresh = gtk_builder_get_object(builder, "tool_refresh");

	tool_break_before = gtk_builder_get_object(builder, "tool_break_before");
	tool_break_after = gtk_builder_get_object(builder, "tool_break_after");
	tool_step = gtk_builder_get_object(builder, "tool_step");
	tool_flush = gtk_builder_get_object(builder, "tool_flush");
	tool_separator = gtk_builder_get_object(builder, "tool_separator");

	tool_back = gtk_builder_get_object(builder, "tool_back");
	tool_forward = gtk_builder_get_object(builder, "tool_forward");
	tool_background = gtk_builder_get_object(builder, "tool_background");
	tool_alpha = gtk_builder_get_object(builder, "tool_alpha");
	tool_automatic = gtk_builder_get_object(builder, "tool_auto");

	tool_disable = gtk_builder_get_object(builder, "tool_disable");
	tool_enable = gtk_builder_get_object(builder, "tool_enable");
	tool_save = gtk_builder_get_object(builder, "tool_save");
	tool_revert = gtk_builder_get_object(builder, "tool_revert");

	setup_cols(builder, treeview, p);

	/* manualy set up signals */
	g_signal_connect(selection, "changed", G_CALLBACK(changed), p);
	g_signal_connect(tool_quit, "clicked", G_CALLBACK(destroy), p);
	g_signal_connect(tool_refresh, "clicked", G_CALLBACK(refresh), p);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), p);

	p->context.ra[CTX_VIEW_FRAGMENT] = gtk_builder_get_object(builder, "ra_ctx_fragment");
	p->context.ra[CTX_VIEW_VERTEX] = gtk_builder_get_object(builder, "ra_ctx_vertex");
	p->context.ra[CTX_VIEW_GEOM] = gtk_builder_get_object(builder, "ra_ctx_geom");
	p->context.ra[CTX_VIEW_ZS] = gtk_builder_get_object(builder, "ra_ctx_zs");
	p->context.ra[CTX_VIEW_COLOR0] = gtk_builder_get_object(builder, "ra_ctx_color0");
	p->context.ra[CTX_VIEW_COLOR1] = gtk_builder_get_object(builder, "ra_ctx_color1");
	p->context.ra[CTX_VIEW_COLOR2] = gtk_builder_get_object(builder, "ra_ctx_color2");
	p->context.ra[CTX_VIEW_COLOR3] = gtk_builder_get_object(builder, "ra_ctx_color3");
	p->context.ra[CTX_VIEW_COLOR4] = gtk_builder_get_object(builder, "ra_ctx_color4");
	p->context.ra[CTX_VIEW_COLOR5] = gtk_builder_get_object(builder, "ra_ctx_color5");
	p->context.ra[CTX_VIEW_COLOR6] = gtk_builder_get_object(builder, "ra_ctx_color6");
	p->context.ra[CTX_VIEW_COLOR7] = gtk_builder_get_object(builder, "ra_ctx_color7");

	p->context.ra[CTX_VIEW_TEXTURE0] = gtk_builder_get_object(builder, "ra_ctx_texture0");
	p->context.ra[CTX_VIEW_TEXTURE1] = gtk_builder_get_object(builder, "ra_ctx_texture1");
	p->context.ra[CTX_VIEW_TEXTURE2] = gtk_builder_get_object(builder, "ra_ctx_texture2");
	p->context.ra[CTX_VIEW_TEXTURE3] = gtk_builder_get_object(builder, "ra_ctx_texture3");
	p->context.ra[CTX_VIEW_TEXTURE4] = gtk_builder_get_object(builder, "ra_ctx_texture4");
	p->context.ra[CTX_VIEW_TEXTURE5] = gtk_builder_get_object(builder, "ra_ctx_texture5");
	p->context.ra[CTX_VIEW_TEXTURE6] = gtk_builder_get_object(builder, "ra_ctx_texture6");
	p->context.ra[CTX_VIEW_TEXTURE7] = gtk_builder_get_object(builder, "ra_ctx_texture7");
	p->context.ra[CTX_VIEW_TEXTURE8] = gtk_builder_get_object(builder, "ra_ctx_texture8");
	p->context.ra[CTX_VIEW_TEXTURE9] = gtk_builder_get_object(builder, "ra_ctx_texture9");
	p->context.ra[CTX_VIEW_TEXTURE10] = gtk_builder_get_object(builder, "ra_ctx_texture10");
	p->context.ra[CTX_VIEW_TEXTURE11] = gtk_builder_get_object(builder, "ra_ctx_texture11");
	p->context.ra[CTX_VIEW_TEXTURE12] = gtk_builder_get_object(builder, "ra_ctx_texture12");
	p->context.ra[CTX_VIEW_TEXTURE13] = gtk_builder_get_object(builder, "ra_ctx_texture13");
	p->context.ra[CTX_VIEW_TEXTURE14] = gtk_builder_get_object(builder, "ra_ctx_texture14");
	p->context.ra[CTX_VIEW_TEXTURE15] = gtk_builder_get_object(builder, "ra_ctx_texture15");

	g_assert(27 < CTX_VIEW_NUM);

	p->main.draw = draw;
	p->main.window = window;
	p->main.textview = textview;
	p->main.treeview = treeview;
	p->main.treestore = treestore;
	p->main.context_view = context_view;
	p->main.textview_scrolled = textview_scrolled;

	p->tool.break_before = GTK_WIDGET(tool_break_before);
	p->tool.break_after = GTK_WIDGET(tool_break_after);
	p->tool.step = GTK_WIDGET(tool_step);
	p->tool.flush = GTK_WIDGET(tool_flush);
	p->tool.separator = GTK_WIDGET(tool_separator);

	p->tool.back = GTK_WIDGET(tool_back);
	p->tool.forward = GTK_WIDGET(tool_forward);
	p->tool.background = GTK_WIDGET(tool_background);
	p->tool.alpha = GTK_WIDGET(tool_alpha);
	p->tool.automatic = GTK_WIDGET(tool_automatic);

	p->tool.disable = GTK_WIDGET(tool_disable);
	p->tool.enable = GTK_WIDGET(tool_enable);
	p->tool.save = GTK_WIDGET(tool_save);
	p->tool.revert = GTK_WIDGET(tool_revert);

	draw_setup(draw, p);

	gtk_widget_hide(p->tool.back);
	gtk_widget_hide(p->tool.forward);
	gtk_widget_hide(p->tool.background);
	gtk_widget_hide(p->tool.alpha);

	gtk_widget_hide(p->tool.disable);
	gtk_widget_hide(p->tool.enable);
	gtk_widget_hide(p->tool.save);
	gtk_widget_hide(p->tool.revert);

	gtk_widget_hide(p->main.textview_scrolled);
	gtk_widget_hide(GTK_WIDGET(p->main.textview));
	gtk_widget_hide(GTK_WIDGET(p->main.draw));

	gtk_widget_show(window);

	icon_setup(p);

	context_init(p);

	/* do a refresh */
	refresh(GTK_WIDGET(tool_refresh), p);
}

void main_quit(struct program *p)
{
	if (p->rbug.con) {
		rbug_disconnect(p->rbug.con);
		g_io_channel_unref(p->rbug.channel);
		g_source_remove(p->rbug.event);
		g_hash_table_unref(p->rbug.hash_event);
		g_hash_table_unref(p->rbug.hash_reply);
	}

	g_free(p->ask.host);

	gtk_main_quit();
}

void icon_add(const char *filename, const char *name, struct program *p)
{
	GdkPixbuf *icon = gdk_pixbuf_new_from_file(filename, NULL);

	if (!icon)
		return;

	g_hash_table_insert(p->icon.hash, (char*)name, icon);
}

GdkPixbuf * icon_get(const char *name, struct program *p)
{
	return (GdkPixbuf *)g_hash_table_lookup(p->icon.hash, name);
}
