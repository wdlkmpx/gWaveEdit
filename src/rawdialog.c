/*
 * Copyright (C) 2002 2003 2005 2007, Magnus Hjorth
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

#include <gtk/gtk.h>
#include "rawdialog.h"
#include "inifile.h"
#include "formatselector.h"
#include "int_box.h"
#include "main.h"
#include "mainloop.h"

static gboolean ok_flag, destroy_flag;
static Dataformat fmt;
static FormatSelector *fs;
static Intbox *offset_box;
static gint maxhdrsize;

static void
rawdialog_response (GtkDialog * dlg, int response, gpointer user_data)
{
   if (response == GTK_RESPONSE_OK)
   {
      if (format_selector_check(fs) || intbox_check_limit(offset_box,0,maxhdrsize,_("header size")))
      {
         g_signal_stop_emission_by_name (dlg, "response");
         return;
      }
      format_selector_get (fs,&fmt);
      ok_flag = TRUE;
   }
   gtk_widget_destroy (GTK_WIDGET (dlg));
   destroy_flag = TRUE;
}

Dataformat *rawdialog_execute(gchar *filename, gint filesize, guint *offset)
{
     GtkWidget * dialog, * vbox;
     GtkWidget *b,*c;
     gchar *ch;
     
     memset(&fmt,0,sizeof(fmt));
     fmt.type = DATAFORMAT_PCM;
     fmt.samplerate = 22050;
     fmt.channels = 1;
     fmt.samplesize = 1;
     fmt.samplebytes= fmt.samplesize * fmt.channels;
     fmt.sign = FALSE;
     fmt.bigendian = IS_BIGENDIAN;
     dataformat_get_from_inifile("rawDialog",TRUE,&fmt);    
     maxhdrsize = filesize;
     
     //w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     dialog = gtk_dialog_new ();
     gtk_window_set_title (GTK_WINDOW (dialog), _("Unknown file format"));
     gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
     gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
     gtk_container_set_border_width (GTK_CONTAINER (dialog),5);

     vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
     gtk_box_set_spacing (GTK_BOX (vbox), 3);

     ch = g_strdup_printf(_("The format of file '%s' could not be recognized.\n\n"
			  "Please specify the sample format below."),filename);
     b = gtk_label_new(ch);
     gtk_label_set_line_wrap(GTK_LABEL(b),TRUE);
     gtk_box_pack_start (GTK_BOX (vbox), b, TRUE, FALSE, 0);
     g_free(ch);
     b = format_selector_new(TRUE);
     fs = FORMAT_SELECTOR(b);
     format_selector_set(fs,&fmt);
     gtk_container_add (GTK_CONTAINER (vbox), b);
     b = gtk_hbox_new(FALSE,0);
     gtk_container_add (GTK_CONTAINER (vbox), b);
     c = gtk_label_new(_("Header bytes: "));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = intbox_new(inifile_get_guint32("rawDialog_offset",0));
     offset_box = INTBOX(c);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     b = gtk_hseparator_new();
     gtk_container_add (GTK_CONTAINER (vbox), b);

     gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);
     gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
     g_signal_connect (dialog, "response", G_CALLBACK (rawdialog_response), NULL);
     
     ok_flag = destroy_flag = FALSE;
     
     gtk_widget_show_all (dialog);

     // wait for the dialog to close
     while (!destroy_flag) {
        mainloop();
     }
     
     if (!ok_flag) return NULL;

     *offset = (guint) offset_box->val;
     dataformat_save_to_inifile("rawDialog",&fmt,TRUE);
     inifile_set_guint32("rawDialog_offset",*offset);
     return &fmt;
}
