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
#include "util/u_network.h"


gboolean ask_connect(struct program *p)
{
	uint16_t port = p->ask.port;
	int socket = -1;

	while (socket < 0) {
		/* after ten tries quit */
		if (port >= p->ask.port + 10)
			return false;

		socket = u_socket_connect(p->ask.host, port++);
	}

	/* store the actual port, minus one */
	p->ask.port = --port;

	p->rbug.socket = socket;
	p->rbug.con = rbug_from_socket(socket);

	rbug_glib_io_watch(p);

	main_window_create(p);

	return true;
}

static void connect(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	char *host;
	uint16_t port;

	(void)widget;

	host = gtk_editable_get_chars(GTK_EDITABLE(p->ask.entry_host), 0, -1);
	port = (uint16_t)gtk_adjustment_get_value(p->ask.adjustment_port);

	p->ask.host = host;
	p->ask.port = port;

	if (!ask_connect(p))
		return;

	gtk_widget_destroy(p->ask.window);

	p->ask.window = NULL;
	p->ask.entry_host = NULL;
	p->ask.entry_port = NULL;
}

static void destroy(GtkWidget *widget, gpointer data)
{
	struct program *p = (struct program *)data;
	(void)widget;

	if (!p->main.window)
		main_quit(p);
}

void ask_window_create(struct program *p)
{
	GtkBuilder *builder;
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *quit;
	GtkWidget *entry_host;
	GtkWidget *entry_port;
	GtkAdjustment *adjustment_port;

	builder = gtk_builder_new();

	gtk_builder_add_from_file(builder, "res/ask.xml", NULL);
	gtk_builder_connect_signals(builder, NULL);

	window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	button = GTK_WIDGET(gtk_builder_get_object(builder, "connect"));
	quit = GTK_WIDGET(gtk_builder_get_object(builder, "quit"));
	entry_host = GTK_WIDGET(gtk_builder_get_object(builder, "entry_host"));
	entry_port = GTK_WIDGET(gtk_builder_get_object(builder, "entry_port"));
	adjustment_port = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "adjustment_port"));

	/* manualy set up signals */
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(connect), p);
	g_signal_connect(G_OBJECT(quit), "clicked", G_CALLBACK(destroy), p);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(destroy), p);

	gtk_widget_show(window);

	p->ask.window = window;
	p->ask.entry_host = entry_host;
	p->ask.entry_port = entry_port;
	p->ask.adjustment_port = adjustment_port;

	gtk_adjustment_set_value(adjustment_port, 13370);
}
