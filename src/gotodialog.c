/*
 * Copyright (C) 2003 2004 2006 2011, Magnus Hjorth
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
#include <gdk/gdkkeysyms.h>

#include "gotodialog.h"
#include "inifile.h"
#include "main.h"
#include "gettext.h"

#define GOTODLG_POS_AFTER_BEG_FILE 0
#define GOTODLG_POS_AFTER_END_FILE 1
#define GOTODLG_POS_AFTER_CURSOR   2
#define GOTODLG_POS_AFTER_BEG_SEL  3
#define GOTODLG_POS_AFTER_END_SEL  4

#define GOTODLG_UNIT_SECONDS 0
#define GOTODLG_UNIT_SAMPLES 1

typedef struct _GotoDialog_data
{
   Mainwindow *mw;
   Floatbox *offset;
   GtkToggleButton *relbuttons[5];
   GtkToggleButton *unitbuttons[2];
} GotoDialog_data;


static gboolean goto_dialog_apply(GotoDialog_data *gd)
{
   off_t p=0,q;
   float f;
   Document *d = gd->mw->doc;
   int i, j;
   if (d == NULL) {
      return FALSE;
   }
   if (floatbox_check(gd->offset)) {
      return TRUE;
   }

   for (i=0; i<5; i++) {
     if (gtk_toggle_button_get_active(gd->relbuttons[i]))
        break;
   }
   switch (i)
   {
      case GOTODLG_POS_AFTER_BEG_FILE:
         p = 0;
         break;
      case GOTODLG_POS_AFTER_END_FILE:
         p = d->chunk->length;
         break;
      case GOTODLG_POS_AFTER_CURSOR:
         p = d->cursorpos;
         break;
      case GOTODLG_POS_AFTER_BEG_SEL:
         p = (d->selstart==d->selend) ? 0 : d->selstart;
         break;
      case GOTODLG_POS_AFTER_END_SEL:
         p = (d->selstart==d->selend) ? d->chunk->length : d->selend;
         break;
      default:
         g_assert_not_reached();
   }

   for (j=0; j<2; j++) {
      if (gtk_toggle_button_get_active(gd->unitbuttons[j]))
         break;
   }

   f = gd->offset->val;
   if (j == GOTODLG_UNIT_SECONDS) {
      f *= ((float)(d->chunk->format.samplerate));
   }
   q = p + (off_t)f;

   if (q > d->chunk->length) {
      q = d->chunk->length;
   } else if (q < 0) {
      q = 0;
   }
     
   document_set_cursor(d,q);

   inifile_set_gfloat ("gotoOffset",gd->offset->val);
   inifile_set_guint32 ("gotoRel",i);
   inifile_set_guint32 ("gotoUnits",j);
   return FALSE;
}


static void goto_dlg_response (GtkDialog * dlg, int response, gpointer user_data)
{
   GotoDialog_data * gd = (GotoDialog_data *) user_data;
   if (response == GTK_RESPONSE_OK) {
      if (goto_dialog_apply (user_data) == TRUE) {
         g_signal_stop_emission_by_name (dlg, "response");
         return;
      }
   } else if (response == GTK_RESPONSE_APPLY) {
      goto_dialog_apply (user_data);
      g_signal_stop_emission_by_name (dlg, "response");
      return;
   }
   gtk_widget_destroy (GTK_WIDGET (dlg));
   g_free (gd);
}


GtkWidget * goto_dialog_new (Mainwindow *mw)
{
   GtkWidget * c, * frame, * dialog;
   GtkWidget * vbox, * hbox, * radio, * label;
   guint32 i;
   GotoDialog_data * gd = malloc (sizeof(GotoDialog_data));
   memset (gd, 0, sizeof(GotoDialog_data));
   gd->mw = mw;

   dialog = gtk_dialog_new ();
   gtk_window_set_title (GTK_WINDOW (dialog),_("Position cursor"));
   gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
   gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gd->mw));
   gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
   gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

   // vbox
   vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

   // vbox -> frame
   frame = gtk_frame_new (NULL);
   gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

   // vbox -> frame -> vbox
   vbox = gtk_vbox_new (FALSE, 0);
   gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
   gtk_container_add (GTK_CONTAINER (frame), vbox);

   // vbox -> frame -> vbox -> hbox
   hbox = gtk_hbox_new (FALSE,5);
   gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 3);
   label = gtk_label_new (_("Place cursor "));
   gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
   c = floatbox_new(inifile_get_gfloat("gotoOffset",0.0));
   gd->offset = FLOATBOX(c);
   gtk_box_pack_start (GTK_BOX (hbox),c, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label (NULL, _("seconds"));
   gd->unitbuttons[GOTODLG_UNIT_SECONDS] = GTK_TOGGLE_BUTTON(radio);
   gtk_box_pack_start (GTK_BOX (hbox), radio, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label_from_widget (
                     GTK_RADIO_BUTTON(radio), _("samples"));
   gd->unitbuttons[GOTODLG_UNIT_SAMPLES] = GTK_TOGGLE_BUTTON(radio);
   gtk_box_pack_start (GTK_BOX (hbox), radio, FALSE, FALSE, 0);

   // vbox -> frame -> vbox  <--
   radio = gtk_radio_button_new_with_label (NULL, _("after beginning of file"));
   gd->relbuttons[GOTODLG_POS_AFTER_BEG_FILE] = GTK_TOGGLE_BUTTON(radio);
   gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label_from_widget (
                     GTK_RADIO_BUTTON (radio), _("after end of file"));
   gd->relbuttons[GOTODLG_POS_AFTER_END_FILE] = GTK_TOGGLE_BUTTON (radio);
   gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label_from_widget (
                     GTK_RADIO_BUTTON (radio), _("after current cursor position"));
   gd->relbuttons[GOTODLG_POS_AFTER_CURSOR] = GTK_TOGGLE_BUTTON (radio);
   gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label_from_widget (
                     GTK_RADIO_BUTTON (radio), _("after selection start"));
   gd->relbuttons[GOTODLG_POS_AFTER_BEG_SEL] = GTK_TOGGLE_BUTTON (radio);
   gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);

   radio = gtk_radio_button_new_with_label_from_widget (
                     GTK_RADIO_BUTTON (radio), _("after selection end"));
   gd->relbuttons[GOTODLG_POS_AFTER_END_SEL] = GTK_TOGGLE_BUTTON (radio);
   gtk_box_pack_start (GTK_BOX (vbox), radio, FALSE, FALSE, 0);

   label = gtk_label_new(_("(use a negative number to place the cursor before "
                     "instead of after the selected point)"));
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
   gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

   i = inifile_get_guint32("gotoRel",0);
   if (i>4) i=0;
   gtk_toggle_button_set_active (gd->relbuttons[i],TRUE);

   i = inifile_get_guint32("gotoUnits",0);
   if (i>2) i=0;
   gtk_toggle_button_set_active (gd->unitbuttons[i],TRUE);

   // dialog buttons
   gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);
   gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Apply"), GTK_RESPONSE_APPLY);
   gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Close"), GTK_RESPONSE_CLOSE);
   g_signal_connect (dialog, "response", G_CALLBACK (goto_dlg_response), gd);

   // display dialog
   gtk_widget_show_all (GTK_WIDGET (dialog));

   return (dialog);
}

