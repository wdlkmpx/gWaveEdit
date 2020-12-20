/*
 * Copyright (C) 2003, Magnus Hjorth
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


#ifndef HISTORYBOX_H_INCLUDED
#define HISTORYBOX_H_INCLUDED

#include <gtk/gtk.h>
#include "gtkcompat.h"

#define HISTORY_BOX_TYPE          (history_box_get_type ())
#define HISTORY_BOX(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), HISTORY_BOX_TYPE, HistoryBox))
#define IS_HISTORY_BOX(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HISTORY_BOX_TYPE))
#define HISTORY_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  HISTORY_BOX_TYPE, HistoryBoxClass))

typedef struct {
     GtkComboBoxText combo;
     gpointer history;
} HistoryBox;

typedef struct {
     GtkComboBoxTextClass comboclass;
} HistoryBoxClass;

GType history_box_get_type(void);
GtkWidget *history_box_new(gchar *historyname, gchar *value);
gchar *history_box_get_value(HistoryBox *box);
void history_box_set_history(HistoryBox *box, gchar *historyname);
void history_box_set_value(HistoryBox *box, gchar *value);
void history_box_rotate_history(HistoryBox *box);

#endif
