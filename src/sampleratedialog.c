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
#include "sampleratedialog.h"
#include "chunk.h"
#include "float_box.h"
#include "soxdialog.h"
#include "pipedialog.h"
#include "um.h"
#include "inifile.h"
#include "effectbrowser.h"
#include "rateconv.h"
#include "gettext.h"

G_DEFINE_TYPE(SamplerateDialog,samplerate_dialog,EFFECT_DIALOG_TYPE)

static void set_method(Combo *cbo, gpointer user_data)
{
     SamplerateDialog *s = SAMPLERATE_DIALOG(user_data);
     s->method = combo_selected_index(cbo);
}

static Chunk *apply_proc(Chunk *chunk, StatusBar *bar, gpointer user_data)
{
     SamplerateDialog *s = SAMPLERATE_DIALOG(user_data);
     return chunk_convert_samplerate(chunk,s->rate->val,
				     rateconv_driver_id(FALSE,s->method),
				     dither_editing, bar);
}

static gboolean apply(EffectDialog *ed)
{
     gboolean b;
     Document *d = EFFECT_BROWSER(ed->eb)->dl->selected;
     SamplerateDialog *s = SAMPLERATE_DIALOG(ed);
     if (intbox_check(s->rate) || s->rate->val==0) return TRUE;

     inifile_set_guint32("srate_method",s->method);
     b = document_apply_cb(d,apply_proc,FALSE,ed);
     mainwindow_update_texts();
     return b;
}

static void samplerate_dialog_setup(EffectDialog *ed)
{
     intbox_set(SAMPLERATE_DIALOG(ed)->rate,
		EFFECT_BROWSER(ed->eb)->dl->format.samplerate);
}

static void samplerate_dialog_class_init(SamplerateDialogClass *klass)
{
     EffectDialogClass *edc = EFFECT_DIALOG_CLASS(klass);
     edc->apply = apply;
     edc->setup = samplerate_dialog_setup;
}

static void samplerate_dialog_init(SamplerateDialog *v)
{
     EffectDialog *ed = EFFECT_DIALOG(v);
     GtkWidget *a,*b,*c;
     GtkRequisition req;
     GList *l = NULL;
     int i,j;

     a = gtk_vbox_new ( FALSE, 3 );
     gtk_box_pack_start (GTK_BOX(ed->input_area),a,FALSE,FALSE,0);
     b = gtk_label_new ( _("(This changes the sample rate of \n"
			 "the entire file, not just the selection)") );
     gtk_box_pack_start ( GTK_BOX(a),b,FALSE,FALSE,0 );
     b = gtk_hbox_new ( FALSE, 3 );
     gtk_box_pack_start (GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_label_new ( _("New samplerate: ") );
     gtk_box_pack_start (GTK_BOX(b),c,FALSE,FALSE,0);
     c = intbox_new(0);
     v->rate = INTBOX(c);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     b = gtk_hbox_new( FALSE, 3 );
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_label_new(_("Method: "));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);

     c = combo_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_size_request(c,&req);
#ifdef COMBO_OLDSCHOOL
     gtk_widget_set_size_request(c,req.width*3/2,req.height);
#endif
     i = rateconv_driver_count(FALSE);
     for (j=0; j<i; j++)
	  l = g_list_append(l,(gpointer)rateconv_driver_name(FALSE,j));
     v->method = inifile_get_guint32("srate_method",0);
     if (v->method >= g_list_length(l)) v->method = 0;
     combo_set_items(COMBO(c),l,v->method);
     g_list_free(l);
     
     g_signal_connect(G_OBJECT(c),"selection_changed",
			G_CALLBACK(set_method),v);

     gtk_widget_show_all(a);
}
