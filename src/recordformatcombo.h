/*
 * Copyright (C) 2009, Magnus Hjorth
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


#ifndef RECORDFORMATCOMBO_H_INCLUDED
#define RECORDFORMATCOMBO_H_INCLUDED

#include <gtk/gtk.h>
#include "combo.h"
#include "dataformat.h"
#include "listobject.h"

#define RECORD_FORMAT_COMBO_TYPE          (record_format_combo_get_type ())
#define RECORD_FORMAT_COMBO(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), RECORD_FORMAT_COMBO_TYPE, RecordFormatCombo))
#define IS_RECORD_FORMAT_COMBO(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RECORD_FORMAT_COMBO_TYPE))
#define RECORD_FORMAT_COMBO_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  RECORD_FORMAT_COMBO_TYPE, RecordFormatComboClass))

typedef struct {
     gint num;
     gchar *name;
     Dataformat fmt;
} RecordFormat;


typedef struct {
     Combo c;

     /* Private */
     int current_selection_type; /* 0 none, 1 user preset, 2 driver preset, 
				  * 3 custom */
     gchar *current_selection_name;
     Dataformat current_selection_format;

     int stored_selection_type;
     gchar *stored_selection_name;
     Dataformat stored_selection_format;

     int named_preset_start,nameless_preset_start,custom_start,other_start;
     ListObject *named_presets,*nameless_presets;

     gboolean show_other;

} RecordFormatCombo;

typedef struct {
     ComboClass klass;
     void (*format_changed)(RecordFormatCombo *rfc, Dataformat *new_fmt);
     void (*format_dialog_request)(RecordFormatCombo *rfc);
} RecordFormatComboClass;

GType record_format_combo_get_type(void);

GtkWidget *record_format_combo_new(ListObject *named_presets, 
				   ListObject *driver_presets,
				   gboolean show_dialog_item);

gboolean record_format_combo_set_named_preset(RecordFormatCombo *rfc, 
					     gchar *preset_name);

void record_format_combo_set_format(RecordFormatCombo *rfc, Dataformat *fmt);

void record_format_combo_store(RecordFormatCombo *rfc);
void record_format_combo_recall(RecordFormatCombo *rfc);

Dataformat *record_format_combo_get_format(RecordFormatCombo *rfc);
gchar *record_format_combo_get_preset_name(RecordFormatCombo *rfc);

#endif
