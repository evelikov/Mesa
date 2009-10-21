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

#include "pipe/p_defines.h"


/*
 * Actions
 */

struct context_action_info;

static struct context_action_info *
context_start_info_action(rbug_context_t c, GtkTreeIter *iter, gboolean force_update, struct program *p);
static void
context_stop_info_action(struct context_action_info *info, struct program *p);


/*
 * Private
 */


static void ra(GtkWidget *widget, struct program *p)
{
	gboolean active;
	int i;

	for (i = 0; i < CTX_VIEW_NUM; i++)
		if (p->context.ra[i] == G_OBJECT(widget))
			break;

	g_assert(i < CTX_VIEW_NUM);

	active = gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(widget));

	if (!active || (unsigned)i == p->context.view_id)
		return;

	p->context.view_id = i;

	context_start_info_action(p->selected.id, &p->selected.iter, FALSE, p);
}

static void break_before(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	gboolean active;
	(void)widget;

	g_assert(p->selected.type == TYPE_CONTEXT);

	active = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	if (active)
		rbug_send_context_draw_block(con, p->selected.id,
		                             RBUG_BLOCK_BEFORE, NULL);
	else
		rbug_send_context_draw_unblock(con, p->selected.id,
		                               RBUG_BLOCK_BEFORE, NULL);

	context_start_info_action(p->selected.id, &p->selected.iter, FALSE, p);
}

static void break_after(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	gboolean active;
	(void)widget;

	g_assert(p->selected.type == TYPE_CONTEXT);

	active = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	if (active)
		rbug_send_context_draw_block(con, p->selected.id,
		                             RBUG_BLOCK_AFTER, NULL);
	else
		rbug_send_context_draw_unblock(con, p->selected.id,
		                               RBUG_BLOCK_AFTER, NULL);

	context_start_info_action(p->selected.id, &p->selected.iter, FALSE, p);
}

static void step(GtkWidget *widget, struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	(void)widget;

	g_assert(p->selected.type == TYPE_CONTEXT);

	rbug_send_context_draw_step(con, p->selected.id,
	                            RBUG_BLOCK_BEFORE | RBUG_BLOCK_AFTER, NULL);
}

static void flush(GtkWidget *widget, struct program *p)
{
	(void)widget;

	rbug_send_context_flush(p->rbug.con, p->selected.id,
	                        PIPE_FLUSH_TEXTURE_CACHE |
	                        PIPE_FLUSH_RENDER_CACHE, NULL);

	context_start_info_action(p->selected.id, &p->selected.iter, FALSE, p);
}

static gboolean blocked(struct rbug_event *e, struct rbug_header *h, struct program *p)
{
	struct rbug_proto_context_draw_blocked *b = (struct rbug_proto_context_draw_blocked *)h;
	GtkTreeIter iter;
	(void)e;


	rbug_send_context_flush(p->rbug.con, b->context,
	                        PIPE_FLUSH_TEXTURE_CACHE |
	                        PIPE_FLUSH_RENDER_CACHE, NULL);

	if (main_find_id(b->context, &iter, p))
		context_start_info_action(b->context, &iter, TRUE, p);

	(void)context_stop_info_action;

	return TRUE;
}


/*
 * Exported
 */


void context_list(GtkTreeStore *store, GtkTreeIter *parent, struct program *p)
{
	struct rbug_proto_context_list_reply *list;
	struct rbug_connection *con = p->rbug.con;
	struct rbug_header *header;
	uint32_t i;

	rbug_send_context_list(con, NULL);
	header = rbug_get_message(con, NULL);
	list = (struct rbug_proto_context_list_reply *)header;

	g_assert(header);
	g_assert(header->opcode == RBUG_OP_CONTEXT_LIST_REPLY);

	for (i = 0; i < list->contexts_len; i++) {
		GtkTreeIter iter;
		gtk_tree_store_insert_with_values(store, &iter, parent, -1,
		                                  COLUMN_ID, list->contexts[i],
		                                  COLUMN_TYPE, TYPE_CONTEXT,
		                                  COLUMN_TYPENAME, "context",
		                                  -1);

		shader_list(store, &iter, list->contexts[i], p);
	}

	rbug_free_header(header);
}

