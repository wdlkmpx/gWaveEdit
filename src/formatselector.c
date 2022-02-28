/*
 * Copyright (C) 2002 2003 2004 2005 2010 2012, Magnus Hjorth
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

#include "main.h"
#include "formatselector.h"
#include "inifile.h"

#define DEFAULT_CHANS 2
#define DEFAULT_RATE 44100

G_DEFINE_TYPE(FormatSelector,format_selector,GTK_TYPE_TABLE)

static void format_selector_class_init(FormatSelectorClass *klass)
{
}

static void samplesize_changed (GtkComboBox *obj, gpointer user_data)
{
     int i;
     FormatSelector *fs = FORMAT_SELECTOR(user_data);
     i = gtk_combo_box_get_active (obj);
     gtk_widget_set_sensitive(GTK_WIDGET(fs->sign_combo),(i<4));
     gtk_widget_set_sensitive(GTK_WIDGET(fs->packing_combo),(i==2));
     if (i == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (fs->sign_combo),0);
     else if (i < 4)
        gtk_combo_box_set_active (GTK_COMBO_BOX (fs->sign_combo),1);
     else
        gtk_combo_box_set_active (GTK_COMBO_BOX (fs->endian_combo),
                                  dataformat_sample_t.bigendian?1:0);
}

static void format_selector_init(FormatSelector *fs)
{
     GtkWidget *a=GTK_WIDGET(fs);

     gtk_table_set_row_spacings(GTK_TABLE(fs),3);
     gtk_table_set_col_spacings(GTK_TABLE(fs),4);
     attach_label(_("Sample type: "),GTK_WIDGET(fs),1,0);

     fs->samplesize_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("8 bit PCM"));
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("16 bit PCM"));
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("24 bit PCM"));
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("32 bit PCM"));
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("Floating-point, single"));
     gtk_combo_box_text_append_text (fs->samplesize_combo, _("Floating-point, double"));
     g_signal_connect (G_OBJECT(fs->samplesize_combo),"changed",
                       G_CALLBACK(samplesize_changed), fs);
     gtk_table_attach(GTK_TABLE(a),GTK_WIDGET(fs->samplesize_combo),1,2,1,2,GTK_FILL,0,0,0);
     attach_label(_("Signedness: "),a,2,0);

     fs->sign_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
     gtk_combo_box_text_append_text (fs->sign_combo, _("Unsigned"));
     gtk_combo_box_text_append_text (fs->sign_combo, _("Signed"));
     gtk_table_attach(GTK_TABLE(a),GTK_WIDGET(fs->sign_combo),1,2,2,3,GTK_FILL,0,0,0);
     attach_label(_("Endianness: "),a,3,0);

     fs->endian_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
     gtk_combo_box_text_append_text (fs->endian_combo, _("Little endian"));
     gtk_combo_box_text_append_text (fs->endian_combo, _("Big endian"));
     gtk_table_attach(GTK_TABLE(a),GTK_WIDGET(fs->endian_combo),1,2,3,4,GTK_FILL,0,0,0);
     attach_label(_("Alignment:"),a,4,0);

     fs->packing_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
     gtk_combo_box_text_append_text (fs->packing_combo, _("Packed"));
     gtk_combo_box_text_append_text (fs->packing_combo, _("Top bytes"));
     gtk_combo_box_text_append_text (fs->packing_combo, _("Bottom bytes"));
     gtk_widget_set_sensitive (GTK_WIDGET (fs->packing_combo), FALSE);
     gtk_table_attach(GTK_TABLE(a),GTK_WIDGET(fs->packing_combo),1,2,4,5,GTK_FILL,0,0,0);
     
     fs->channel_combo = NULL;
     fs->rate_box = NULL;

     gtk_widget_show_all(GTK_WIDGET(fs));
     gtk_widget_hide(GTK_WIDGET(fs));
}

/* Show channel and sample rate items */
static void format_selector_show_full(FormatSelector *fs)
{
     GtkWidget *a=GTK_WIDGET(fs),*b,*c;
     guint i;
     attach_label(_("Channels: "),a,0,0);
     fs->channel_combo = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
     for (i=1; i<9; i++) {
         gtk_combo_box_text_append_text (fs->channel_combo, channel_format_name(i));
     }
     gtk_combo_box_set_active (GTK_COMBO_BOX (fs->channel_combo), DEFAULT_CHANS-1);
     gtk_table_attach(GTK_TABLE(fs),GTK_WIDGET(fs->channel_combo),1,2,0,1,GTK_FILL,0,0,0);
     gtk_widget_show (GTK_WIDGET (fs->channel_combo));

     attach_label(_("Sample rate: "),a,5,0);
     b = gtk_alignment_new(0.0,0.5,0.0,1.0);
     gtk_table_attach(GTK_TABLE(fs),b,1,2,5,6,GTK_FILL,0,0,0);
     gtk_widget_show(b);
     c = intbox_new(DEFAULT_RATE);
     fs->rate_box = INTBOX(c);
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_widget_show(c);
}

