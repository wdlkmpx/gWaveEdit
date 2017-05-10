/*
 * Copyright (C) 2002 2003 2004 2005 2006 2008 2011 2012, Magnus Hjorth
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "um.h"
#include "main.h"
#include "mainloop.h"
#include "gettext.h"

gboolean um_use_gtk = FALSE;

int user_message_flag=0;
static int modal_result;
static const int mr_yes=MR_YES,mr_no=MR_NO,mr_cancel=MR_CANCEL,mr_ok=MR_OK;

static void modal_callback(GtkWidget *widget, gpointer data)
{
     modal_result=*((int *)data);
}

/* Output each line of msg with "mhWaveEdit: " in front of it. */
void console_message(const char *msg)
{
     char *b,*c,*d;
     b = c = g_strdup(msg);
     while (1) {
	  fputs(_("mhWaveEdit: "),stderr);
	  d = strchr(c,'\n');
	  if (d != NULL) *d = 0;
	  fputs(c,stderr);
	  fputs("\n",stderr);
	  if (d != NULL) c = d+1;
	  else break;
     }
     g_free(b);
}

void console_perror(const char *msg)
{
     fprintf(stderr,_("mhWaveEdit: %s: %s\n"),msg,strerror(errno));
}

void user_perror(const char *msg){
     char *d;
     d = g_strdup_printf("%s: %s",msg,strerror(errno));
     user_error(d);
     g_free(d);
}

static gboolean responded = FALSE;
static gint r;

static void response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
     responded = TRUE;
     r = arg1;
}

static void nonblock_response(GtkDialog *dialog, gint arg1, gpointer user_data)
{
     if (arg1 != GTK_RESPONSE_DELETE_EVENT)
	  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static int showdlg(GtkMessageType mt, GtkButtonsType bt, char *msg, 
		   gboolean block)
{
     GtkWidget *w;
     g_assert(block || bt == GTK_BUTTONS_OK);
     if (!um_use_gtk) {
	  g_assert(bt == GTK_BUTTONS_OK);
	  console_message(msg);
	  return MR_OK;
     }
     w = gtk_message_dialog_new(NULL,GTK_DIALOG_MODAL,mt,bt,"%s",msg);
     if (bt == GTK_BUTTONS_NONE)
	  gtk_dialog_add_buttons(GTK_DIALOG(w),GTK_STOCK_YES,GTK_RESPONSE_YES,
				 GTK_STOCK_NO,GTK_RESPONSE_NO,GTK_STOCK_CANCEL,
				 GTK_RESPONSE_CANCEL,NULL);
     if (block) {
	  g_signal_connect(G_OBJECT(w),"response",
			     G_CALLBACK(response),NULL);
	  responded = FALSE;
     } else
	  g_signal_connect(G_OBJECT(w),"response",
			     G_CALLBACK(nonblock_response),NULL);
     gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER);
     gtk_widget_show(w);
     if (block)
	  while (!responded)
	       mainloop();
     else
	  return MR_OK;
     if (r != GTK_RESPONSE_DELETE_EVENT) gtk_widget_destroy(w);
     if (r == GTK_RESPONSE_OK) return MR_OK;
     if (r == GTK_RESPONSE_CANCEL) return MR_CANCEL;
     if (r == GTK_RESPONSE_YES) return MR_YES;
     if (r == GTK_RESPONSE_NO) return MR_NO;
     if (bt == GTK_BUTTONS_OK) return MR_OK;
     return MR_CANCEL;
}

int user_message(char *msg, int type)
{
     if (type == UM_YESNOCANCEL)
	  return showdlg(GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,msg,TRUE);
     if (type == UM_OKCANCEL)
	  return showdlg(GTK_MESSAGE_WARNING,GTK_BUTTONS_OK_CANCEL,msg,TRUE);
     return showdlg(GTK_MESSAGE_INFO,GTK_BUTTONS_OK,msg,TRUE);     
}

void user_info(char *msg)
{
     showdlg(GTK_MESSAGE_INFO,GTK_BUTTONS_OK,msg,TRUE);
}

