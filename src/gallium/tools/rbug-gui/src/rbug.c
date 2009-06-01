
#include "program.h"

#define SERIAL2KEY(s) ((void*)(unsigned long)s)
#define KEY2SERIAL(k) ((uint32_t)(unsigned long)k)

static guint hash_func(gconstpointer key)
{
	return KEY2SERIAL(key);
}

static gboolean equal_func(gconstpointer a, gconstpointer b)
{
	return KEY2SERIAL(a) == KEY2SERIAL(b);
}

static void rbug_handle_header(struct rbug_header *header, struct program *p)
{
	struct rbug_event *e;
	uint32_t serial;
	void *ptr;

	g_assert(header->opcode < 0);
	serial = *(uint32_t*)&header[1];

	ptr = g_hash_table_lookup(p->rbug.hash, SERIAL2KEY(serial));
	g_hash_table_remove(p->rbug.hash, SERIAL2KEY(serial));

	if (!ptr) {
		g_print("lost message with id %u\n", serial);
		return;
	}

	e = (struct rbug_event *)ptr;
	e->func(e, header, p);

	rbug_free_header(header);
}

static gboolean rbug_event(GIOChannel *channel, GIOCondition c, gpointer data)
{
	struct program *p = (struct program *)data;
	struct rbug_connection *con = p->rbug.con;
	struct rbug_header *header;
	(void)channel;

	if (c & (G_IO_IN | G_IO_PRI)) {
		header = rbug_get_message(con, NULL);
		if (!header) {
			main_quit(p);
			return false;
		}

		rbug_handle_header(header, p);
	}

	if (c & (G_IO_ERR | G_IO_HUP | G_IO_NVAL)) {
		main_quit(p);
		return false;
	}

	return true;
}

/*
 * exported
 */

void rbug_finish_and_emit_events(struct program *p)
{
	struct rbug_connection *con = p->rbug.con;
	struct rbug_header *header;
	uint32_t serial;

	rbug_send_ping(con, &serial);

	do {
		header = rbug_get_message(con, NULL);

		if (!header) {
			g_print("Connection problems\n");
			break;
		}

		if (header->opcode < 0) {
			uint32_t s =*(uint32_t*)(&header[1]);
			if (s == serial) {
				break;
			} else if (s > serial) {
				g_print("Bad bad bad lost message that we waited for\n");
				break;
			}
		}

		rbug_handle_header(header, p);
		header = NULL;
	} while (1);

	if (header && header->opcode != RBUG_OP_PING_REPLY) {
		g_print("Bad bad bad reply which we waited for was not ping\n");
	}

	rbug_free_header(header);
}

void rbug_add_event(struct rbug_event *e, uint32_t serial, struct program *p)
{
	g_hash_table_insert(p->rbug.hash, SERIAL2KEY(serial), e);
}

void rbug_glib_io_watch(struct program *p)
{
	gint mask = (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL);

	p->rbug.channel = g_io_channel_unix_new(p->rbug.socket);
	p->rbug.event = g_io_add_watch(p->rbug.channel, mask, rbug_event, p);
	g_io_channel_set_encoding(p->rbug.channel, NULL, NULL);
	p->rbug.hash = g_hash_table_new(hash_func, equal_func);
}
