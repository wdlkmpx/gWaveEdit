/*
 * Copyright (C) 2002 2003 2004 2005 2011, Magnus Hjorth
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include "um.h"
#include "int_box.h"
#include "main.h"
#include "gettext.h"

G_DEFINE_TYPE(Intbox,intbox,GTK_TYPE_ENTRY)

enum {
     NUMCHANGED_SIGNAL,
     LAST_SIGNAL
};

static guint intbox_signals[LAST_SIGNAL] = { 0 };

static void intbox_update_text(Intbox *box)
{
     gchar e[30];
     g_snprintf(e,sizeof(e),"%ld",box->val);
     gtk_entry_set_text(GTK_ENTRY(box),e);
}

static void intbox_activate(GtkEntry *editable)
{
     long l;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(editable));
     l=strtol(c,&d,10);
     if (*d==0)
	  intbox_set(INTBOX(editable),l);
     else
	  intbox_update_text(INTBOX(editable));
     if (GTK_ENTRY_CLASS(intbox_parent_class)->activate)
          GTK_ENTRY_CLASS(intbox_parent_class)->activate(editable);
}

static gint intbox_focus_out(GtkWidget *widget, GdkEventFocus *event)
{
     long l;
     char *c,*d;
     Intbox *b = INTBOX(widget);
     c=(char *)gtk_entry_get_text(GTK_ENTRY(widget));
     l=strtol(c,&d,10);
     if (*d==0 && b->adj!=NULL && l>=(long)gtk_adjustment_get_lower(b->adj) &&
	 l<=(long)gtk_adjustment_get_upper(b->adj))
	  gtk_adjustment_set_value(b->adj,l);
     return GTK_WIDGET_CLASS(intbox_parent_class)->focus_out_event(widget,event);
}

static void intbox_class_init(IntboxClass *klass)
{
     GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
     GTK_ENTRY_CLASS(klass)->activate = intbox_activate;
     wc->focus_out_event = intbox_focus_out;
     klass->numchange=NULL;
     intbox_signals[NUMCHANGED_SIGNAL] = 
	  g_signal_new("numchanged", G_TYPE_FROM_CLASS(klass),
		       G_SIGNAL_RUN_FIRST,
		       G_STRUCT_OFFSET(IntboxClass,numchange),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__LONG,G_TYPE_NONE,1,
		       G_TYPE_LONG);
}

static void intbox_init(Intbox *fbox)
{
     gtk_entry_set_width_chars(GTK_ENTRY(fbox),10);
     fbox->adj = NULL;
}

void intbox_set(Intbox *box, long val)
{
if (box->val == val) return;
 if (box->adj != NULL && 
     val >= (long)gtk_adjustment_get_lower(box->adj) && 
     val <= (long)gtk_adjustment_get_upper(box->adj)) {
      gtk_adjustment_set_value(box->adj,(gfloat)val);
      return;
 } 
box->val=val;
 intbox_update_text(box); 
     g_signal_emit(G_OBJECT(box),intbox_signals[NUMCHANGED_SIGNAL],0,box->val);
}

GtkWidget *intbox_new(long val)
{
Intbox *box;
box=g_object_new(INTBOX_TYPE, NULL);
box->val=val-1;
intbox_set(box,val);
return GTK_WIDGET(box);
}

gboolean intbox_check(Intbox *box)
{
     long l;
     char *c,*d;
     c=(char *)gtk_entry_get_text(GTK_ENTRY(box));
     l=strtol(c,&d,10);
     if (*d==0) {
	  intbox_set(box,l);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("'%s' is not a number!"),c);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }
}

gboolean intbox_check_limit(Intbox *box, long int lowest, long int highest,
			    gchar *valuename)
{
     long l;
     char *c,*d;
     c = (char *)gtk_entry_get_text(GTK_ENTRY(box));
     l = strtol(c,&d,10);
     if (*d == 0 && l >= lowest && l <= highest) {
	  intbox_set(box,l);
	  return FALSE;
     } else {
	  d = g_strdup_printf(_("Value for %s must be a number between %ld and "
			      "%ld"),valuename,lowest,highest);
	  user_error(d);
	  g_free(d);
	  return TRUE;
     }
}

static void intbox_adj_changed(GtkAdjustment *adjustment, gpointer user_data)
{
     Intbox *box = INTBOX(user_data);
     box->val = box->adj->value;
     intbox_update_text(box);
     g_signal_emit(G_OBJECT(box),intbox_signals[NUMCHANGED_SIGNAL],0,
		     box->val);
}


GtkWidget *intbox_create_scale(Intbox *box, long minval, long maxval)
{
     GtkWidget *w;
     GtkRequisition req;

     if (box->adj == NULL) {	  
	  box->adj = GTK_ADJUSTMENT(gtk_adjustment_new(minval,minval,
						       maxval+1.0,
						       1.0,
						       10.0,
						       1.0));
	  g_signal_connect(G_OBJECT(box->adj),"value_changed",
			     G_CALLBACK(intbox_adj_changed),box);
	  gtk_adjustment_set_value(box->adj,box->val);
     }
     w = gtk_hscale_new(box->adj);
     gtk_scale_set_digits(GTK_SCALE(w),0);
     gtk_widget_size_request(w,&req);
     gtk_widget_set_size_request(w,req.width*5,req.height);
     return w;
}