void context_unselected(struct program *p)
{
	int i;

	gtk_widget_hide(p->main.context_view);
	gtk_widget_hide(p->tool.break_before);
	gtk_widget_hide(p->tool.break_after);
	gtk_widget_hide(p->tool.step);
	gtk_widget_hide(p->tool.flush);
	gtk_widget_hide(p->tool.separator);

	for (i = 0; i < CTX_VIEW_NUM; i++)
		g_signal_handler_disconnect(p->context.ra[i], p->context.sid[i]);
	g_signal_handler_disconnect(p->tool.step, p->context.sid[i++]);
	g_signal_handler_disconnect(p->tool.break_before, p->context.sid[i++]);
	g_signal_handler_disconnect(p->tool.break_after, p->context.sid[i++]);
	g_signal_handler_disconnect(p->tool.flush, p->context.sid[i++]);
}

void context_selected(struct program *p)
{
	g_assert(p->selected.type == TYPE_CONTEXT);
	int i;

	for (i = 0; i < CTX_VIEW_NUM; i++)
		p->context.sid[i] = g_signal_connect(p->context.ra[i], "toggled", G_CALLBACK(ra), p);
	p->context.sid[i++] = g_signal_connect(p->tool.step, "clicked", G_CALLBACK(step), p);
	p->context.sid[i++] = g_signal_connect(p->tool.break_before, "toggled", G_CALLBACK(break_before), p);
	p->context.sid[i++] = g_signal_connect(p->tool.break_after, "toggled", G_CALLBACK(break_after), p);
	p->context.sid[i++] = g_signal_connect(p->tool.flush, "clicked", G_CALLBACK(flush), p);

	gtk_widget_show(p->main.context_view);
	gtk_widget_show(p->tool.break_before);
	gtk_widget_show(p->tool.break_after);
	gtk_widget_show(p->tool.step);
	gtk_widget_show(p->tool.flush);
	gtk_widget_show(p->tool.separator);

	context_start_info_action(p->selected.id, &p->selected.iter, FALSE, p);
}

void context_init(struct program *p)
{
	p->context.blocked_event.func = blocked;

	rbug_add_event(&p->context.blocked_event, RBUG_OP_CONTEXT_DRAW_BLOCKED, p);
}


/*
 * Action fuctions
 */


struct context_action_info
{
	struct rbug_event e;

	rbug_context_t cid;

	GtkTreeIter iter;
	gboolean update;

	gboolean running;
	gboolean pending;
};

static void context_action_info_clean(struct context_action_info *action, struct program *p)
{
	(void)p;

	if (!action)
		return;

	g_free(action);
}

static gboolean context_action_info_info(struct rbug_event *e,
                                        struct rbug_header *header,
                                        struct program *p)
{
	struct rbug_proto_context_info_reply *info;
	struct context_action_info *action;
	GtkTreeIter iter;
	GdkPixbuf *buf = NULL;
	gboolean ret;
	int k;


	info = (struct rbug_proto_context_info_reply *)header;
	action = (struct context_action_info *)e;

	/* ack pending message */
	action->pending = FALSE;

	g_assert(header->opcode == RBUG_OP_CONTEXT_INFO_REPLY);

	if (info->blocker || info->blocked)
		buf = icon_get("shader_off_normal", p);
	else
		buf = icon_get("shader_on_normal", p);

	gtk_tree_store_set(p->main.treestore, &action->iter, COLUMN_PIXBUF, buf, -1);

	/* if this context is not currently selected */
	if (action->cid != p->selected.id)
		goto out;

