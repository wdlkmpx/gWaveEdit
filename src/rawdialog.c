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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 */


#include <config.h>

#include <gtk/gtk.h>
#include "rawdialog.h"
#include "inifile.h"
#include "formatselector.h"
#include "int_box.h"
#include "main.h"
#include "gettext.h"

static gboolean ok_flag, destroy_flag;
static Dataformat fmt;
static FormatSelector *fs;

static void rawdialog_ok(GtkButton *button, gpointer user_data)
{
     if (format_selector_check(fs)) {
          gtk_signal_emit_stop_by_name(GTK_OBJECT(button),"clicked");
          return;
     }
     format_selector_get(fs,&fmt);
     ok_flag = TRUE;
}

static void rawdialog_destroy(GtkObject *object, gpointer user_data)
{
     destroy_flag = TRUE;
}

Dataformat *rawdialog_execute(gchar *filename)
{
     GtkWindow *w;
     GtkWidget *a,*b,*c;
     gchar *ch;
     
     fmt.type = DATAFORMAT_PCM;
     fmt.samplerate = 22050;
     fmt.channels = 1;
     fmt.samplesize = 1;
     fmt.samplebytes= fmt.samplesize * fmt.channels;
     fmt.sign = FALSE;
     fmt.bigendian = IS_BIGENDIAN;
     dataformat_get_from_inifile("rawDialog",TRUE,&fmt);     
     
     w = GTK_WINDOW(gtk_window_new(GTK_WINDOW_DIALOG));
     gtk_window_set_title(w,_("Unknown file format"));
     gtk_window_set_modal(w,TRUE);
     gtk_container_set_border_width(GTK_CONTAINER(w),5);
     gtk_signal_connect(GTK_OBJECT(w),"destroy",
          GTK_SIGNAL_FUNC(rawdialog_destroy),NULL);
     a = gtk_vbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(w),a);
     ch = g_strdup_printf(_("The format of file '%s' could not be recognized.\n\n"
			  "Please specify the sample format below."),filename);
     b = gtk_label_new(ch);
     gtk_label_set_line_wrap(GTK_LABEL(b),TRUE);
     gtk_box_pack_start(GTK_BOX(a),b,TRUE,FALSE,0);
     g_free(ch);
     b = format_selector_new(TRUE);
     fs = FORMAT_SELECTOR(b);
     format_selector_set(fs,&fmt);
     gtk_container_add(GTK_CONTAINER(a),b);
     b = gtk_hseparator_new();
     gtk_container_add(GTK_CONTAINER(a),b);
     b = gtk_hbutton_box_new();
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_button_new_with_label(_("OK"));
     gtk_signal_connect(GTK_OBJECT(c),"clicked",GTK_SIGNAL_FUNC(rawdialog_ok),
          NULL);
     gtk_signal_connect_object(GTK_OBJECT(c),"clicked",
          GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(w));
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_button_new_with_label(_("Cancel"));
     gtk_signal_connect_object(GTK_OBJECT(c),"clicked",
          GTK_SIGNAL_FUNC(gtk_widget_destroy),GTK_OBJECT(w));
     gtk_container_add(GTK_CONTAINER(b),c);
     
     ok_flag = destroy_flag = FALSE;
     
     gtk_widget_show_all(GTK_WIDGET(w));
     while (!destroy_flag) mainloop(TRUE);
     
     if (!ok_flag) return NULL;

     dataformat_save_to_inifile("rawDialog",&fmt,TRUE);
     return &fmt;
}