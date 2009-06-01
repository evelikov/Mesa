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

void context_list(GtkTreeStore *store, GtkTreeIter *parent, struct program *p)
{
	struct rbug_proto_context_list_reply *list;
	struct rbug_connection *con = p->rbug.con;
	struct rbug_header *header;
	uint32_t i;

	rbug_send_context_list(con, NULL);
	header = rbug_get_message(con, NULL);
	list = (struct rbug_proto_context_list_reply *)header;

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
