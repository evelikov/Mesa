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

#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"



/*
 * Actions
 */


static struct shader_action_info *
shader_start_info_action(rbug_context_t c,
                         rbug_shader_t s,
                         GtkTreeIter *iter,
                         struct program *p);
static void shader_stop_info_action(struct shader_action_info *info, struct program *p);

static void shader_start_list_action(GtkTreeStore *store, GtkTreeIter *parent,
                                     rbug_context_t ctx, struct program *p);


/*
 * Internal
 */


static void disable(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	(void)widget;

	g_assert(p->viewed.type == TYPE_SHADER);

	rbug_finish_and_emit_events(p);
	rbug_send_shader_disable(con, p->viewed.parent, p->viewed.id, true, NULL);
	rbug_finish_and_emit_events(p);

	gtk_widget_hide(p->tool.disable);
	gtk_widget_show(p->tool.enable);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);
}

static void enable(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	(void)widget;

	g_assert(p->viewed.type == TYPE_SHADER);

	rbug_finish_and_emit_events(p);
	rbug_send_shader_disable(con, p->viewed.parent, p->viewed.id, false, NULL);
	rbug_finish_and_emit_events(p);

	gtk_widget_show(p->tool.disable);
	gtk_widget_hide(p->tool.enable);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);
}

static void update_text(struct rbug_proto_shader_info_reply *info, struct program *p)
{
	GtkTextBuffer *buffer;
#define TEXT_SIZE 16*1024
	gchar text[TEXT_SIZE];

	/* just in case */
	g_assert(sizeof(struct tgsi_token) == 4);

	if (info->replaced_len > 0)
		tgsi_dump_str((struct tgsi_token *)info->replaced, 0, text, TEXT_SIZE);
	else
		tgsi_dump_str((struct tgsi_token *)info->original, 0, text, TEXT_SIZE);

	/* just in case */
	text[TEXT_SIZE-1] = 0;
#undef TEXT_SIZE
	buffer = gtk_text_view_get_buffer(p->main.textview);
	gtk_text_buffer_set_text(buffer, text, -1);
}

static void revert(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	(void)widget;

	g_assert(p->viewed.type == TYPE_SHADER);

	rbug_finish_and_emit_events(p);
	rbug_send_shader_replace(con, p->viewed.parent, p->viewed.id, NULL, 0, NULL);
	rbug_finish_and_emit_events(p);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);
}

static void save(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct tgsi_token tokens[300];
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	gboolean result;
	char *text = 0;
	unsigned num;
	(void)widget;

	g_assert(p->viewed.type == TYPE_SHADER);
	g_assert(sizeof(struct tgsi_token) == 4);

	rbug_finish_and_emit_events(p);

	buffer = gtk_text_view_get_buffer(p->main.textview);
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	result = tgsi_text_translate(text, tokens, 300);

	if (!result) {
		goto out;
	}

	num = tgsi_num_tokens(tokens);

	rbug_send_shader_replace(con, p->viewed.parent, p->viewed.id,
	                         (uint32_t*)tokens, num, NULL);

	gtk_widget_show(p->tool.revert);

	rbug_finish_and_emit_events(p);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);

out:
	g_free(text);
}

void shader_refresh(struct program *p)
{
	g_assert(p->viewed.type == TYPE_SHADER);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);
}

void shader_viewed(struct program *p)
{
	g_assert(p->viewed.type == TYPE_SHADER);

	p->shader.id[0] = g_signal_connect(p->tool.save, "clicked", G_CALLBACK(save), p);
	p->shader.id[1] = g_signal_connect(p->tool.revert, "clicked", G_CALLBACK(revert), p);
	p->shader.id[2] = g_signal_connect(p->tool.enable, "clicked", G_CALLBACK(enable), p);
	p->shader.id[3] = g_signal_connect(p->tool.disable, "clicked", G_CALLBACK(disable), p);

	gtk_widget_show(GTK_WIDGET(p->main.textview));
	gtk_widget_show(p->main.textview_scrolled);

	if (p->shader.info)
		shader_stop_info_action(p->shader.info, p);

	shader_start_info_action(p->viewed.parent, p->viewed.id, &p->viewed.iter, p);
}

void shader_unviewed(struct program *p)
{
	g_signal_handler_disconnect(p->tool.save, p->shader.id[0]);
	g_signal_handler_disconnect(p->tool.revert, p->shader.id[1]);
	g_signal_handler_disconnect(p->tool.enable, p->shader.id[2]);
	g_signal_handler_disconnect(p->tool.disable, p->shader.id[3]);

	gtk_widget_hide(p->tool.enable);
	gtk_widget_hide(p->tool.disable);
	gtk_widget_hide(p->tool.save);
	gtk_widget_hide(p->tool.revert);
	gtk_widget_hide(GTK_WIDGET(p->main.textview));
	gtk_widget_hide(p->main.textview_scrolled);
}

