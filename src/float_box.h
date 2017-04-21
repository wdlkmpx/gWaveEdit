/*
 * Copyright (C) 2002 2003 2005, Magnus Hjorth
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


#ifndef FLOATBOX_H_INCLUDED
#define FLOATBOX_H_INCLUDED

#define FLOATBOX_TYPE          (floatbox_get_type ())
#define FLOATBOX(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOATBOX_TYPE, Floatbox))
#define IS_FLOATBOX(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOATBOX_TYPE))
#define FLOATBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  FLOATBOX_TYPE, FloatboxClass))

typedef struct _Floatbox Floatbox;
typedef struct _FloatboxClass FloatboxClass;

struct _Floatbox {
     GtkEntry parent;
     float val;
     GtkAdjustment *adj;
};

struct _FloatboxClass {
	GtkEntryClass parent;
	void (*numchange)(Floatbox *, float);
	};

GType floatbox_get_type(void);
GtkWidget *floatbox_new(float value);
GtkWidget *floatbox_create_scale(Floatbox *box, float minval, float maxval);
void floatbox_set(Floatbox *box, float value);
gboolean floatbox_check(Floatbox *box);
gboolean floatbox_check_limit(Floatbox *box, float lowest, float highest,
			      gchar *valuename);


#endif



