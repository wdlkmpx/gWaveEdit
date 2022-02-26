/*
 * Copyright (C) 2004 2006 2009, Magnus Hjorth
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


#include "combo.h"

G_DEFINE_TYPE(Combo,combo,GTK_TYPE_COMBO_BOX_TEXT)

enum { CHANGED_SIGNAL, LAST_SIGNAL };
static guint combo_signals[LAST_SIGNAL] = { 0 };

static void combo_size_request(GtkWidget *widget, GtkRequisition *req)
{
     Combo *obj = COMBO(widget);
     GTK_WIDGET_CLASS(combo_parent_class)->size_request(widget,req);
     if (obj->max_request_width >= 0 && req->width > obj->max_request_width)
	  req->width = obj->max_request_width;
}

static void combo_changed(GtkComboBox *combo)
{
     if (GTK_COMBO_BOX_CLASS(combo_parent_class)->changed)
	  GTK_COMBO_BOX_CLASS(combo_parent_class)->changed(combo);
}

static void combo_class_init(ComboClass *klass)
{
     GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
     GTK_COMBO_BOX_CLASS(klass)->changed = combo_changed;
     klass->selection_changed = NULL;
     wc->size_request = combo_size_request;
     combo_signals[CHANGED_SIGNAL] = 
	  g_signal_new("selection_changed", G_TYPE_FROM_CLASS(klass),
	               G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(ComboClass,selection_changed),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__VOID,G_TYPE_NONE,0);
}


static void combo_init(Combo *obj)
{
  obj->max_request_width = -1;
}

void combo_set_items(Combo *combo, GList *item_strings, int default_index)
{
     GList *l;
     gchar *c;
     int len;
     gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (combo));
     for (l=item_strings,len=0; l!=NULL; l=l->next,len++) {
	  c = (gchar *)l->data;
	  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),c);
     }
     if (default_index >= len || default_index < 0) {
        default_index = 0;
     }
     gtk_combo_box_set_active(GTK_COMBO_BOX(combo),default_index);
}

void combo_set_selection(Combo *combo, int item_index)
{
     gtk_combo_box_set_active(GTK_COMBO_BOX(combo),item_index);
}

int combo_selected_index(Combo *combo)
{
     return gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}

char *combo_selected_string(Combo *combo)
{
   char * str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
   return (str); /* must be freed */
}

void combo_remove_item(Combo *combo, int item_index)
{
     int i;
     i = combo_selected_index(combo);
     g_assert(i != item_index);
     gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(combo),item_index);
}

GtkWidget *combo_new(void)
{
     return (GtkWidget *) g_object_new(COMBO_TYPE, NULL);
}

void combo_set_max_request_width(Combo *c, int width)
{
     c->max_request_width = width;
}
