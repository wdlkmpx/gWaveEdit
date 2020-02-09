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


#include <config.h>

#include <math.h>
#include <gtk/gtk.h>
#include "mainwindow.h"
#include "effectdialog.h"
#include "effectbrowser.h"
#include "speeddialog.h"
#include "float_box.h"
#include "gettext.h"

G_DEFINE_TYPE(SpeedDialog,speed_dialog,EFFECT_DIALOG_TYPE)

static Chunk *apply_proc(Chunk *chunk, StatusBar *bar, gpointer user_data)
{
     SpeedDialog *s = SPEED_DIALOG(user_data);
     return chunk_convert_speed(chunk, s->speed->val / 100.0, dither_editing, 
				bar);
}

static gboolean apply(EffectDialog *ed)
{
     SpeedDialog *s = SPEED_DIALOG(ed);
     if (floatbox_check(s->speed) || s->speed->val<=0.0) return TRUE; 
     if (s->speed->val==100.0) return FALSE;
     return document_apply_cb(EFFECT_BROWSER(EFFECT_DIALOG(s)->eb)->
			      dl->selected, apply_proc,TRUE,s);
}

static void speed_dialog_class_init(SpeedDialogClass *klass)
{
     EffectDialogClass *edc = EFFECT_DIALOG_CLASS(klass);
     edc->apply = apply;
}

static void speed_dialog_init(SpeedDialog *v)
{
     EffectDialog *ed = EFFECT_DIALOG(v);
     GtkWidget *c;
     c = gtk_label_new (_("Speed:"));
     gtk_box_pack_start( GTK_BOX(ed->input_area), c, FALSE, FALSE, 0 );
     gtk_widget_show(c);
     c = floatbox_new(100.0);
     v->speed = FLOATBOX(c);
     gtk_box_pack_start( GTK_BOX(ed->input_area), c, FALSE, FALSE, 0 );
     gtk_widget_show(c);
     c = gtk_label_new("%");
     gtk_box_pack_start( GTK_BOX(ed->input_area), c, FALSE, FALSE, 0 );
     gtk_widget_show(c);
}