	if (info->blocker & RBUG_BLOCK_BEFORE)
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.break_before), TRUE);
	else
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.break_before), FALSE);

	if (info->blocker & RBUG_BLOCK_AFTER)
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.break_after), TRUE);
	else
		gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(p->tool.break_after), FALSE);



	/* disable buttons that don't have a valid target */
	gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_FRAGMENT]), (info->fragment));
	gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_VERTEX]), (info->vertex));
	gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_GEOM]), false);

	for (k = 0; k < 8; k++) {
		bool active = (unsigned)k < info->cbufs_len && info->cbufs[k];
		gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_COLOR0+k]), active);
	}
	gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_ZS]), (info->zsbuf));

	for (k = 0; k < 16; k++) {
		bool active = (unsigned)k < info->texs_len && info->texs[k];
		gtk_action_set_sensitive(GTK_ACTION(p->context.ra[CTX_VIEW_TEXTURE0+k]), active);
	}

	/* find the proper view */
	ret = FALSE;
	switch (p->context.view_id) {
	case CTX_VIEW_FRAGMENT:
		ret = main_find_id(info->fragment, &iter, p);
		break;
	case CTX_VIEW_VERTEX:
		ret = main_find_id(info->vertex, &iter, p);
		break;
	case CTX_VIEW_COLOR0:
	case CTX_VIEW_COLOR1:
	case CTX_VIEW_COLOR2:
	case CTX_VIEW_COLOR3:
	case CTX_VIEW_COLOR4:
	case CTX_VIEW_COLOR5:
	case CTX_VIEW_COLOR6:
	case CTX_VIEW_COLOR7:
		{
			unsigned i = p->context.view_id - CTX_VIEW_COLOR0;
			if (info->cbufs_len < i + 1)
				break;
			if (info->cbufs[i] == 0)
				break;
			ret = main_find_id(info->cbufs[i], &iter, p);
			break;
		}
	case CTX_VIEW_ZS:
		if (info->zsbuf == 0)
			break;
		ret = main_find_id(info->zsbuf, &iter, p);
		break;
	case CTX_VIEW_TEXTURE0:
	case CTX_VIEW_TEXTURE1:
	case CTX_VIEW_TEXTURE2:
	case CTX_VIEW_TEXTURE3:
	case CTX_VIEW_TEXTURE4:
	case CTX_VIEW_TEXTURE5:
	case CTX_VIEW_TEXTURE6:
	case CTX_VIEW_TEXTURE7:
	case CTX_VIEW_TEXTURE8:
	case CTX_VIEW_TEXTURE9:
	case CTX_VIEW_TEXTURE10:
	case CTX_VIEW_TEXTURE11:
	case CTX_VIEW_TEXTURE12:
	case CTX_VIEW_TEXTURE13:
	case CTX_VIEW_TEXTURE14:
	case CTX_VIEW_TEXTURE15:
		{
			unsigned i = p->context.view_id - CTX_VIEW_TEXTURE0;
			if (info->texs_len < i + 1)
				break;
			if (info->texs[i] == 0)
				break;
			ret = main_find_id(info->texs[i], &iter, p);
			break;
		}
	default:
		break;
	}

	if (ret)
		main_set_viewed(&iter, action->update, p);
	else
		main_set_viewed(NULL, FALSE, p);

out:
	context_action_info_clean(action, p);
	return FALSE;
}

static struct context_action_info *
context_start_info_action(rbug_context_t c,
                          GtkTreeIter *iter,
                          gboolean force_update,
                          struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct context_action_info *action;
	uint32_t serial = 0;

	action = g_malloc(sizeof(*action));
	memset(action, 0, sizeof(*action));

	rbug_send_context_info(con, c, &serial);

	action->e.func = context_action_info_info;
	action->cid = c;
	action->iter = *iter;
	action->pending = TRUE;
	action->running = TRUE;
	action->update = force_update;

	rbug_add_reply(&action->e, serial, p);

	return action;
}

static void context_stop_info_action(struct context_action_info *action, struct program *p)
{
	if (!action)
		return;

	action->running = FALSE;

	if (!action->pending)
		context_action_info_clean(action, p);
}