GtkWidget *format_selector_new(gboolean show_full)
{
     GtkWidget *fs = g_object_new(FORMAT_SELECTOR_TYPE, NULL);
     if (show_full) format_selector_show_full(FORMAT_SELECTOR(fs));
     return fs;
}

void format_selector_set(FormatSelector *fs, Dataformat *fmt)
{
    if (fmt->type == DATAFORMAT_PCM) {
        if (fmt->samplesize == 4 && fmt->packing != 0) {
            gtk_combo_box_set_active (GTK_COMBO_BOX (fs->samplesize_combo), 2);
            gtk_combo_box_set_active (GTK_COMBO_BOX (fs->packing_combo), fmt->packing);
        } else {
            gtk_combo_box_set_active (GTK_COMBO_BOX (fs->samplesize_combo), fmt->samplesize-1);
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX (fs->sign_combo), fmt->sign?1:0);
    } else {
        if (fmt->samplesize == sizeof(float)) {
            gtk_combo_box_set_active (GTK_COMBO_BOX (fs->samplesize_combo), 4);
        } else {
            gtk_combo_box_set_active (GTK_COMBO_BOX (fs->samplesize_combo), 5);
        }
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (fs->endian_combo), fmt->bigendian?1:0);
    if (fs->channel_combo != NULL) {
        gtk_combo_box_set_active (GTK_COMBO_BOX (fs->channel_combo), fmt->channels-1);
    }
    if (fs->rate_box != NULL) {
        intbox_set(fs->rate_box, fmt->samplerate);
    }
}

void format_selector_get(FormatSelector *fs, Dataformat *result)
{
    int i;
    i = gtk_combo_box_get_active (GTK_COMBO_BOX (fs->samplesize_combo));
    if (i<4) {
        result->type = DATAFORMAT_PCM;
        if (i == 2) {
            result->packing = gtk_combo_box_get_active (GTK_COMBO_BOX (fs->packing_combo));
            result->samplesize = (result->packing != 0)?4:3;
        } else {
            result->samplesize = i+1;
            result->packing = 0;
        }
    } else {
        result->type = DATAFORMAT_FLOAT;
        result->samplesize = (i>4)?sizeof(double):sizeof(float);
    }
    result->sign = gtk_combo_box_get_active (GTK_COMBO_BOX (fs->sign_combo));
    result->bigendian = gtk_combo_box_get_active (GTK_COMBO_BOX (fs->endian_combo));
    if (fs->channel_combo != NULL) {
        result->channels = gtk_combo_box_get_active(GTK_COMBO_BOX(fs->channel_combo)) + 1;
    } else {
        result->channels = DEFAULT_CHANS;
    }
    if (fs->rate_box != NULL) {
        intbox_check(fs->rate_box);
        result->samplerate = fs->rate_box->val;
    } else {
        result->samplerate = DEFAULT_RATE;
    }
    result->samplebytes = result->samplesize * result->channels;
}

void format_selector_set_from_inifile(FormatSelector *fs, gchar *ini_prefix)
{
     Dataformat fmt;
     if (dataformat_get_from_inifile(ini_prefix,fs->channel_combo!=NULL,&fmt))
	  format_selector_set(fs,&fmt);
}

void format_selector_save_to_inifile(FormatSelector *fs, gchar *ini_prefix)
{
     Dataformat f;
     format_selector_get(fs,&f);
     dataformat_save_to_inifile(ini_prefix,&f,fs->channel_combo!=NULL);
}

gboolean format_selector_check(FormatSelector *fs)
{
     if (fs->rate_box != NULL)
	  return intbox_check_limit(fs->rate_box,1000,500000,_("sample rate"));
     else
	  return FALSE;
}
