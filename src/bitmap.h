/*
 * Copyright (C) 2010, Magnus Hjorth
 *
 * This file is part of mhWaveEdit.
 *
 * mhWaveEdit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by        
 * the Free Software Foundation; either version 2 of the License, or  
 * (at your option) any later version.
 *
 * mhWaveEdit is distributed in the hope that it will be useful,   
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mhWaveEdit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


/* Simple widget to display an xbm bitmap.
 * Supports two color modes (default is black color):
 *   Constant color - set using bitmap_set_color
 *   Themed - interpolated between the current theme's fg and bg colors
 *     Set using bitmap_set_fg (alpha=0.0 gives bg color, 1.0 gives fg color). 
 * In both modes the bitmap has transparent background.
 */

#ifndef BITMAP_H_INCLUDED
#define BITMAP_H_INCLUDED

#include <gtk/gtk.h>

#define BITMAP_TYPE          (bitmap_get_type ())
#define BITMAP(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), BITMAP_TYPE, Bitmap))
#define IS_BITMAP(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BITMAP_TYPE))
#define BITMAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  BITMAP_TYPE, BitmapClass))

typedef struct {
     GtkWidget wid;
     GdkGC *gc;
     gboolean update_gc;
     GdkBitmap *bmp;
     gint bmpw, bmph;
     gint color_mode; /* 0=fixed, 1=between fg/bg */
     GdkColor clr;
     gint alpha; /* 0..256 */
} Bitmap;

typedef struct {
     GtkWidgetClass klass;
} BitmapClass;

GType bitmap_get_type(void);
GtkWidget *bitmap_new_from_data(unsigned char *data, int width, int height);
void bitmap_set_color(Bitmap *bmp, GdkColor *clr);
void bitmap_set_fg(Bitmap *bmp, gfloat alpha);

#endif