void user_error(char *msg)
{
     showdlg(GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,msg,TRUE);
}

void popup_error(char *msg)
{
     showdlg(GTK_MESSAGE_ERROR,GTK_BUTTONS_OK,msg,FALSE);
}

void user_warning(char *msg)
{
     showdlg(GTK_MESSAGE_WARNING,GTK_BUTTONS_OK,msg,TRUE);
}


struct user_input_data {
     gboolean (*validator)(gchar *c);
     GtkWidget *entry,*window;
     gchar *result;
};

static gboolean user_input_quitflag;

static void user_input_destroy(void)
{
     user_input_quitflag=TRUE;
}

static void user_input_ok(GtkButton *button, gpointer user_data)
{
     gchar *c;
     struct user_input_data *uid = user_data;
     c = (gchar *)gtk_entry_get_text(GTK_ENTRY(uid->entry));
     if (uid->validator(c)) {
	  uid->result = g_strdup(c);
	  modal_result = MR_OK;
	  gtk_widget_destroy(uid->window);
     }
}

gchar *user_input(gchar *label, gchar *title, gchar *defvalue, 
		  gboolean (*validator)(gchar *c), GtkWindow *below)
{
     GtkWidget *a,*b,*c,*d,*ent;
     struct user_input_data uid;
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     if (below != NULL) {
	  gtk_window_set_transient_for(GTK_WINDOW(a),below);
	  gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER_ON_PARENT);
     } else
	  gtk_window_set_position(GTK_WINDOW(a),GTK_WIN_POS_CENTER);
     gtk_window_set_title(GTK_WINDOW(a),title?title:_("Input"));
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     g_signal_connect(G_OBJECT(a),"destroy",
			G_CALLBACK(user_input_destroy),NULL);
     b = gtk_vbox_new(FALSE,3);
     gtk_container_add(GTK_CONTAINER(a),b);
     c = gtk_hbox_new(FALSE,2);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     if (label) {
	  d = gtk_label_new(label);
	  gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     }
     ent = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(ent),defvalue);
     gtk_entry_set_activates_default(GTK_ENTRY(ent),TRUE);
     gtk_box_pack_start(GTK_BOX(b),ent,FALSE,FALSE,0);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,3);
     c = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_button_new_with_label(_("OK"));
     g_signal_connect(G_OBJECT(d),"clicked",
			G_CALLBACK(user_input_ok),&uid);
     gtk_container_add(GTK_CONTAINER(c),d);
     gtk_widget_set_can_default(d,TRUE);
     gtk_widget_grab_default(d);
     d = gtk_button_new_with_label(_("Cancel"));
     g_signal_connect(G_OBJECT(d),"clicked",G_CALLBACK(modal_callback),
			(gpointer)&mr_cancel);
     g_signal_connect_swapped(G_OBJECT(d),"clicked",
			       G_CALLBACK(gtk_widget_destroy), a);
     gtk_container_add(GTK_CONTAINER(c),d);
     gtk_widget_show_all(a);
     uid.window = a;
     uid.validator = validator;
     uid.entry = ent;
     modal_result = MR_CANCEL;
     user_input_quitflag = FALSE;
     while (!user_input_quitflag)
	  mainloop();
     if (modal_result == MR_CANCEL) return NULL;
     else return uid.result;
}

static gboolean user_input_float_validator(gchar *c)
{
     gchar *d;
     errno = 0;
     strtod(c,&d);
     return (errno == 0 && *d == 0);
}

gboolean user_input_float(gchar *label, gchar *title, gfloat defvalue, 
			  GtkWindow *below, gfloat *result)
{
     gchar *c,d[128],*e;
     format_float(defvalue, d, sizeof(d));
     c = user_input(label,title,d,user_input_float_validator,below);
     if (!c) return TRUE;
     *result = (gfloat)strtod(c,&e);
     g_assert(*e == 0);
     g_free(c);
     return FALSE;     
}

static gboolean echo_func(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
     return *((int *)user_data) == MR_OK;
}

