
#ifndef _RBUG_GUI_PROGRAM_H_
#define _RBUG_GUI_PROGRAM_H_

#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

#include "rbug/rbug.h"

struct texture_action_read;
struct shader_action_info;

enum columns {
	COLUMN_ID = 0,
	COLUMN_TYPE,
	COLUMN_TYPENAME,
	COLUMN_PIXBUF,
};

enum types {
	TYPE_NONE = 0,
	TYPE_SCREEN,
	TYPE_CONTEXT,
	TYPE_TEXTURE,
	TYPE_SHADER,
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
		GtkTextView *textview;
		GtkWidget *textview_scrolled;
		GtkTreeView *treeview;
		GtkTreeStore *treestore;
		GtkDrawingArea *draw;

		GtkTreeIter top;
	} main;

	struct {
		GtkTreeIter iter;
		enum types type;
		guint64 parent;
		guint64 id;
	} selected;

	struct {
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
		GHashTable *hash;
	} rbug;

	struct {
		GHashTable *hash;
	} icon;
};

struct rbug_event
{
	void (*func)(struct rbug_event *, struct rbug_header *, struct program *);
};


/* src/ask.c */
void ask_window_create(struct program *p);
gboolean ask_connect(struct program *p);


/* src/main.c */
void main_window_create(struct program *p);
void main_quit(struct program *p);
void icon_add(const char *filename, const char *name, struct program *p);
GdkPixbuf* icon_get(const char *name, struct program *p);


/* src/rbug.c */
void rbug_add_event(struct rbug_event *e, uint32_t serial, struct program *p);
void rbug_glib_io_watch(struct program *p);
void rbug_finish_and_emit_events(struct program *p);


/* src/context.c */
void context_list(GtkTreeStore *store,
                  GtkTreeIter *parent,
                  struct program *p);

/* src/texture.c */
void texture_list(GtkTreeStore *store, GtkTreeIter *parent, struct program *p);
void texture_unselected(struct program *p);
void texture_selected(struct program *p);
void texture_refresh(struct program *p);
void texture_draw(struct program *p);


/* src/shader.c */
void shader_unselected(struct program *p);
void shader_selected(struct program *p);
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
