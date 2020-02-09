/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
 *
 * This file is part of gWaveEdit.
 *
 * gWaveEdit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by        
 * the Free Software Foundation; either version 2 of the License, or  
 * (at your option) any later version.
 *
 * gWaveEdit is distributed in the hope that it will be useful,   
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gWaveEdit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


/* ChunkView is a graphical component that gives a view of the data inside a 
 * chunk object. 
 */

#ifndef CHUNKVIEW_H_INCLUDED
#define CHUNKVIEW_H_INCLUDED

#include <gtk/gtk.h>
#include "chunk.h"
#include "document.h"
#include "viewcache.h"

#define CHUNKVIEW_TYPE          (chunk_view_get_type ())
#define CHUNKVIEW(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), CHUNKVIEW_TYPE, ChunkView))
#define IS_CHUNKVIEW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CHUNKVIEW_TYPE))
#define CHUNKVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  CHUNKVIEW_TYPE, ChunkViewClass))

typedef struct _ChunkView {

     /* PRIVATE MEMBERS - Keep out! */

     GtkDrawingArea parent;

     /* Image used for double-buffering the sample view. Only used under GTK2,
      * however the image_width/image_height variables are used at various
      * places so they are kept updated (ugly solution) */
     GdkPixmap *image;
     guint image_width,image_height;     


     ViewCache *cache;
     int last_redraw_time,need_redraw_left,need_redraw_right;

     gfloat scale_factor; /* Vertical scaling factor (default: 1.0) */

     /* READ-ONLY PUBLIC MEMBERS */
     /* To change these, use the public functions below. */

     Document *doc;
     gboolean timescale;        /* Time scale visible flag */
     gboolean show_bufpos;

} ChunkView;

typedef struct _ChunkViewClass {
     GtkDrawingAreaClass parent_class;

     /* SIGNALS */

     /* Emitted when the user double-clicked somewhere in the view */
     /* The off_t pointer is because off_t values aren't directly supported by
      * the GTK (1.2) type system */
     void (*double_click)(ChunkView *view, off_t *sample);

} ChunkViewClass;

GType chunk_view_get_type(void);


/* Creates a new ChunkView */
GtkWidget *chunk_view_new(void);


void chunk_view_set_document(ChunkView *cv, Document *d);


/* Selects whether a time scale should be drawn at the bottom of the view. */
void chunk_view_set_timescale(ChunkView *cv, gboolean scale_visible);


/* This is called when the caches should progress. */

gboolean chunk_view_update_cache(ChunkView *cv);

void chunk_view_force_repaint(ChunkView *cv);

void chunk_view_use_backing_pixmap(ChunkView *cv, gboolean use_pixmap);

void chunk_view_set_scale(ChunkView *cv, gfloat scale);

#endif