void shader_unselected(struct program *p)
{
	main_set_viewed(NULL, FALSE, p);
}

void shader_selected(struct program *p)
{
	main_set_viewed(&p->selected.iter, FALSE, p);
}

void shader_list(GtkTreeStore *store, GtkTreeIter *parent,
                 rbug_context_t ctx, struct program *p)
{
	shader_start_list_action(store, parent, ctx, p);
}


/*
 * Action fuctions
 */

struct shader_action_info
{
	struct rbug_event e;

	rbug_context_t cid;
	rbug_shader_t sid;

	GtkTreeIter iter;

	gboolean running;
	gboolean pending;
};

static void shader_action_info_clean(struct shader_action_info *action, struct program *p)
{
	if (!action)
		return;

	if (p->shader.info == action)
		p->shader.info = NULL;

	g_free(action);
}

static gboolean shader_action_info_info(struct rbug_event *e,
                                        struct rbug_header *header,
                                        struct program *p)
{
	struct rbug_proto_shader_info_reply *info;
	struct shader_action_info *action;
	GdkPixbuf *buf = NULL;

	info = (struct rbug_proto_shader_info_reply *)header;
	action = (struct shader_action_info *)e;

	/* ack pending message */
	action->pending = FALSE;

	g_assert(header->opcode == RBUG_OP_SHADER_INFO_REPLY);

	if (info->disabled) {
		if (info->replaced_len == 0)
			buf = icon_get("shader_off_normal", p);
		else
			buf = icon_get("shader_off_replaced", p);
	} else {
		if (info->replaced_len == 0)
			buf = icon_get("shader_on_normal", p);
		else
			buf = icon_get("shader_on_replaced", p);
	}
	gtk_tree_store_set(p->main.treestore, &action->iter, COLUMN_PIXBUF, buf, -1);

	if (p->viewed.id != action->sid)
		goto out;

	update_text(info, p);

	if (info->disabled) {
		gtk_widget_hide(p->tool.disable);
		gtk_widget_show(p->tool.enable);
	} else {
		gtk_widget_show(p->tool.disable);
		gtk_widget_hide(p->tool.enable);
	}
	gtk_widget_show(p->tool.save);
	if (info->replaced_len > 0)
		gtk_widget_show(p->tool.revert);

out:
	shader_action_info_clean(action, p);
	return FALSE;
}

static struct shader_action_info *
shader_start_info_action(rbug_context_t c,
                         rbug_shader_t s,
                         GtkTreeIter *iter,
                         struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct shader_action_info *action;
	uint32_t serial = 0;

	action = g_malloc(sizeof(*action));
	memset(action, 0, sizeof(*action));

	rbug_send_shader_info(con, c, s, &serial);

	action->e.func = shader_action_info_info;
	action->cid = c;
	action->sid = s;
	action->iter = *iter;
	action->pending = TRUE;
	action->running = TRUE;

	rbug_add_reply(&action->e, serial, p);

	return action;
}

static void shader_stop_info_action(struct shader_action_info *action, struct program *p)
{
	action->running = FALSE;

	if (p->shader.info == action)
		p->shader.info = NULL;

	if (!action->pending)
		shader_action_info_clean(action, p);
}

struct shader_action_list
{
	struct rbug_event e;

	rbug_context_t ctx;

	GtkTreeStore *store;
	GtkTreeIter parent;
};

static gboolean shader_action_list_list(struct rbug_event *e,
                                        struct rbug_header *header,
                                        struct program *p)
{

	struct rbug_proto_shader_list_reply *list;
	struct shader_action_list *action;
	GtkTreeStore *store;
	GtkTreeIter *parent;
	uint32_t i;

	action = (struct shader_action_list *)e;
	list = (struct rbug_proto_shader_list_reply *)header;
	parent = &action->parent;
	store = action->store;

	for (i = 0; i < list->shaders_len; i++) {
		GtkTreeIter iter;
		gtk_tree_store_insert_with_values(store, &iter, parent, -1,
		                                  COLUMN_ID, list->shaders[i],
		                                  COLUMN_TYPE, TYPE_SHADER,
		                                  COLUMN_TYPENAME, "shader",
		                                  -1);

		shader_start_info_action(action->ctx, list->shaders[i], &iter, p);
	}

	g_free(action);

	return FALSE;
}

static void shader_start_list_action(GtkTreeStore *store, GtkTreeIter *parent,
                                     rbug_context_t ctx, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct shader_action_list *action;
	uint32_t serial = 0;

	action = g_malloc(sizeof(*action));
	memset(action, 0, sizeof(*action));

	rbug_send_shader_list(con, ctx, &serial);

	action->e.func = shader_action_list_list;
	action->ctx = ctx;
	action->store = store;
	action->parent = *parent;

	rbug_add_reply(&action->e, serial, p);
}
