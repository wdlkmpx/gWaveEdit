/*
 * Copyright (C) 2002 2003 2004 2009, Magnus Hjorth
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


/* This is a dropdown read-only combo box with text items. */


#ifndef COMBO_H_INCLUDED
#define COMBO_H_INCLUDED

#include "main.h"

#define COMBO_TYPE          (combo_get_type ())
#define COMBO(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), COMBO_TYPE, Combo))
#define IS_COMBO(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COMBO_TYPE))
#define COMBO_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  COMBO_TYPE, ComboClass))

typedef struct {
     GtkComboBoxText parent;
     int max_request_width;
} Combo;

typedef struct {
     GtkComboBoxTextClass parent_class;
     void (*selection_changed)(Combo *obj);
} ComboClass;

GType combo_get_type(void);
GtkWidget *combo_new(void);
/* The removed item must not be currently selected */
void combo_remove_item(Combo *combo, int item_index);
void combo_set_max_request_width(Combo *c, int w);

//====
void w_gtk_glist_to_combo (GtkComboBox *combo, GList *strings, int default_index);
void w_gtk_strv_to_combo (GtkComboBox *combo, char **strv, int default_index);

#endif
