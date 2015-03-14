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
#include <GL/gl.h>

static void realize(GtkWidget* widget, gpointer data)
{
#if 0
	GdkGLContext* context = gtk_widget_get_gl_context(widget);
	GdkGLDrawable* drawable = gtk_widget_get_gl_drawable(widget);
	(void)data;

	if (!gdk_gl_drawable_gl_begin(drawable, context))
		return;

	gdk_gl_drawable_gl_end(drawable);
#else
	(void)widget;
	(void)data;
#endif
}

static gboolean configure(GtkWidget* widget, GdkEventConfigure* e, gpointer data)
{
#if 0
	GdkGLContext* context = gtk_widget_get_gl_context(widget);
	GdkGLDrawable* drawable = gtk_widget_get_gl_drawable(widget);
	(void)data;
	(void)e;

	if (!gdk_gl_drawable_gl_begin(drawable, context))
		return FALSE;

	gdk_gl_drawable_gl_end (drawable);
#else
	(void)widget;
	(void)data;
	(void)e;
#endif
	return TRUE;
}

static void draw_tri(struct program *p)
{
	glClearColor(0.3, 0.1, 0.3, 0.5);
	glViewport(0, 0, (GLint)p->draw.width, (GLint)p->draw.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -0.5, 1000.0);
	glMatrixMode(GL_MODELVIEW);

	glClear(GL_COLOR_BUFFER_BIT);

	glBegin(GL_TRIANGLES);
	glColor3f(.8,0,0);
	glVertex3f(-0.9, -0.9, -30.0);
	glColor3f(0,.9,0);
	glVertex3f( 0.9, -0.9, -30.0);
	glColor3f(0,0,.7);
	glVertex3f( 0.0,  0.9, -30.0);
	glEnd();
}

gboolean draw_gl_begin(struct program *p)
{
	GtkWidget *widget = GTK_WIDGET(p->main.draw);
	GdkGLContext* context = gtk_widget_get_gl_context(widget);
	GdkGLDrawable* drawable = gtk_widget_get_gl_drawable(widget);

	return gdk_gl_drawable_gl_begin(drawable, context);
}

void draw_gl_end(struct program *p)
{
	GtkWidget *widget = GTK_WIDGET(p->main.draw);
	GdkGLDrawable* drawable = gtk_widget_get_gl_drawable(widget);

	gdk_gl_drawable_gl_end(drawable);
}

void draw_ortho_top_left(struct program *p)
{
	glViewport(0, 0, (GLint)p->draw.width, (GLint)p->draw.height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, p->draw.width, p->draw.height, 0.0, -0.5, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void draw_checker(guint width, guint height, struct program *p)
{
	int x;
	int y;
	int i;

	glClearColor(0.3, 0.3, 0.3, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glColor3f(.8,.8,.8);
	glBegin(GL_QUADS);
	for (i = 0, x = 0; x < (int)p->draw.width; i++) {
		x = (width * i) - (width / 2);
		y = ((i % 2) * height) - (height / 2);
		for (; y < (int)p->draw.height; y += height * 2) {
			glVertex3f( x +   0.0, y + height, -30.0);
			glVertex3f( x + width, y + height, -30.0);
			glVertex3f( x + width, y +    0.0, -30.0);
			glVertex3f( x +   0.0, y +    0.0, -30.0);
		}
	}
	glEnd();
}

static gboolean expose(GtkWidget* widget, GdkEventExpose* e, gpointer data)
{
	struct program *p = (struct program *)data;
	GdkGLContext* context = gtk_widget_get_gl_context(widget);
	GdkGLDrawable* drawable = gtk_widget_get_gl_drawable(widget);
	(void)e;

	p->draw.width = widget->allocation.width;
	p->draw.height = widget->allocation.height;

	if (!gdk_gl_drawable_gl_begin(drawable, context))
		return FALSE;

	if (p->viewed.type == TYPE_TEXTURE)
		texture_draw(p);
	else
		draw_tri(p);

	if (gdk_gl_drawable_is_double_buffered (drawable))
		gdk_gl_drawable_swap_buffers(drawable);
	else
		glFlush();

	gdk_gl_drawable_gl_end(drawable);

	return TRUE;
}

void draw_setup(GtkDrawingArea *draw, struct program *p)
{
	GObject *obj = G_OBJECT(draw);

	gtk_widget_set_gl_capability(GTK_WIDGET(draw), p->draw.config, NULL,
	                             TRUE, GDK_GL_RGBA_TYPE);

	g_signal_connect_after(obj, "realize", G_CALLBACK(realize), p);
	g_signal_connect(obj, "configure-event", G_CALLBACK(configure), p);
	g_signal_connect(obj, "expose-event", G_CALLBACK(expose), p);
}
