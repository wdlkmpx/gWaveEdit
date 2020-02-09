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

G_DEFINE_TYPE(GotoDialog,goto_dialog,GTK_TYPE_WINDOW)

static gboolean goto_dialog_apply(GotoDialog *gd)
{
     off_t p=0,q;
     float f;
     Document *d = gd->mw->doc;
     int i, j;
     if (d == NULL) return FALSE;
     if (floatbox_check(gd->offset)) return TRUE;
     for (i=0; i<5; i++)
	  if (gtk_toggle_button_get_active(gd->relbuttons[i]))
	       break;
     switch (i) {
     case GOTO_DIALOG_POS_AFTER_BEG_FILE:
      p = 0;
      break;
     case GOTO_DIALOG_POS_AFTER_END_FILE:
      p = d->chunk->length;
      break;
     case GOTO_DIALOG_POS_AFTER_CURSOR:
      p = d->cursorpos;
      break;
     case GOTO_DIALOG_POS_AFTER_BEG_SEL:
      p = (d->selstart==d->selend)?0:d->selstart;
      break;
     case GOTO_DIALOG_POS_AFTER_END_SEL: 
	  p = (d->selstart==d->selend)?d->chunk->length:d->selend; 
	  break;
     default:
      g_assert_not_reached();
     }

     for (j=0; j<2; j++)
          if (gtk_toggle_button_get_active(gd->unitbuttons[j]))
               break;
     f = gd->offset->val;
     if (j == GOTO_DIALOG_UNIT_SECONDS)
          f *= ((float)(d->chunk->format.samplerate));
     q = p+(off_t)f;

     if(q > d->chunk->length) {
          q = d->chunk->length;
     } else if(q < 0) {
          q = 0;
     }
     
     document_set_cursor(d,q);

     inifile_set_gfloat("gotoOffset",gd->offset->val);
     inifile_set_guint32("gotoRel",i);
     inifile_set_guint32("gotoUnits",j);
     return FALSE;
}

static void goto_dialog_ok(GtkButton *button, GotoDialog *gd)
{
     if (!goto_dialog_apply(gd))
	  gtk_widget_destroy(GTK_WIDGET(gd));
}

static void goto_dialog_init(GotoDialog *gd)
{
     GtkWidget *a,*b,*c;
     guint32 i;
     GtkAccelGroup* ag;

     ag = gtk_accel_group_new();

     a = gtk_vbox_new(FALSE,0);
     gtk_container_set_border_width(GTK_CONTAINER(a),8);
     gtk_container_add(GTK_CONTAINER(gd),a);
     b = gtk_hbox_new(FALSE,5);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,3);
     c = gtk_label_new(_("Place cursor "));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = floatbox_new(inifile_get_gfloat("gotoOffset",0.0));
     gd->offset = FLOATBOX(c);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = gtk_radio_button_new_with_label(NULL, _("seconds"));
     gd->unitbuttons[GOTO_DIALOG_UNIT_SECONDS] = GTK_TOGGLE_BUTTON(c);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     c = gtk_radio_button_new_with_label_from_widget(
          GTK_RADIO_BUTTON(c), _("samples"));
     gd->unitbuttons[GOTO_DIALOG_UNIT_SAMPLES] = GTK_TOGGLE_BUTTON(c);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);

     b = gtk_radio_button_new_with_label(NULL,_("after beginning of file"));
     gd->relbuttons[GOTO_DIALOG_POS_AFTER_BEG_FILE] = GTK_TOGGLE_BUTTON(b);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

     b = gtk_radio_button_new_with_label_from_widget(
	  GTK_RADIO_BUTTON(b),_("after end of file"));
     gd->relbuttons[GOTO_DIALOG_POS_AFTER_END_FILE] = GTK_TOGGLE_BUTTON(b);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

     b = gtk_radio_button_new_with_label_from_widget(
	  GTK_RADIO_BUTTON(b),_("after current cursor position"));
     gd->relbuttons[GOTO_DIALOG_POS_AFTER_CURSOR] = GTK_TOGGLE_BUTTON(b);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

     b = gtk_radio_button_new_with_label_from_widget(
	  GTK_RADIO_BUTTON(b),_("after selection start"));
     gd->relbuttons[GOTO_DIALOG_POS_AFTER_BEG_SEL] = GTK_TOGGLE_BUTTON(b);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

     b = gtk_radio_button_new_with_label_from_widget(
	  GTK_RADIO_BUTTON(b),_("after selection end"));
     gd->relbuttons[GOTO_DIALOG_POS_AFTER_END_SEL] = GTK_TOGGLE_BUTTON(b);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);

     b = gtk_label_new(_("(use a negative number to place the cursor before "
		       "instead of after the selected point)"));
     gtk_label_set_justify(GTK_LABEL(b),GTK_JUSTIFY_LEFT);
     gtk_label_set_line_wrap(GTK_LABEL(b),TRUE);
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     b = gtk_hbutton_box_new();
     gtk_button_box_set_layout(GTK_BUTTON_BOX(b),GTK_BUTTONBOX_END);
     gtk_box_pack_end(GTK_BOX(a),b,FALSE,FALSE,0);
     c = gtk_button_new_with_label(_("OK"));
     g_signal_connect(G_OBJECT(c),"clicked",
			G_CALLBACK(goto_dialog_ok),gd);
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_KP_Enter, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Return, 0, 
				 (GtkAccelFlags) 0);
     c = gtk_button_new_with_label(_("Apply"));
     g_signal_connect_swapped(G_OBJECT(c),"clicked",
			       G_CALLBACK(goto_dialog_apply), gd);
     gtk_container_add(GTK_CONTAINER(b),c);
     c = gtk_button_new_with_label(_("Close"));
     g_signal_connect_swapped(G_OBJECT(c),"clicked",
			       G_CALLBACK(gtk_widget_destroy), gd);
     gtk_container_add(GTK_CONTAINER(b),c);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Escape, 0, 
				 (GtkAccelFlags) 0);
     b = gtk_hseparator_new();
     gtk_box_pack_end(GTK_BOX(a),b,FALSE,FALSE,0);

     i = inifile_get_guint32("gotoRel",0);
     if (i>4) i=0;
     gtk_toggle_button_set_active(gd->relbuttons[i],TRUE);

     i = inifile_get_guint32("gotoUnits",0);
     if (i>2) i=0;
     gtk_toggle_button_set_active(gd->unitbuttons[i],TRUE);

     gtk_widget_show_all(a);
     gtk_window_set_title(GTK_WINDOW(gd),_("Position cursor"));
     gtk_window_add_accel_group(GTK_WINDOW (gd), ag);
     gtk_window_set_position (GTK_WINDOW (gd), GTK_WIN_POS_CENTER);
}

static void goto_dialog_class_init(GotoDialogClass *klass)
{
}

GtkWidget *goto_dialog_new(Mainwindow *mw)
{     
     GtkWidget *w;
     w = g_object_new(GOTO_DIALOG_TYPE, NULL);
     GOTO_DIALOG(w)->mw = mw;
     return w;
}