static void user_choice_select(GtkTreeSelection *sel, gpointer user_data)
{
     GtkTreeModel *model;
     GtkTreeIter iter;
     gint *result = user_data;
     if (gtk_tree_selection_get_selected (sel, &model, &iter))
          gtk_tree_model_get (model, &iter, 1, result, -1);
}

gint user_choice(gchar **choices, guint def, gchar *windowtitle, 
		 gchar *windowtext, gboolean allow_cancel)
{
     GtkWidget *a,*b,*c,*d,*list=NULL,*w;
     guint i;
     GtkListStore *store;
     GtkTreeSelection *sel;
     GtkTreeIter iter;
     GtkTreeViewColumn *col;
     GtkCellRenderer *renderer;
     gint result;
     a = gtk_window_new(GTK_WINDOW_DIALOG);
     gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a), windowtitle?windowtitle:_("Choice"));
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     if (allow_cancel) 
	  g_signal_connect(G_OBJECT(a),"delete_event",
			     G_CALLBACK(echo_func),(gpointer)&mr_cancel);
     else
	  g_signal_connect(G_OBJECT(a),"delete_event",
			     G_CALLBACK(echo_func),(gpointer)&mr_ok);
     g_signal_connect(G_OBJECT(a),"destroy",
			G_CALLBACK(user_input_destroy),NULL);
     b = gtk_vbox_new(FALSE,5);
     gtk_container_add(GTK_CONTAINER(a),b);
     if (windowtext) {
	  c = gtk_label_new(windowtext);
	  gtk_label_set_line_wrap(GTK_LABEL(c),TRUE);
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
	  c = gtk_hseparator_new();
	  gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     }
     c = gtk_scrolled_window_new(NULL,NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(c),
				    GTK_POLICY_NEVER,
				    GTK_POLICY_AUTOMATIC);
     gtk_widget_set_size_request(GTK_WIDGET(c),-1,300);
     gtk_box_pack_start(GTK_BOX(b),c,TRUE,TRUE,0);
     d = list = gtk_tree_view_new();
     gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list),FALSE);
     store = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_INT);
     gtk_tree_view_set_model(GTK_TREE_VIEW(list),GTK_TREE_MODEL(store));
     renderer = gtk_cell_renderer_text_new();
     col = gtk_tree_view_column_new_with_attributes(NULL,renderer,"text",0,NULL);
     gtk_tree_view_append_column(GTK_TREE_VIEW(list),col);
     sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
     gtk_tree_selection_set_mode(sel,GTK_SELECTION_SINGLE);
     g_signal_connect(sel,"changed",G_CALLBACK(user_choice_select),&result);

     for (i=0; choices[i]!=NULL; i++) {
	  gtk_list_store_append(store,&iter);
	  gtk_list_store_set(store,&iter,0,choices[i],1,i,-1);
	  if (i == def) gtk_tree_selection_select_iter(sel,&iter);
     }
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(c),d);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     
     c = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     
     d = gtk_button_new_with_label(_("OK"));
     g_signal_connect(G_OBJECT(d),"clicked",G_CALLBACK(modal_callback),
			(gpointer)&mr_ok);
     g_signal_connect_swapped(G_OBJECT(d),"clicked",
			       G_CALLBACK(gtk_widget_destroy), a);
     gtk_container_add(GTK_CONTAINER(c),d);
     
     if (allow_cancel) {
	  d = gtk_button_new_with_label(_("Cancel"));
	  g_signal_connect(G_OBJECT(d),"clicked",
			     G_CALLBACK(modal_callback),
			     (gpointer)&mr_cancel);
	  g_signal_connect_swapped(G_OBJECT(d),"clicked",
				    G_CALLBACK(gtk_widget_destroy), a);
	  gtk_container_add(GTK_CONTAINER(c),d);
     }

     modal_result = MR_CANCEL;
     gtk_widget_show_all(a);

     user_input_quitflag = FALSE;
     while (!user_input_quitflag)
	  mainloop();
     
     g_assert(modal_result==MR_OK || allow_cancel);

     if (modal_result==MR_CANCEL) return -1;
     else return result;
}

