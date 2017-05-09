/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
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


#include <config.h>

#include <math.h>
#include <gtk/gtk.h>
#include "mainwindow.h"
#include "effectdialog.h"
#include "effectbrowser.h"
#include "main.h"
#include "um.h"

G_DEFINE_TYPE(EffectDialog,effect_dialog,GTK_TYPE_VBOX)

enum { APPLY_SIGNAL, SETUP_SIGNAL, TARGET_CHANGED_SIGNAL, LAST_SIGNAL };
static guint effect_dialog_signals[LAST_SIGNAL] = { 0 };

static void effect_dialog_class_init(EffectDialogClass *klass)
{
     GtkObjectClass *oc = GTK_OBJECT_CLASS(klass);
     klass->apply = NULL;
     klass->setup = NULL;
     klass->target_changed = NULL;

     effect_dialog_signals[APPLY_SIGNAL] =
          g_signal_new("apply",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(EffectDialogClass,apply),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__BOOLEAN,G_TYPE_NONE,1,G_TYPE_BOOLEAN);
     effect_dialog_signals[SETUP_SIGNAL] =
          g_signal_new("setup",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(EffectDialogClass,setup),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,G_TYPE_NONE,0);
     effect_dialog_signals[TARGET_CHANGED_SIGNAL] = 
	  g_signal_new("target-changed",G_TYPE_FROM_CLASS(klass),G_SIGNAL_RUN_FIRST,
		       G_STRUCT_OFFSET(EffectDialogClass,target_changed),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__VOID,G_TYPE_NONE,0);

     gtk_object_class_add_signals(oc,effect_dialog_signals,LAST_SIGNAL);
}

gboolean effect_dialog_apply(EffectDialog *ed)
{
     gboolean r;
     g_signal_emit(G_OBJECT(ed),effect_dialog_signals[APPLY_SIGNAL],0,&r);
     return r;
}

static void effect_dialog_init(EffectDialog *v)
{
     GtkWidget *b;

     gtk_box_set_spacing(GTK_BOX(v),3);

     b = gtk_hbox_new ( FALSE, 3 );
     v->input_area = GTK_CONTAINER(b);
     gtk_box_pack_start ( GTK_BOX(v), b, FALSE, FALSE, 0 );
     gtk_widget_show ( b );

     gtk_container_set_border_width(GTK_CONTAINER(v),5);
}

static void effect_dialog_eb_target_changed(DocumentList *dl, 
					    gpointer user_data)
{
     /* puts("effect_dialog_eb_target_changed"); */
     g_signal_emit(G_OBJECT(user_data),
		     effect_dialog_signals[TARGET_CHANGED_SIGNAL],0);
}

void effect_dialog_setup(EffectDialog *ed, gchar *effect_name, gpointer eb)
{
     g_assert(ed->eb == NULL && eb != NULL);
     ed->eb = eb;
     ed->effect_name = effect_name;
     gtk_signal_connect_while_alive
	  (GTK_OBJECT(EFFECT_BROWSER(eb)->dl),"document_changed",
	   G_CALLBACK(effect_dialog_eb_target_changed),ed,GTK_OBJECT(ed));
     g_signal_emit(G_OBJECT(ed),
                     effect_dialog_signals[SETUP_SIGNAL],0);
}

