
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
