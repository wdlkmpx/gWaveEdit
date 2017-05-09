/*
 * Copyright (C) 2002 2003 2004 2005 2006 2007 2008 2009 2010, Magnus Hjorth
 * Copyright (C) 2011 2012 2013 2018, Magnus Hjorth
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

#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gtkfiles.h"
#include "filetypes.h"
#include "chunkview.h"
#include "mainwindow.h"
#include "recorddialog.h"
#include "configdialog.h"
#include "gotodialog.h"
#include "chunk.h"
#include "um.h"
#include "main.h"
#include "inifile.h"
#include "player.h"
#include "sound.h"
#include "effectbrowser.h"
#include "soxdialog.h"
#include "help.h"
#include "ladspadialog.h"
#include "gettext.h"
#include "bitmap.h"

#include "button_open.xpm"
#include "button_save.xpm"
#include "button_undo.xpm"
#include "button_redo.xpm"
#include "button_cut.xpm"
#include "button_copy.xpm"
#include "button_paste.xpm"
#include "button_pasteover.xpm"
#include "button_delete.xpm"
#include "button_cursorstart.xpm"
#include "button_cursorend.xpm"
#include "button_play.xpm"
#include "button_playselection.xpm"
#include "button_stop.xpm"
#include "button_loop.xpm"
#include "button_follow.xpm"
#include "button_record.xpm"
#include "button_mixer.xpm"
#include "button_bounce.xpm"
#include "icon.xpm"
#include "vzoom.xbm"
#include "hzoom.xbm"
#include "speed.xbm"

/* #define SHOW_DEBUG_MENU */

#ifdef FIXED_DATE
#define BUILD_DATE FIXED_DATE
#else
#define BUILD_DATE __DATE__
#endif

#ifdef FIXED_TIME
#define BUILD_TIME FIXED_TIME
#else
#define BUILD_TIME __TIME__
#endif

G_DEFINE_TYPE(Mainwindow,mainwindow,GTK_TYPE_WINDOW)

ListObject *mainwindow_objects = NULL;

static gboolean window_geometry_stack_inited = FALSE;
static GSList *window_geometry_stack = NULL;
static GList *recent_filenames = NULL;
static Chunk *clipboard = NULL;
gboolean autoplay_mark_flag = FALSE;
gboolean varispeed_reset_flag = FALSE;

static Mainwindow *mainwindow_set_document(Mainwindow *w, Document *doc,
					   gchar *filename);
static void mainwindow_view_changed(Document *d, gpointer user_data);
static void mainwindow_selection_changed(Document *d, gpointer user_data);
static void mainwindow_cursor_changed(Document *d, gboolean rolling, 
				      gpointer user_data);
static void mainwindow_state_changed(Document *d, gpointer user_data);



static void load_recent(void)
{
     guint i;
     gchar *c,*d;
     g_assert(recent_filenames == NULL);
     for (i=1; ; i++) {
	  c = g_strdup_printf("recentFile%d",i);
	  d = inifile_get(c,NULL);
	  g_free(c);
	  if (d == NULL) break;
	  recent_filenames = g_list_append(recent_filenames, g_strdup(d));
     }
}

static void save_recent(void)
{
     guint i,j;
     GList *l;
     gchar *c;
     j = inifile_get_guint32("recentFiles",4);
     if (j > MAINWINDOW_RECENT_MAX) j=4;
     for (i=1,l=recent_filenames; i<=j && l!=NULL; i++,l=l->next) {
	  c = g_strdup_printf("recentFile%d",i);
	  inifile_set(c,l->data);
	  g_free(c);
     }
}

static void update_file_recent(Mainwindow *w)
{
     guint i = 1;
     GList *l = recent_filenames;
     GList *m = w->recent;
     gchar *c,*d;
     for (; m!=NULL && l!=NULL; m=m->next,l=l->next,i++) {
	  d = namepart((gchar *)l->data);
	  c = g_strdup_printf("%d. %s",i,d);
	  gtk_label_set_text(GTK_LABEL(GTK_BIN(m->data)->child),c);
	  gtk_widget_set_sensitive(GTK_WIDGET(m->data),TRUE);
	  g_free(c);
     }
     for (; m!=NULL; m=m->next,i++) {
	  c = g_strdup_printf("%d.",i);
	  gtk_label_set_text(GTK_LABEL(GTK_BIN(m->data)->child),c);
	  gtk_widget_set_sensitive(GTK_WIDGET(m->data),FALSE);
	  g_free(c);
     }
}

static void recent_file(gchar *filename)
{
     GList *l = recent_filenames;
     /* Remove other reference to this file, if it exists */
     while (l!=NULL) {
	  if (!strcmp((gchar *)l->data,filename)) {
	       recent_filenames = g_list_remove_link(recent_filenames,l);
	       g_free(l->data);
	       g_list_free_1(l);
	       break;
	  } else
	       l = l->next;
     }
     /* Put file at top */
     recent_filenames = g_list_prepend(recent_filenames,g_strdup(filename));
     list_object_foreach(mainwindow_objects,(GFunc)update_file_recent,NULL);
     save_recent();
}

static void update_desc(Mainwindow *w);

static void set_sensitive(GList *l, gboolean s)
{
     while (l != NULL) {
	  gtk_widget_set_sensitive(GTK_WIDGET(l->data),s);
	  l = l->next;
     }
}

/* This function makes the window un-sensitive and brings it to front.*/
static void procstart(StatusBar *bar, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     mainwindow_set_sensitive(w,FALSE);
     w->esc_pressed_flag = FALSE;
     gtk_window_present(GTK_WINDOW(w));
     gtk_grab_add(GTK_WIDGET(w));
}

static void procend(StatusBar *bar, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     mainwindow_set_sensitive(w,TRUE);
     mainwindow_update_texts();
     gtk_grab_remove(GTK_WIDGET(w));
}

static void fix_title(Mainwindow *wnd)
{
     gchar *c;
     if (wnd->doc != NULL) {
	  c = g_strdup_printf ( _("mhWaveEdit: %s (%s): %d Hz, %s"),
				wnd->doc->titlename,
				chunk_get_time(wnd->doc->chunk,
					       wnd->doc->chunk->length,
					       NULL),
				wnd->doc->chunk->format.samplerate,
				sampletype_name(&(wnd->doc->chunk->format)));
     } else 
	  c = g_strdup(PROGRAM_VERSION_STRING);
     gtk_window_set_title ( GTK_WINDOW ( wnd ), c );
     g_free ( c );
}

static void update_desc(Mainwindow *w)
{
     if (w->doc != NULL)
	  status_bar_set_info(w->statusbar,w->doc->cursorpos,
			      (playing_document==w->doc),w->doc->viewstart,
			      w->doc->viewend,w->doc->selstart,
			      w->doc->selend,
			      w->doc->chunk->format.samplerate,
			      w->doc->chunk->length);
     else
	  status_bar_reset(w->statusbar);
			      
}

static void typesel_changed(Combo *obj, GtkWidget *user_data)
{
     int i;
     gchar *c;
     i = combo_selected_index(obj);
     if (i == 0) {
	  gtk_widget_set_sensitive(user_data,TRUE);
     } else {
	  c = fileformat_extension(i-1);
	  get_filename_modify_extension(c);
	  if (fileformat_has_options(i-1)) {
	       gtk_widget_set_sensitive(user_data,TRUE);
	  } else {
	       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(user_data),TRUE);
	       gtk_widget_set_sensitive(user_data,FALSE);
	  }
     }
}

static void mainwindow_open(Mainwindow *w, gchar *filename)
{
     Document *doc;
     doc = document_new_with_file ( filename, w->statusbar );
     if (doc == NULL) return;
     mainwindow_set_document ( w, doc, filename );
     inifile_set("lastOpenFile",filename);
     recent_file(filename);
}

static gchar *get_save_filename(gchar *old_filename, gchar *title_text, 
				gint *type_id, gboolean *use_defaults)
{
     gchar *lsf,*lsd,*filename;
     gchar *c,*d,*e;
     GtkBox *b1,*b2;
     GtkWidget *w,*usedef,*typesel;
     GList *l;
     gint i;

     /* Get the lastSaveFile entry */
     lsf = inifile_get("lastSaveFile",NULL);
     /* If we didn't have a lastSaveFile entry, assume we want to
	save into the same directory as we loaded from. */
     if (lsf == NULL) lsf = inifile_get("lastOpenFile",NULL);
     /* Get the directory part and copy into lsd */
     if (lsf != NULL) {
	  lsd = g_strdup(lsf);
	  c = strrchr(lsd,'/');
	  if (c != NULL) c[1]=0;
	  else {
	       g_free(lsd);
	       lsd = NULL;
	  }
     } else lsd = NULL;

     /* Create the custom widget */
     usedef = gtk_check_button_new_with_label(_("Use default settings"));
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedef),TRUE);
     typesel = combo_new();
     
     l = g_list_append(NULL,g_strdup(_("Auto-detect from extension")));
     for (i=0; ; i++) {
	  c = fileformat_name(i);
	  if (c == NULL) break;
	  d = fileformat_extension(i);
	  e = g_strdup_printf("%s (%s)",c,d);
	  l = g_list_append(l,e);
     }
     combo_set_items(COMBO(typesel),l,0);
     g_signal_connect(G_OBJECT(typesel),"selection-changed",
			G_CALLBACK(typesel_changed),usedef);
     g_list_foreach(l,(GFunc)g_free,NULL);
     g_list_free(l);

     b1 = GTK_BOX(gtk_hbox_new(FALSE,0));
     w = gtk_label_new(_("File type: "));
     gtk_box_pack_start(b1,w,FALSE,FALSE,0);
     gtk_box_pack_start(b1,typesel,TRUE,TRUE,0);
     gtk_box_pack_start(b1,usedef,FALSE,FALSE,6);
     b2 = GTK_BOX(gtk_vbox_new(FALSE,8));
     gtk_box_pack_start(b2,GTK_WIDGET(b1),FALSE,FALSE,0);

     gtk_object_ref(GTK_OBJECT(usedef));
     gtk_object_ref(GTK_OBJECT(typesel));

     gtk_widget_show_all(GTK_WIDGET(b2));

     if (lsd == NULL)
	  filename = get_filename ( old_filename, "*.wav", title_text, 
				    TRUE, GTK_WIDGET(b2));
     else if (old_filename == NULL)
	  filename = get_filename (lsd, "*.wav", title_text, TRUE, 
				   GTK_WIDGET(b2));
     else { 
	  c = strrchr(old_filename, '/');
	  if (!c) c=old_filename;
	  else c = c+1;
	  d = g_strjoin("/",lsd,c,NULL);
	  filename = get_filename(d, "*.wav", title_text, TRUE, 
				  GTK_WIDGET(b2));
	  g_free(d);
     }

     *use_defaults = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedef));
     *type_id = combo_selected_index(COMBO(typesel)) - 1;

     g_free(lsd);
     gtk_object_unref(GTK_OBJECT(typesel));
     gtk_object_unref(GTK_OBJECT(usedef));
     return filename;
     
}

static gboolean mainwindow_save ( Mainwindow *w, gchar *filename )
{
     int type_id = -1;
     gboolean use_defs = TRUE;
     gboolean r;
     gboolean fmal = FALSE; /* TRUE if filename should be g_free:d */    

     if (!document_can_undo(w->doc) && w->doc->filename != NULL)
	  if (user_message(_("The file has not changed since last save. Press OK"
			   " if you want to save it anyway?"),UM_OKCANCEL) == 
	      MR_CANCEL) 
	       return FALSE;

     /* If there is no filename provided, get a filename from a dialog
      * box. Also set the fmal flag so we remember to free the
      * filename later. */  
     if (filename == NULL) {
	  filename = get_save_filename(w->doc->filename,_("Save File"),
				       &type_id,&use_defs);
	  if (filename == NULL) return TRUE;
	  fmal = TRUE;
     }

     g_assert(filename != NULL);
     /* Save the file */
     r = document_save(w->doc, filename, type_id, use_defs);
     if (r) { if (fmal) g_free(filename); return TRUE; }

     inifile_set("lastSaveFile",filename);
     recent_file(filename);
     if (fmal) g_free(filename);

     return FALSE;
}

static gboolean change_check ( Mainwindow *w )
{
     gint i;
     gchar *c;
     if (w->doc == NULL || 
	 (w->doc->filename != NULL && !document_can_undo(w->doc))) 
	  return FALSE;
     c = g_strdup_printf( _("Save changes to %s?"),
			  w->doc->titlename );
     i = user_message ( c, UM_YESNOCANCEL );
     switch (i) {
     case MR_YES:
	  return mainwindow_save ( w, w->doc->filename );
     case MR_NO:
	  return FALSE;
     case MR_CANCEL:
	  return TRUE;
     }
     g_assert_not_reached();
     return TRUE;
}

static void mainwindow_destroy(GtkObject *obj)
{
     Mainwindow *w = MAINWINDOW(obj);

     list_object_remove(mainwindow_objects, obj);

     if ( w->doc != NULL ) {
	  gtk_signal_disconnect_by_data(GTK_OBJECT(w->doc),obj);
	  gtk_object_unref(GTK_OBJECT(w->doc));
	  w->doc = NULL;
     }

     if (list_object_get_size(mainwindow_objects) == 0) {
	  if (clipboard) gtk_object_unref ( GTK_OBJECT(clipboard) );
	  clipboard = NULL;
	  quitflag = TRUE;
	  geometry_stack_save_to_inifile("windowGeometry",
					 window_geometry_stack);
     }

     GTK_OBJECT_CLASS(mainwindow_parent_class)->destroy(obj);
}

static gint mainwindow_delete_event(GtkWidget *widget, GdkEventAny *event)
{
     Mainwindow *w = MAINWINDOW ( widget );
     GtkWidgetClass *pcw = GTK_WIDGET_CLASS(mainwindow_parent_class);
     if (change_check(w)) return TRUE;
     if (playing_document == w->doc) player_stop();
     geometry_stack_push(GTK_WINDOW(w),NULL,&window_geometry_stack);
     if (pcw->delete_event) return pcw->delete_event(widget,event);
     else return FALSE;
}

static void mainwindow_realize(GtkWidget *widget)
{
     if (GTK_WIDGET_CLASS(mainwindow_parent_class)->realize) 
	  GTK_WIDGET_CLASS(mainwindow_parent_class)->realize(widget);
     if (!icon) icon = gdk_pixbuf_new_from_xpm_data (icon_xpm);
     gtk_window_set_icon(GTK_WINDOW(widget),icon);

}

static void mainwindow_toggle_mark(Mainwindow *w, gchar *label)
{
     off_t u;
     u = document_get_mark(w->doc,label);
     if (u == w->doc->cursorpos) 
	  document_set_mark(w->doc,label,DOCUMENT_BAD_MARK);
     else
	  document_set_mark(w->doc,label,w->doc->cursorpos);
}

static void do_play(Mainwindow *w, off_t start, off_t end, gboolean loop)
{
     gboolean speed_reset = FALSE;
     if (varispeed_reset_flag && 
	 !(player_playing() && playing_document == w->doc))
	  speed_reset = TRUE;
     if (speed_reset) document_stop(w->doc,FALSE);
#ifdef INV_SPEED
     if (speed_reset) gtk_adjustment_set_value(w->speed_adj,-1.0);
     document_play(w->doc, start, end, loop, -w->speed_adj->value);     
#else
     if (speed_reset) gtk_adjustment_set_value(w->speed_adj,1.0);
     document_play(w->doc, start, end, loop, w->speed_adj->value);
#endif
     update_desc(w);
}


static void mainwindow_goto_mark(Mainwindow *w, gchar *label)
{
     off_t u;
     u = document_get_mark(w->doc,label);
     if (u != DOCUMENT_BAD_MARK) {
	  if (autoplay_mark_flag)
	       do_play(w,u,w->doc->chunk->length,w->loopmode);
	  else
	       document_set_cursor(w->doc, u);
     }
}

static void view_zoomin(GtkMenuItem *menu_item, gpointer user_data);
static void view_zoomout(GtkMenuItem *menu_item, gpointer user_data);
static void view_zoomtoselection(GtkMenuItem *menu_item, gpointer user_data);
static void view_zoomall(GtkMenuItem *menu_item, gpointer user_data);
static void edit_play(GtkMenuItem *menu_item, gpointer user_data);
static void edit_playselection(GtkMenuItem *menu_item, gpointer user_data);
static void edit_stop(GtkMenuItem *menu_item, gpointer user_data);

static gint mainwindow_keypress(GtkWidget *widget, GdkEventKey *event)
{     
     Mainwindow *w = MAINWINDOW(widget);
     off_t o;
     /* printf("%d\n",event->keyval); */
     if (!w->sensitive) {
	  if (event->keyval == GDK_Escape) 
	       status_bar_break_progress(w->statusbar);
	  return TRUE;
     }
     if (w->doc == NULL) 
          return GTK_WIDGET_CLASS(mainwindow_parent_class)->key_press_event(widget,event);
     if ((event->state & GDK_CONTROL_MASK))
	  switch (event->keyval) {
	  case GDK_0: mainwindow_toggle_mark(w,"0"); return TRUE;
	  case GDK_1: mainwindow_toggle_mark(w,"1"); return TRUE;
	  case GDK_2: mainwindow_toggle_mark(w,"2"); return TRUE;
	  case GDK_3: mainwindow_toggle_mark(w,"3"); return TRUE;
	  case GDK_4: mainwindow_toggle_mark(w,"4"); return TRUE;
	  case GDK_5: mainwindow_toggle_mark(w,"5"); return TRUE;
	  case GDK_6: mainwindow_toggle_mark(w,"6"); return TRUE;
	  case GDK_7: mainwindow_toggle_mark(w,"7"); return TRUE;
	  case GDK_8: mainwindow_toggle_mark(w,"8"); return TRUE;
	  case GDK_9: mainwindow_toggle_mark(w,"9"); return TRUE;
	  case GDK_Left:
	  case GDK_KP_Left: 
	       if (playing_document == w->doc) 
		    player_nudge(-0.5); 
	       else {
		    o = w->doc->cursorpos - w->doc->chunk->format.samplerate/2;
		    if (o < 0) o = 0;
		    document_set_cursor(w->doc,o);
	       }
	       return TRUE;
	  case GDK_Right:
	  case GDK_KP_Right:
	       if (playing_document == w->doc) 
		    player_nudge(0.5); 
	       else {
		    o = w->doc->cursorpos + w->doc->chunk->format.samplerate/2;
		    if (o > w->doc->chunk->length) o = w->doc->chunk->length;
		    document_set_cursor(w->doc,o);
	       }
	       return TRUE;
	  case GDK_Tab:
	       o = (w->doc->viewstart + w->doc->viewend) / 2;
	       document_set_cursor(w->doc,o);
	  }
     else 
	  switch (event->keyval) {	       
	  case GDK_0: mainwindow_goto_mark(w,"0"); return TRUE;
	  case GDK_1: mainwindow_goto_mark(w,"1"); return TRUE;
	  case GDK_2: mainwindow_goto_mark(w,"2"); return TRUE;
	  case GDK_3: mainwindow_goto_mark(w,"3"); return TRUE;
	  case GDK_4: mainwindow_goto_mark(w,"4"); return TRUE;
	  case GDK_5: mainwindow_goto_mark(w,"5"); return TRUE;
	  case GDK_6: mainwindow_goto_mark(w,"6"); return TRUE;
	  case GDK_7: mainwindow_goto_mark(w,"7"); return TRUE;
	  case GDK_8: mainwindow_goto_mark(w,"8"); return TRUE;
	  case GDK_9: mainwindow_goto_mark(w,"9"); return TRUE;
	  case GDK_plus:
	  case GDK_KP_Add:
	  case GDK_equal:
	       view_zoomin(NULL,widget); return TRUE;
	  case GDK_Down:
	  case GDK_KP_Down:
	       document_zoom(w->doc,2.0,FALSE); return TRUE;
	  case GDK_minus:
	  case GDK_KP_Subtract: 
	       view_zoomout(NULL,widget); return TRUE;
	  case GDK_Up:
	  case GDK_KP_Up:
	       document_zoom(w->doc,0.5,FALSE); return TRUE;
	  case GDK_greater: view_zoomtoselection(NULL,widget); return TRUE;
	  case GDK_less: view_zoomall(NULL,widget); return TRUE;
	  case GDK_comma: edit_play(NULL,widget); return TRUE;
	  case GDK_period: edit_stop(NULL,widget); return TRUE;
	  case GDK_slash: edit_playselection(NULL, widget); return TRUE;
	  case GDK_space:
	       if ((event->state & GDK_SHIFT_MASK) != 0)
		    do_play(w,0,w->doc->chunk->length,w->loopmode);
	       else if (playing_document == w->doc) 
		    document_stop(w->doc,w->bouncemode);
	       else if (w->doc != NULL)
		    do_play(w,w->doc->cursorpos,w->doc->chunk->length,
			    w->loopmode);
	       return TRUE;	       
	  case GDK_Left:
	  case GDK_KP_Left:
	       if (playing_document==w->doc && w->doc->followmode)
		    player_nudge(-0.5);
	       else
		    document_scroll(w->doc,
				    -(w->doc->viewend - w->doc->viewstart)/MAINWINDOW_SCROLL_DELTA_RATIO);
	       return TRUE;
	  case GDK_Right:
	  case GDK_KP_Right:
	       if (playing_document==w->doc && w->doc->followmode)
		    player_nudge(+0.5);
	       else
		    document_scroll(w->doc,
				    (w->doc->viewend - w->doc->viewstart)/MAINWINDOW_SCROLL_DELTA_RATIO);
	       return TRUE;
	  case GDK_parenleft:
	       o = 3*w->doc->chunk->format.samplerate;
	       if (w->doc->selstart == w->doc->selend) {
		    if (w->doc->chunk->length < o)
			 do_play(w,0,w->doc->chunk->length,FALSE);
		    else
			 do_play(w,0,o,FALSE);
	       } else if (w->doc->selstart+o > w->doc->selend)
		    do_play(w,w->doc->selstart,w->doc->selend,FALSE);
	       else
		    do_play(w,w->doc->selstart,w->doc->selstart+o,FALSE);
	       return TRUE;		    
	  case GDK_parenright:
	       o = 3*w->doc->chunk->format.samplerate;
	       if (w->doc->selstart == w->doc->selend) {
		    if (w->doc->chunk->length < o)
			 do_play(w,0,w->doc->chunk->length,FALSE);
		    else
			 do_play(w,w->doc->chunk->length-o,
				 w->doc->chunk->length,FALSE);
	       } else if (w->doc->selstart+o > w->doc->selend)
		    do_play(w,w->doc->selstart,w->doc->selend,FALSE);
	       else
		    do_play(w,w->doc->selend-o,w->doc->selend,FALSE);
	       return TRUE;		    
	  case GDK_Home:
	       document_set_view(w->doc,0,w->doc->viewend-w->doc->viewstart);
	       if (playing_document != w->doc || w->doc->followmode)
		    document_set_cursor(w->doc,0);
	       return TRUE;
	  case GDK_End:
	       /* Stop playback to prevent cursor jumping back to start point */
	       document_stop(w->doc,FALSE);
	       document_scroll(w->doc,w->doc->chunk->length);
	       if (playing_document != w->doc || w->doc->followmode)
		    document_set_cursor(w->doc,w->doc->chunk->length);
	       return TRUE;
	  case GDK_Tab:
	       document_scroll(w->doc,
			       w->doc->cursorpos-
			       (w->doc->viewend+w->doc->viewstart)/2);
	       return TRUE;
	  }
     return GTK_WIDGET_CLASS(mainwindow_parent_class)->key_press_event(widget,event);
}

static void urldecode(char *str)
{
     char *s,*d;
     int i,j;
     s = d = str;
     while (*s != 0) {
	  if (*s != '%' || s[1] == 0 || s[2] == 0) {
	       *d = *s;
	       d ++;
	       s ++;
	  } else {
	       i = hexval(s[1]);
	       j = hexval(s[2]);
	       if (i < 0 || j < 0) { 
		    *d = *s;
		    d ++;
		    s ++;
		    continue;
	       } 
	       *d = (char)(i*16+j);
	       d ++;
	       s += 3;
	  }
	  
     }
     *d = 0;
}

static void mainwindow_drag_data_received(GtkWidget *widget, 
					  GdkDragContext *dc, gint x, gint y, 
					  GtkSelectionData *selection_data, 
					  guint info, guint t)
{
     gchar *c,*d,*e,*f;
     if (selection_data->length > 0) {
	  
	  /* Copy and add extra null for safety */
	  c = g_malloc(selection_data->length+1);
	  memcpy(c,selection_data->data,selection_data->length);
	  c[selection_data->length] = 0;

	  gtk_drag_finish(dc, TRUE, FALSE, t);
	  
	  /* Split into files */
	  for (d=strtok(c,"\n\r"); d!=NULL; d=strtok(NULL,"\n\r")) {
	       e = strchr(d,':');
	       if (e == NULL) continue;
	       *e = 0;
	       if (strcmp(d,"file")) continue;
	       while (e[1] == '/') e++;
	       
	       urldecode(e);
	       
	       if (!file_exists(e)) {
		    f = g_strdup_printf("Silently ignoring non-esisting "
					"file: %s\n",e);
		    console_message(f);
		    g_free(f);
		    continue;
	       }

	       mainwindow_open(MAINWINDOW(widget),e);
	       /* printf("Dropped: %s\n",e); */
	       
	  }

	  g_free(c);

     } else

	  gtk_drag_finish(dc, FALSE, FALSE, t);
}

static void mainwindow_class_init(MainwindowClass *klass)
{
     GtkObjectClass *oc = GTK_OBJECT_CLASS(klass);
     GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
     oc->destroy = mainwindow_destroy;
     wc->delete_event = mainwindow_delete_event;
     wc->realize = mainwindow_realize;
     wc->key_press_event = mainwindow_keypress;
     wc->drag_data_received = mainwindow_drag_data_received;
}

static Mainwindow *mainwindow_set_document(Mainwindow *w, Document *d,
				    gchar *filename)
{
     if (w->doc != NULL) {
	  w = MAINWINDOW ( mainwindow_new() );
	  gtk_widget_show ( GTK_WIDGET ( w ) );
     }
     w->doc = d;
     document_set_followmode(d,w->followmode);
     gtk_object_ref(GTK_OBJECT(w->doc));
     gtk_object_sink(GTK_OBJECT(w->doc));
     g_signal_connect(G_OBJECT(d),"view_changed",
			G_CALLBACK(mainwindow_view_changed),w);
     g_signal_connect(G_OBJECT(d),"selection_changed",
			G_CALLBACK(mainwindow_selection_changed),w);
     g_signal_connect(G_OBJECT(d),"cursor_changed",
			G_CALLBACK(mainwindow_cursor_changed),w);
     g_signal_connect(G_OBJECT(d),"state_changed",
			G_CALLBACK(mainwindow_state_changed),w);
     chunk_view_set_document ( w->view, d );
     document_set_status_bar(d, w->statusbar);
     fix_title(w);
     update_desc(w);
     set_sensitive(w->need_chunk_items,TRUE);
     if (!inifile_get_gboolean("varispeed",TRUE))
	  gtk_widget_set_sensitive(w->speed_slider,FALSE);
     list_object_add(mainwindow_objects,w);
     mainwindow_view_changed(w->doc,w);
     return w;
}

static Mainwindow *mainwindow_set_chunk(Mainwindow *w, Chunk *c, gchar *filename)
{
     Document *d;
     d = document_new_with_chunk(c,filename,w->statusbar);
     return mainwindow_set_document(w,d,filename);
}

void mainwindow_set_speed_sensitive(gboolean sensitive)
{
     GList *l;
     Mainwindow *w;
     for (l=mainwindow_objects->list;l!=NULL;l=l->next) {
	  w = MAINWINDOW(l->data);
	  gtk_widget_set_sensitive(w->speed_slider,sensitive);
     }
}

static void file_open(GtkMenuItem *menuitem, gpointer user_data)
{
     gchar *c;
     Mainwindow *w = MAINWINDOW ( user_data );     
     if (w->doc != NULL && w->doc->filename != NULL)
	  c = get_filename(w->doc->filename,"*.wav",_("Load File"),FALSE,NULL);
     else {
	  c = inifile_get("lastOpenFile",NULL);
	  if (c == NULL) c = inifile_get("lastSaveFile",NULL);
	  c = get_filename(c,"*.wav", _("Load File"), FALSE, NULL);
     }
     if (!c) return;
     mainwindow_open(w,c);
     g_free(c);
}

static void file_save(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     mainwindow_save ( w, w->doc->filename );
}

static void file_saveas(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_save ( MAINWINDOW(user_data), NULL );
}

static void file_saveselection(GtkMenuItem *menuitem, gpointer user_data)
{
     gint type_id;
     gboolean use_defs;
     gchar *fn;
     Chunk *c;
     Mainwindow *w = MAINWINDOW(user_data);
     fn = get_save_filename(NULL,_("Save selection as ..."),
			    &type_id,&use_defs);
     if (!fn) return;
     c = chunk_get_part(w->doc->chunk,w->doc->selstart,
			w->doc->selend-w->doc->selstart);
     if (!chunk_save(c,fn,type_id,use_defs,dither_editing,w->statusbar))
	  inifile_set("lastSaveFile",fn);
     gtk_object_sink(GTK_OBJECT(c));     
     g_free(fn);
}


static void file_close(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (change_check(w)) return;
     if (playing_document == w->doc) player_stop();
     if (list_object_get_size(mainwindow_objects)==1 &&
	 w->doc != NULL) {
	  chunk_view_set_document(w->view, NULL);
	  gtk_signal_disconnect_by_data(GTK_OBJECT(w->doc),w);
	  gtk_object_unref(GTK_OBJECT(w->doc));
	  w->doc = NULL;
	  fix_title(w);
	  set_sensitive(w->need_chunk_items,FALSE);
	  set_sensitive(w->need_undo_items,FALSE);
	  set_sensitive(w->need_selection_items,FALSE);
	  list_object_remove(mainwindow_objects,w);
     } else {
	  geometry_stack_push(GTK_WINDOW(w),NULL,&window_geometry_stack);
	  gtk_widget_destroy(GTK_WIDGET(w));
     }
}

static void try_close(Mainwindow *w, gboolean *user_data)
{
     if (*user_data) return;
     *user_data = change_check(w);
}

static void file_exit(GtkMenuItem *menuitem, gpointer user_data)
{
     gboolean b = FALSE;
     guint i;
     GtkWidget *wid;
     if (list_object_get_size(mainwindow_objects) == 0)
	  file_close(menuitem,user_data);
     list_object_foreach(mainwindow_objects,(GFunc)try_close,&b);
     if (b) return;
     player_stop();
     /* Reverse the list so we get the oldest windows on the top of the 
      * geometry stack */
     i = list_object_get_size(mainwindow_objects);
     for (; i>0; i--) {
	  wid = GTK_WIDGET(list_object_get(mainwindow_objects,i-1));
	  geometry_stack_push(GTK_WINDOW(wid),NULL,&window_geometry_stack);
	  gtk_widget_destroy(wid);
     }
}

static void edit_undo(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     document_undo(w->doc);
}

static void edit_redo(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     document_redo(w->doc);
}

static void update_clipboard_items(Mainwindow *w, gpointer user_data)
{
     set_sensitive(w->need_clipboard_items,user_data != NULL);
}

static void edit_cut(GtkMenuItem *menuitem, gpointer user_data)
{
     Chunk *chunk, *part;
     Mainwindow *w = MAINWINDOW(user_data);
     g_assert (w->doc->selend != w->doc->selstart);
     if (clipboard) gtk_object_unref(GTK_OBJECT(clipboard));
     part = chunk_get_part( w->doc->chunk, w->doc->selstart,
			    w->doc->selend - w->doc->selstart);
     chunk = chunk_remove_part( w->doc->chunk, w->doc->selstart,
				w->doc->selend - w->doc->selstart);
     clipboard = part;
     gtk_object_ref(GTK_OBJECT(clipboard));
     gtk_object_sink(GTK_OBJECT(clipboard));

     document_update(w->doc, chunk, w->doc->selstart, -(clipboard->length));
     list_object_foreach(mainwindow_objects, (GFunc)update_clipboard_items, 
			 (gpointer)w);
}

static void edit_crop(GtkMenuItem *menu_item, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     Chunk *c;
     c = chunk_get_part( w->doc->chunk, w->doc->selstart,
			 w->doc->selend - w->doc->selstart);
     document_update(w->doc, c, 0, -(w->doc->selstart));
}

static void edit_copy(GtkMenuItem *menu_item, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     g_assert (w->doc->selend != w->doc->selstart);
     if (clipboard) gtk_object_unref(GTK_OBJECT(clipboard));
     clipboard = chunk_get_part(w->doc->chunk, w->doc->selstart, 
				w->doc->selend - w->doc->selstart);
     gtk_object_ref(GTK_OBJECT(clipboard));
     gtk_object_sink(GTK_OBJECT(clipboard));

     list_object_foreach(mainwindow_objects, (GFunc)update_clipboard_items, 
			 (gpointer)w);
}

static void edit_paste(GtkMenuItem *menu_item, gpointer user_data)
{
     Chunk *c = clipboard, *nc;
     Mainwindow *w = MAINWINDOW(user_data);
     off_t cp,cl;
     if (w->doc == NULL) {
	  mainwindow_set_chunk(w,clipboard,NULL);
	  return;
     }
     
     if (!dataformat_equal(&(w->doc->chunk->format),&(c->format))) {
	  c = chunk_convert(c,&(w->doc->chunk->format),dither_editing,
			    w->statusbar);
	  if (c == NULL) return;	  
     }
     cl = c->length;
     nc = chunk_insert(w->doc->chunk,c,w->doc->cursorpos);     
     gtk_object_sink(GTK_OBJECT(c));

     cp = w->doc->cursorpos;
     document_update(w->doc, nc, cp, cl);
     document_set_selection( w->doc, cp, cp + cl );
}

static void edit_pasteover(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     Chunk *c,*d=clipboard;
     off_t orig_len,dl;
     if (w->doc == NULL) {
	  mainwindow_set_chunk(w,clipboard,NULL);
	  return;
     }

     if (!dataformat_equal(&(w->doc->chunk->format),&(d->format))) {
	  d = chunk_convert(d,&(w->doc->chunk->format),dither_editing,
			    w->statusbar);
	  if (d == NULL) return;
     }
     dl = d->length;

     orig_len = MIN(w->doc->chunk->length-w->doc->cursorpos, dl);
     c = chunk_replace_part(w->doc->chunk, w->doc->cursorpos, 
			    orig_len, d);
     gtk_object_sink(GTK_OBJECT(d));

     document_update(w->doc, c, 0, 0);
     document_set_selection( w->doc, w->doc->cursorpos,
			     w->doc->cursorpos + dl );
}

static void edit_mixpaste(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     Chunk *c,*p,*y,*d=clipboard;
     off_t partlen;
     if (w->doc == NULL) {
	  mainwindow_set_chunk(w,clipboard,NULL);
	  return;
     }

     if (!dataformat_equal(&(w->doc->chunk->format),&(d->format))) {
	  d = chunk_convert(d,&(w->doc->chunk->format),dither_editing,
			    w->statusbar);
	  if (d == NULL) return;
     }

     p = chunk_get_part(w->doc->chunk,w->doc->cursorpos,d->length);
     partlen = p->length;
     y = chunk_mix(p,d,dither_editing,w->statusbar);
     gtk_object_sink(GTK_OBJECT(p));
     if (!y) return;
     c = chunk_replace_part(w->doc->chunk,w->doc->cursorpos,partlen,y);
     gtk_object_sink(GTK_OBJECT(y));
     document_update(w->doc, c, 0, 0);
     document_set_selection( w->doc, w->doc->cursorpos,
			     w->doc->cursorpos + d->length );
     gtk_object_sink(GTK_OBJECT(d));
}

static void edit_pastetonew(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     mainwindow_set_chunk(w,clipboard,NULL);
}

static void edit_delete(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     Chunk *chunk;
     gint32 slen;
     slen = w->doc->selend - w->doc->selstart;
     chunk = chunk_remove_part(w->doc->chunk, w->doc->selstart, slen);
     document_update( w->doc, chunk, w->doc->selstart, -slen);
}

static Chunk *interp(Chunk *chunk, StatusBar *bar, gpointer user_data)
{
  return chunk_interpolate_endpoints(chunk,TRUE,dither_editing,bar);
}

static void edit_silence(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_apply_cb(w->doc,interp,TRUE,NULL);
}

static void edit_selectall(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (w->doc->selstart != 0 || w->doc->selend != w->doc->chunk->length)
	  document_set_selection(w->doc,0,w->doc->chunk->length);
     else
	  document_set_selection(w->doc,0,0);
}

static void edit_selectnone(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_set_selection(w->doc,0,0);
}

static void view_zoomin(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     document_zoom(w->doc,2.0,TRUE);
}

static void view_zoomout(GtkMenuItem *item, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     document_zoom(w->doc,0.5,TRUE);
}

static void view_zoomtoselection(GtkMenuItem *menuitem, gpointer user_data)
{
     Document *d;
     d = MAINWINDOW(user_data)->doc;
     document_set_view(d,d->selstart,d->selend);
}

static void view_zoomall(GtkMenuItem *menuitem, gpointer user_data)
{     
     Document *d;
     d = MAINWINDOW(user_data)->doc;
     document_set_view(d,0,d->chunk->length);
}

#ifdef SHOW_DEBUG_MENU
static void debug_mark(GtkMenuItem *menuitem, gpointer user_data)
{
     /* MAINWINDOW(user_data)->changed = TRUE; */
}

static gboolean dummy_proc(void *in, guint sample_size, 
			   chunk_writeout_func out_func, WriteoutID id,
			   Dataformat *format)
{
     return out_func(id,in,sample_size);
}

static void debug_dummy(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_apply(w->doc,dummy_proc,NULL,CHUNK_FILTER_MANY,FALSE,"DUMMY");
}

static void debug_dummy2(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_apply(w->doc,dummy_proc,NULL,CHUNK_FILTER_MANY,TRUE,"DUMMY");
}

static void checkoc_proc(Chunk *chunk, gpointer user_data)
{
     if (chunk->opencount > 0) 
	  printf(_("Chunk %p has opencount=%d\n"),chunk,chunk->opencount);
}

static void debug_checkoc(GtkMenuItem *menuitem, gpointer user_data)
{
     puts("--------");
     chunk_foreach((GFunc)checkoc_proc,NULL);
}

static void dump_format(Dataformat *df)
{
     printf("%d Channels @ %dHz ",df->channels,df->samplerate);
     if (df->type == DATAFORMAT_FLOAT) {
	  if (df->samplesize <= sizeof(float)) {
	       puts("float");
	  } else
	       puts("double");
     } else {
	  printf("%d%c%c\n",df->samplesize,df->sign?'S':'U',
		 df->bigendian?'B':'L');
     }
}

static void dump_dp(DataPart *dp)
{
     printf("        Datasource@%p, L/S=%Ld/%Ld Format: ",dp->ds,
	    dp->ds->length,dp->ds->bytes);
     dump_format(&(dp->ds->format));
     printf("        Usage: %Ld + %Ld\n",dp->position,dp->length);
     switch (dp->ds->type) {
     case DATASOURCE_REAL:
	  printf("        REAL @ %p\n",dp->ds->data.real);
	  break;
     case DATASOURCE_VIRTUAL:
	  printf("        VIRTUAL @ %s:%Ld\n",dp->ds->data.virtual.filename,
		 dp->ds->data.virtual.offset);
	  break;
     case DATASOURCE_TEMPFILE:
	  printf("        TEMPFILE @ %s:%Ld\n",dp->ds->data.virtual.filename,
		 dp->ds->data.virtual.offset);
	  break;
     case DATASOURCE_SILENCE:
	  puts("        SILENCE");
	  break;
     case DATASOURCE_SNDFILE:
	  printf("        SNDFILE @ %s:%Ld <rr:%c>\n",
		 dp->ds->data.sndfile.filename, dp->ds->data.sndfile.pos,
		 dp->ds->data.sndfile.raw_readable?'T':'F');
	  break;
     case DATASOURCE_SNDFILE_TEMPORARY:
	  printf("        SNDFILE @ %s:%Ld <rr:%c>\n",
		 dp->ds->data.sndfile.filename, dp->ds->data.sndfile.pos,
		 dp->ds->data.sndfile.raw_readable?'T':'F');
	  break;
     case DATASOURCE_REF:
	  printf("        REF --> %p\n",dp->ds->data.clone);
	  break;	  
     case DATASOURCE_CLONE:
	  printf("        CLONE --> %p\n",dp->ds->data.clone);
	  break;
     case DATASOURCE_BYTESWAP:
	  printf("        BYTESWAP --> %p\n",dp->ds->data.clone);
	  break;
     }
}

static void dump_chunk(Chunk *c)
{
     printf("    Chunk@%p, L/S=%Ld/%Ld, Format: ",c,c->length,c->size);
     dump_format(&(c->format));
     puts("      Used datasources:");
     g_list_foreach(c->parts,(GFunc)dump_dp,NULL);
}

static void indirect_dump_chunk(Chunk **cp) 
{
	dump_chunk(*cp);
}

static void windowinfo_proc(gpointer item, gpointer user_data)
{
     struct HistoryEntry *he;
     Mainwindow *w = MAINWINDOW(item);     
     printf(_("\nWindow '%s'\n"),w->doc->filename);
     puts(_("  Current chunk:"));
     dump_chunk(w->view->doc->chunk);
     puts(_("  History:"));
     he = w->doc->history_pos;
     if (he == NULL) {
	  puts("    (empty)");
     } else {
	  while (he->prev != NULL) he=he->prev;
	  while (he != NULL) {
	       dump_chunk(he->chunk);
	       he=he->next;
	  }
     }
}

static void debug_chunkinfo(GtkMenuItem *menuitem, gpointer user_data)
{    
     list_object_foreach(mainwindow_objects,windowinfo_proc,NULL);
}

#endif

static void mainwindow_show_effect_dialog(Mainwindow *mw, gchar *effect_name)
{
     EffectBrowser *eb;
     eb = EFFECT_BROWSER(effect_browser_new_with_effect(mw->doc,effect_name,
							'B',
							(effect_name!=NULL)));
     gtk_widget_show(GTK_WIDGET(eb));
}

static void effects_volume(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"volume");
}

static void effects_speed(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"speed");
}

static void effects_samplerate(GtkMenuItem *menuitem, gpointer user_data)
{    
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"srate");
}

static void effects_samplesize(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"ssize");
}

static void effects_mixchannels(GtkMenuItem *menuitem, gpointer user_data)
{
     Chunk *c;
     Mainwindow *w = MAINWINDOW(user_data);
     player_stop();
     if (w->doc->chunk->format.channels == 1)
	  user_info(_("There already is only one channel!"));
     else {
	  c = chunk_onechannel(w->doc->chunk,dither_editing,w->statusbar);
	  if (c) document_update(w->doc,c,0,0);
     }
}

static void effects_splitchannel(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     Chunk *c;
     c = chunk_copy_channel(w->doc->chunk,0,dither_editing,w->statusbar);
     if (c == NULL) return;
     player_stop();
     document_update(w->doc, c, 0, 0);
}

static void effects_mapchannels(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"mapchannels");
}

static void edit_stop(GtkMenuItem *menu_item, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (playing_document == NULL) {
	  if (w->doc->cursorpos != 0 || w->doc->chunk->length == 0)
	       document_set_cursor(w->doc,0);
	  else
	       document_set_cursor(w->doc,w->doc->chunk->length);
     } else
	  document_stop(w->doc, w->bouncemode);
}


gboolean mainwindow_update_caches(void)
{
     static guint last = 0;
     guint i,s;
     Mainwindow *wnd;

     s = list_object_get_size(mainwindow_objects);
     if (s == 0) return FALSE;
     if (last >= s) last=0;
     i = last+1;
     while (1) {	  
	  if (i >= s) i = 0;
	  wnd = MAINWINDOW(list_object_get(mainwindow_objects, i));
	  if (chunk_view_update_cache(wnd->view)) { last = i; return TRUE; }
	  if (i == last) return FALSE;
	  i++;
     }
}

static void edit_playselection(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     document_play_selection(w->doc,w->loopmode,
			     varispeed_reset_flag ? 1.0 : player_get_speed());
}

static void edit_playall(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     do_play(w,0,w->doc->chunk->length,w->loopmode);
}

static gchar *get_help_page_contents(int page)
{
     const char **c;
     char *ts[128];
     int i,j,k,l,x;
     gchar *r;
     c = help_page_contents[page];
     l = 0;
     for (i=0; c[i]!=NULL && i<128; i++) {
	  ts[i] = gettext(c[i]);
	  l += strlen(ts[i]);
     }
     r = g_malloc(l+1);
     k = 0;
     for (j=0; j<i; j++) {
	  x = strlen(ts[j]);
	  memcpy(r+k, ts[j], x);
	  k += x;
     }
     r[k] = 0;
     g_assert(k==l);
     return r;
}

static void help_readme(GtkMenuItem *menuitem, gpointer user_data)
{
     GtkAccelGroup* ag;
     GtkWidget *window,*table,*notebook,*frame,*label,*button,*box1,*box2;
     int i;
     GtkWidget *scrolledwindow1, *viewport1, *label2;
     gchar *c;
     Mainwindow *mw = MAINWINDOW(user_data);
     PangoFontDescription *pfd;
     ag = gtk_accel_group_new();

     window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

     gtk_container_set_border_width (GTK_CONTAINER (window), 10);
     gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);	// Centre the window
     gtk_window_set_default_size (GTK_WINDOW (window), -1, 400);	// Make readable
     gtk_window_set_title (GTK_WINDOW (window), _("mhWaveEdit Help"));

     box1 = gtk_vbox_new (FALSE, 0);
     gtk_container_add (GTK_CONTAINER (window), box1);
     gtk_widget_show (box1);

     box2 = gtk_vbox_new (FALSE, 10);
     gtk_container_set_border_width (GTK_CONTAINER (box2), 1);
     gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
     gtk_widget_show (box2);

     table = gtk_table_new (4, 6, FALSE);
     gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);

     notebook = gtk_notebook_new ();
     gtk_table_attach_defaults (GTK_TABLE (table), notebook, 0, 6, 0, 1);
     gtk_widget_show (notebook);

     for (i=0; i<help_page_count; i++)
     {
	frame = gtk_frame_new (NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
	gtk_widget_show (frame);

	scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwindow1);
	gtk_container_add (GTK_CONTAINER (frame), scrolledwindow1);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), 
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	viewport1 = gtk_viewport_new (NULL, NULL);
	gtk_widget_show (viewport1);
	gtk_container_add (GTK_CONTAINER (scrolledwindow1), viewport1);

	c = get_help_page_contents(i);
	label2 = gtk_label_new (c);
	g_free(c);
	if (i==HELP_PAGE_SHORTCUTS)	// Keyboard tab only
	{
	     pfd = pango_font_description_copy_static(GTK_WIDGET(mw)->style->font_desc);
	     pango_font_description_set_family_static(pfd, "Monospace");
	     gtk_widget_modify_font(label2, pfd);
	     pango_font_description_free(pfd);
	}
	gtk_container_add (GTK_CONTAINER (viewport1), label2);
	gtk_widget_set_can_focus (label2, TRUE);
	gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label2), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label2), 0, 0);
	gtk_misc_set_padding (GTK_MISC (label2), 5, 5);
	gtk_widget_show (label2);


	label = gtk_label_new (_(help_page_titles[i]));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), frame, label);
     }

     /* Show default page (defined in help.h) */
     gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook),HELP_PAGE_DEFAULT);
     gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);

     box2 = gtk_vbox_new (FALSE, 10);
     gtk_container_set_border_width (GTK_CONTAINER (box2), 1);
     gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
     gtk_widget_show (box2);

     button = gtk_button_new_with_label (_("Close"));
     gtk_widget_add_accelerator (button, "clicked", ag, GDK_KP_Enter, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (button, "clicked", ag, GDK_Return, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (button, "clicked", ag, GDK_Escape, 0, 
				 (GtkAccelFlags) 0);
     gtk_widget_set_can_default (button, TRUE);
     g_signal_connect_swapped(G_OBJECT(button),"clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       window);
     gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
     gtk_widget_show (button);

     gtk_widget_show (table);
     gtk_widget_show (window);
     gtk_window_add_accel_group(GTK_WINDOW (window), ag);
}

static void help_about(void)
{
     GtkAccelGroup* ag;
     gchar *p;
     GtkWidget *a,*b,*c;
     ag = gtk_accel_group_new();
     a = gtk_window_new(GTK_WINDOW_DIALOG);
gtk_window_set_modal(GTK_WINDOW(a),TRUE);
     gtk_window_set_title(GTK_WINDOW(a),_("About mhWaveEdit"));
     gtk_container_set_border_width(GTK_CONTAINER(a),5);
     gtk_window_set_position (GTK_WINDOW (a), GTK_WIN_POS_CENTER);	// Centre the window
     b = gtk_vbox_new(FALSE,10);
     gtk_container_add(GTK_CONTAINER(a),b);
     gtk_widget_show(b);
     c = gtk_label_new(PROGRAM_VERSION_STRING);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_label_new(_("Created by Magnus Hjorth (magnus.hjorth@home.se)\n"
	  "Copyright 2002-2018, Magnus Hjorth"));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);     
     p = g_strdup_printf(_("Current sound driver: %s"),sound_driver_name());
     c = gtk_label_new(p);
     g_free(p);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     p = g_strdup_printf(_("Compiled %s %s"), BUILD_DATE, BUILD_TIME);
     c = gtk_label_new(p);
     g_free(p);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
#ifdef USE_DOUBLE_SAMPLES
     c = gtk_label_new(_("Uses double-precision math"));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
#endif
     c = gtk_hseparator_new();
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_label_new(_("Distributed under GNU General Public License.\n"
		       "For information, see the file COPYING"));
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     c = gtk_button_new_with_label(_("OK"));
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_KP_Enter, 0, (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Return, 0, (GtkAccelFlags) 0);
     gtk_widget_add_accelerator (c, "clicked", ag, GDK_Escape, 0, (GtkAccelFlags) 0);
     g_signal_connect_swapped(G_OBJECT(c),"clicked",
			       G_CALLBACK(gtk_widget_destroy), a);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     gtk_widget_show(c);
     
     gtk_widget_show(a);
     gtk_window_add_accel_group(GTK_WINDOW (a), ag);
}

static void edit_play(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW (user_data);
     do_play(w,w->doc->cursorpos,w->doc->chunk->length,w->loopmode);
}

static void edit_selstartcursor(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (w->doc->selstart == w->doc->selend) 
	  document_set_selection(w->doc,w->doc->cursorpos,
				 w->doc->chunk->length);
     else 
	  document_set_selection(w->doc,w->doc->cursorpos,w->doc->selend);
}

static void edit_selendcursor(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (w->doc->selstart == w->doc->selend)
	  document_set_selection(w->doc,0,w->doc->cursorpos+1);
     else
	  document_set_selection(w->doc,w->doc->selstart,w->doc->cursorpos);
}

static void edit_record(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *wnd = MAINWINDOW (user_data);
     Chunk *c;
     int noverruns, i;
     off_t overrun_locs[10];
     gchar s[2], *str;
     player_stop();
     c = record_dialog_execute(&noverruns, overrun_locs);
     if (c) {
	  wnd = mainwindow_set_chunk(wnd,c,NULL);
	  if (noverruns > 0) {
	       for (i=0; i<noverruns && i<10; i++) {
		    s[0] = '0'+i;
		    s[1] = 0;
		    document_set_mark(wnd->doc, s,
				      overrun_locs[i]/c->format.samplebytes);
	       }
	       str = g_strdup_printf
		    ("%d overruns where detected while recording. "
		     "This means the recording may be damaged. The "
		     "locations where the overruns happened have been "
		     "indicated with markers.",noverruns);
	       user_warning(str);
	       g_free(str);
	  }
     }
}

static void edit_preferences(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_widget_show(config_dialog_new());
}

static void effects_combinechannels(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"combine");
}

static void effects_sandwich(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"sandwich");
}

static void view_timescale(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     chunk_view_set_timescale(w->view, checkmenuitem->active);
}

static void view_horizoom(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     if (w->hzoom_icon == NULL) return;
     if (checkmenuitem->active) {
	  gtk_widget_show(w->hzoom_icon);
	  gtk_widget_show(w->hzoom_slider);
     } else {
	  gtk_widget_hide(w->hzoom_icon);
	  gtk_widget_hide(w->hzoom_slider);
     }
}

static void view_vertzoom(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     if (w->vzoom_icon == NULL) return;
     if (checkmenuitem->active) {
	  gtk_widget_show(w->vzoom_icon);
	  gtk_widget_show(w->vzoom_slider);
	  gtk_widget_show(GTK_WIDGET(w->vzoom_label));
     } else {
	  gtk_widget_hide(w->vzoom_icon);
	  gtk_widget_hide(w->vzoom_slider);
	  gtk_widget_hide(GTK_WIDGET(w->vzoom_label));
     }
}

static void view_speed(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     if (w->speed_icon == NULL) return;
     if (checkmenuitem->active) {
	  gtk_widget_show(w->speed_icon);
	  gtk_widget_show(w->speed_slider);
	  if (w->show_labels) gtk_widget_show(GTK_WIDGET(w->speed_label));
     } else {
	  gtk_widget_hide(w->speed_icon);
	  gtk_widget_hide(w->speed_slider);
	  gtk_widget_hide(GTK_WIDGET(w->speed_label));
     }
}

static void view_sliderlabels(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     w->show_labels = checkmenuitem->active;
     if (w->vzoom_slider == NULL) return;
     if (w->show_labels) {
	  if (GTK_WIDGET_VISIBLE(w->vzoom_slider))
	       gtk_widget_show(GTK_WIDGET(w->vzoom_label));
	  if (GTK_WIDGET_VISIBLE(w->speed_slider))
	       gtk_widget_show(GTK_WIDGET(w->speed_label));
     } else {
	  gtk_widget_hide(GTK_WIDGET(w->vzoom_label));
	  gtk_widget_hide(GTK_WIDGET(w->speed_label));	  
     }
}

static void view_bufpos(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     w->view->show_bufpos = checkmenuitem->active;
     gtk_widget_queue_draw(GTK_WIDGET(w->view));
}

static void edit_insertsilence(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     gfloat f;
     off_t cp;
     Chunk *c,*nc;
     if (user_input_float(_("Seconds of silence: "),_("Insert Silence"),0.0,
			  GTK_WINDOW(w),&f)) 
	  return;
     if (f<=0.0) return;
     c = chunk_new_silent(&(w->doc->chunk->format),f);
     nc = chunk_insert(w->doc->chunk, c, w->doc->cursorpos);
     cp = w->doc->cursorpos;
     document_update(w->doc,nc,cp,c->length);
     document_set_selection( w->doc, cp, cp + c->length );
     gtk_object_sink(GTK_OBJECT(c));
}

static void edit_clearclipboard(GtkMenuItem *menuitem, gpointer user_data)
{
     gtk_object_unref(GTK_OBJECT(clipboard));
     clipboard = NULL;
     list_object_foreach(mainwindow_objects, 
			 (GFunc)update_clipboard_items, NULL);
}

static void edit_positioncursor(GtkMenuItem *menuitem, gpointer user_data)
{
     gtk_widget_show(goto_dialog_new(MAINWINDOW(user_data)));
}

static Chunk *effects_normalize_proc(Chunk *chunk, StatusBar *bar, 
				     gpointer user_data)
{
     sample_t s;
     Chunk *c;
     gfloat *lp = (gfloat *)user_data;
     sample_t level = (lp == NULL) ? 1.0 : *lp;
     s = chunk_peak_level(chunk,bar);
     if (s < 0.0) return NULL;
     c = chunk_amplify(chunk,level/s,dither_editing,bar);
     return c;
}

static void effects_normalize(GtkMenuItem *menuitem, gpointer user_data)
{     
     document_apply_cb(MAINWINDOW(user_data)->doc,effects_normalize_proc,
		       TRUE,NULL);
}

static void effects_normalizeto(GtkMenuItem *menuitem, gpointer user_data)
{
     gboolean b;
     gfloat val;
     val = inifile_get_gfloat("lastNormLevel",1.0);
     b = user_input_float(_("Level: "),_("Normalize to..."),val,
			  GTK_WINDOW(user_data),&val);
     if (b) return;
     b = document_apply_cb(MAINWINDOW(user_data)->doc,effects_normalize_proc,
			   TRUE,&val);
     if (b) return;
     inifile_set_gfloat("lastNormLevel",val);    
}


static void effects_pipe(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),"pipe");
}

static void cursor_moveto_beginning(GtkMenuItem *menuitem, gpointer user_data)
{
     document_set_cursor(MAINWINDOW(user_data)->doc,0);
}

static void cursor_moveto_end(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_set_cursor(w->doc,w->doc->chunk->length);
}

static void cursor_moveto_selstart(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_set_cursor(w->doc,w->doc->selstart);
}

static void cursor_moveto_selend(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_set_cursor(w->doc,w->doc->selend);
}

static void cursor_move_left(GtkMenuItem *menuitem, gpointer user_data)
{
     off_t delta;
     Mainwindow *w = MAINWINDOW(user_data);

     delta =
      - ((w->doc->viewend - w->doc->viewstart)/MAINWINDOW_NUDGE_DELTA_RATIO);

     if(delta > -1) {
          delta = -1;
     }

     document_nudge_cursor(w->doc,delta);
}

static void cursor_move_right(GtkMenuItem *menuitem, gpointer user_data)
{
     off_t delta;
     Mainwindow *w = MAINWINDOW(user_data);

     delta =
      ((w->doc->viewend - w->doc->viewstart)/MAINWINDOW_NUDGE_DELTA_RATIO);

     if(delta < 1) {
          delta = 1;
     }

     document_nudge_cursor(w->doc,delta);
}

static void cursor_move_leftsample(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_nudge_cursor(w->doc,-1);
}

static void cursor_move_rightsample(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     document_nudge_cursor(w->doc,1);
}

static void cursor_findzerocrossing_leftall(GtkMenuItem *menuitem,
 gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     off_t newpos;

     newpos = chunk_zero_crossing_all_reverse(
      w->doc->chunk,w->statusbar,w->doc->cursorpos);

     if(newpos >= 0) {
         document_set_cursor(w->doc,newpos);
     }
}

static void cursor_findzerocrossing_rightall(GtkMenuItem *menuitem,
 gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     off_t newpos;

     newpos = chunk_zero_crossing_all_forward(
      w->doc->chunk,w->statusbar,w->doc->cursorpos);

     if(newpos >= 0) {
         document_set_cursor(w->doc,newpos);
     }
}

static void cursor_findzerocrossing_leftany(GtkMenuItem *menuitem,
 gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     off_t newpos;

     newpos = chunk_zero_crossing_any_reverse(
      w->doc->chunk,w->statusbar,w->doc->cursorpos);

     if(newpos >= 0) {
          document_set_cursor(w->doc,newpos);
     }
}

static void cursor_findzerocrossing_rightany(GtkMenuItem *menuitem,
 gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     off_t newpos;

     newpos = chunk_zero_crossing_any_forward(
      w->doc->chunk,w->statusbar,w->doc->cursorpos);

     if(newpos >= 0) {
          document_set_cursor(w->doc,newpos);
     }
}

static Chunk *byteswap_proc(Chunk *chunk, StatusBar *bar, 
			    gpointer user_data)
{
     return chunk_byteswap(chunk);
}

static void effects_byteswap(GtkMenuItem *menuitem, gpointer user_data)
{
     document_apply_cb(MAINWINDOW(user_data)->doc,byteswap_proc,TRUE,NULL);
}

static Chunk *fadein_proc(Chunk *chunk, StatusBar *bar, gpointer user_data)
{
     return chunk_volume_ramp(chunk,0.0,1.0,dither_editing,bar);
}

static Chunk *fadeout_proc(Chunk *chunk, StatusBar *bar, gpointer user_data)
{
     return chunk_volume_ramp(chunk,1.0,0.0,dither_editing,bar);
}

static void effects_fadein(GtkMenuItem *menuitem, gpointer user_data)
{
     document_apply_cb(MAINWINDOW(user_data)->doc,fadein_proc,TRUE,NULL);
}

static void effects_fadeout(GtkMenuItem *menuitem, gpointer user_data)
{
     document_apply_cb(MAINWINDOW(user_data)->doc,fadeout_proc,TRUE,NULL);
}

static void effects_dialog(GtkMenuItem *menuitem, gpointer user_data)
{
     mainwindow_show_effect_dialog(MAINWINDOW(user_data),NULL);     
}

static void file_recent(GtkMenuItem *menuitem, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     Document *doc;
     gchar *fn;
     GList *l = w->recent;
     GList *m = recent_filenames;
     while (l->data != menuitem) { l=l->next; m=m->next; }
     fn = g_strdup((gchar *)m->data);
     doc = document_new_with_file(fn, w->statusbar);
     if (doc == NULL) { g_free(fn); return; }
     mainwindow_set_document(w, doc, fn);
     inifile_set("lastOpenFile",fn);
     recent_file(fn);
     g_free(fn);
}

static GtkWidget *create_menu(Mainwindow *w)
{
     guint i,j;
     GtkWidget *item, *subitem;
     GtkWidget *file_menu, *edit_menu, *view_menu, *cursor_menu, *play_menu,
               *effects_menu, *help_menu;
     GtkWidget *file_item, *edit_item, *view_item, *cursor_item, *play_item,
               *effects_item, *help_item;
     GtkAccelGroup *accel_group = NULL;
     GtkWidget *menubar, *submenu;
     gchar *desc;

     accel_group = gtk_accel_group_new();
     gtk_window_add_accel_group(GTK_WINDOW(w), accel_group);
     menubar = gtk_menu_bar_new();

     file_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("_Open..."));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_o,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(file_open),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Save"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_s,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(file_save),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Save _as..."));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(file_saveas),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_menu_item_new_with_label(_("Save selection as..."));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(file_saveselection),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     w->recent = NULL;
     j = inifile_get_guint32("recentFiles",4);
     if (j > MAINWINDOW_RECENT_MAX) j=4;
     if (j > 0) {
	  item = gtk_separator_menu_item_new ();
          gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     }
     for (i=1; i<=j; i++) {
          desc = g_strdup_printf("Last%d", i);
          item = gtk_menu_item_new_with_label(desc);
          g_signal_connect(item, "activate",G_CALLBACK(file_recent),w);
          gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
	  w->recent = g_list_append(w->recent, item);
	  gtk_widget_set_sensitive(item,FALSE);
	  g_free(desc);
     }
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Close"));
     g_signal_connect(item, "activate",G_CALLBACK(file_close),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Exit"));
     g_signal_connect(item, "activate",G_CALLBACK(file_exit),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item);

     edit_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("_Undo"));
     w->need_undo_items = g_list_append(w->need_undo_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_z,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_undo),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Redo"));
     w->need_redo_items = g_list_append(w->need_redo_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_redo),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Cu_t"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_x,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_cut),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Copy"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_c,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_copy),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Paste"));
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_v,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_paste),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Paste _over"));
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_pasteover),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Mix paste"));
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_mixpaste),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Insert _silence"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_insertsilence),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Paste to _new"));
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_pastetonew),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Cr_op"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_crop),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Delete"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_Delete,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_delete),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_label(_("Silence selection"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_silence),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Select _all"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_a,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_selectall),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_label(_("Select none"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_selectnone),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_label(_("Clear clipboard"));
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_clearclipboard),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);
     item = gtk_menu_item_new_with_label(_("Preferences"));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_p,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_preferences),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item);

     view_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("Zoom _in"));
     w->zoom_items = g_list_append(w->zoom_items,item);
     g_signal_connect(item, "activate",G_CALLBACK(view_zoomin),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Zoom _out"));
     w->zoom_items = g_list_append(w->zoom_items,item);
     g_signal_connect(item, "activate",G_CALLBACK(view_zoomout),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Zoom to _selection"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(view_zoomtoselection),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Zoom _all"));
     g_signal_connect(item, "activate",G_CALLBACK(view_zoomall),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_mnemonic(_("_Time scale"));
     gtk_check_menu_item_set_active(
	  GTK_CHECK_MENU_ITEM(item),
	  inifile_get_gboolean(INI_SETTING_TIMESCALE,
			       INI_SETTING_TIMESCALE_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_timescale),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_mnemonic(_("_Horizontal zoom"));
     gtk_check_menu_item_set_active
	  (GTK_CHECK_MENU_ITEM(item),
	   inifile_get_gboolean(INI_SETTING_HZOOM,INI_SETTING_HZOOM_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_horizoom),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_mnemonic(_("_Vertical zoom"));
     gtk_check_menu_item_set_active
	  (GTK_CHECK_MENU_ITEM(item),
	   inifile_get_gboolean(INI_SETTING_VZOOM,INI_SETTING_VZOOM_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_vertzoom),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_mnemonic(_("Sp_eed slider"));
     gtk_check_menu_item_set_active
	  (GTK_CHECK_MENU_ITEM(item),
	   inifile_get_gboolean(INI_SETTING_SPEED,INI_SETTING_SPEED_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_speed),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_label(_("Slider labels"));
     gtk_check_menu_item_set_active
	  (GTK_CHECK_MENU_ITEM(item),
	   inifile_get_gboolean(INI_SETTING_SLABELS,INI_SETTING_SLABELS_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_sliderlabels),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
     item = gtk_check_menu_item_new_with_label(_("Buffer position"));
     gtk_check_menu_item_set_active
	  (GTK_CHECK_MENU_ITEM(item),
	   inifile_get_gboolean(INI_SETTING_BUFPOS,INI_SETTING_BUFPOS_DEFAULT));
     g_signal_connect(item, "activate",G_CALLBACK(view_bufpos),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);

     cursor_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_label(_("Set selection start"));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_q,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_selstartcursor),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     item = gtk_menu_item_new_with_label(_("Set selection end"));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_w,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_selendcursor),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     submenu = gtk_menu_new();
     item = gtk_menu_item_new_with_label(_("Move to"));
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     subitem = gtk_menu_item_new_with_label(_("Beginning"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_h,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_moveto_beginning),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("End"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_j,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_moveto_end),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Selection start"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_k,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_moveto_selstart),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Selection end"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_l,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_moveto_selend),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
     submenu = gtk_menu_new();
     item = gtk_menu_item_new_with_label(_("Move"));
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     subitem = gtk_menu_item_new_with_label(_("Left"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_h,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_move_left),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Right"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_j,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_move_right),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Left sample"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_k,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_move_leftsample),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Right sample"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_l,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_move_rightsample),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
     submenu = gtk_menu_new();
     item = gtk_menu_item_new_with_label(_("Find zero-crossing"));
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     subitem = gtk_menu_item_new_with_label(_("Left (all channels)"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_y,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_findzerocrossing_leftall),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Right (all channels)"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_u,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_findzerocrossing_rightall),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Left (any channel)"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_i,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_findzerocrossing_leftany),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     subitem = gtk_menu_item_new_with_label(_("Right (any channel)"));
     gtk_widget_add_accelerator(subitem, "activate", accel_group, GDK_o,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(subitem, "activate",G_CALLBACK(cursor_findzerocrossing_rightany),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(submenu), subitem);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);
     item = gtk_menu_item_new_with_label(_("Position cursor..."));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_g,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(edit_positioncursor),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(cursor_menu), item);

     play_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("_Play from cursor"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_play),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Play _all"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_playall),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Play se_lection"));
     w->need_selection_items = g_list_append(w->need_selection_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_playselection),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Stop"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, item);
     g_signal_connect(item, "activate",G_CALLBACK(edit_stop),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Record..."));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F12,
                                0, GTK_ACCEL_VISIBLE);
     gtk_widget_set_sensitive(item,input_supported());
     g_signal_connect(item, "activate",G_CALLBACK(edit_record),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(play_menu), item);

     effects_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("Fade _in"));
     g_signal_connect(item, "activate",G_CALLBACK(effects_fadein),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Fade o_ut"));
     g_signal_connect(item, "activate",G_CALLBACK(effects_fadeout),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Normalize"));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_n,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(effects_normalize),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Normali_ze to..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_normalizeto),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Volume adjust (fade)..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_volume),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Convert sample_rate..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_samplerate),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Convert sample _format..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_samplesize),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("B_yte swap"));
     g_signal_connect(item, "activate",G_CALLBACK(effects_byteswap),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Mix to mono"));
     g_signal_connect(item, "activate",G_CALLBACK(effects_mixchannels),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Add channe_l"));
     g_signal_connect(item, "activate",G_CALLBACK(effects_splitchannel),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Ma_p channels..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_mapchannels),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Combine channels..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_combinechannels),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Add channels from other file..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_sandwich),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_Speed adjustment..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_speed),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Pipe through program..."));
     g_signal_connect(item, "activate",G_CALLBACK(effects_pipe),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_separator_menu_item_new ();
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("Effects dialog..."));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_e,
                                GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(effects_dialog),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(effects_menu), item);

#ifdef SHOW_DEBUG_MENU
     GtkWidget *debug_menu, *debug_item;
     debug_menu = gtk_menu_new();
     /*item = gtk_menu_item_new_with_label(_("Mark as modified"));
     g_signal_connect(item, "activate",G_CALLBACK(debug_mark),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(debug_menu), item);*/
     item = gtk_menu_item_new_with_label(_("Dummy effect"));
     g_signal_connect(item, "activate",G_CALLBACK(debug_dummy),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(debug_menu), item);
     item = gtk_menu_item_new_with_label(_("Dummy effect FP"));
     g_signal_connect(item, "activate",G_CALLBACK(debug_dummy2),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(debug_menu), item);
     item = gtk_menu_item_new_with_label(_("Check opencount"));
     g_signal_connect(item, "activate",G_CALLBACK(debug_checkoc),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(debug_menu), item);
     item = gtk_menu_item_new_with_label(_("Dump chunk info"));
     g_signal_connect(item, "activate",G_CALLBACK(debug_chunkinfo),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(debug_menu), item);
#endif

     help_menu = gtk_menu_new();
     item = gtk_menu_item_new_with_mnemonic(_("_Documentation"));
     gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F1,
                                0, GTK_ACCEL_VISIBLE);
     g_signal_connect(item, "activate",G_CALLBACK(help_readme),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), item);
     item = gtk_menu_item_new_with_mnemonic(_("_About"));
     g_signal_connect(item, "activate",G_CALLBACK(help_about),w);
     gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), item);

     file_item = gtk_menu_item_new_with_mnemonic(_("_File"));
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);
     edit_item = gtk_menu_item_new_with_mnemonic(_("_Edit"));
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);
     view_item = gtk_menu_item_new_with_mnemonic(_("_View"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, view_item);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);
     cursor_item = gtk_menu_item_new_with_mnemonic(_("_Cursor"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, cursor_item);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(cursor_item), cursor_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), cursor_item);
     play_item = gtk_menu_item_new_with_mnemonic(_("_Play"));
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(play_item), play_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), play_item);
     effects_item = gtk_menu_item_new_with_mnemonic(_("Effec_ts"));
     w->need_chunk_items = g_list_append(w->need_chunk_items, effects_item);
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(effects_item), effects_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), effects_item);
#ifdef SHOW_DEBUG_MENU
     debug_item = gtk_menu_item_new_with_label(_("Debug"));
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(debug_item), debug_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), debug_item);
#endif
     help_item = gtk_menu_item_new_with_mnemonic(_("_Help"));
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
     gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

     update_file_recent(w);
     return menubar;
}

static void loopmode_toggle(GtkToggleToolButton *button, gboolean *user_data)
{
     *user_data = gtk_toggle_tool_button_get_active(button);
     inifile_set_gboolean("loopMode", *user_data);
}

static void followmode_toggle(GtkToggleToolButton *button, gboolean *user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     w->followmode = gtk_toggle_tool_button_get_active(button);
     if (w->doc != NULL)
	  document_set_followmode(w->doc, w->followmode);
     inifile_set_gboolean("followMode", w->followmode);
}

static void bouncemode_toggle(GtkToggleToolButton *button, gboolean *user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     w->bouncemode = gtk_toggle_tool_button_get_active(button);
     inifile_set_gboolean("bounceMode", w->bouncemode);
}

static GtkWidget *create_toolbar(Mainwindow *w)
{
     GtkWidget *t,*b;
     GtkToolItem *i;
     GdkPixbuf *pb;
     t = gtk_toolbar_new();
     pb = gdk_pixbuf_new_from_xpm_data (button_open_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Load a file from disk"));
     g_signal_connect(i,"clicked",G_CALLBACK(file_open),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_save_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Save the current file to disk"));
     g_signal_connect(i,"clicked",G_CALLBACK(file_save),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items,GTK_WIDGET(i));

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_undo_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Undo the last change"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_undo),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_undo_items = g_list_append(w->need_undo_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_redo_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Redo the last undo operation"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_redo),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_redo_items = g_list_append(w->need_redo_items, GTK_WIDGET(i));

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_cut_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Cut out the current selection"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_cut),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_selection_items = g_list_append(w->need_selection_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_copy_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Copy the current selection"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_copy),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_selection_items = g_list_append(w->need_selection_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_paste_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Paste at cursor position"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_paste),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_pasteover_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Paste, overwriting the data after the cursor position"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_pasteover),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_clipboard_items = g_list_append(w->need_clipboard_items, GTK_WIDGET(i));
     gtk_widget_set_sensitive(GTK_WIDGET(i),FALSE);

     pb = gdk_pixbuf_new_from_xpm_data (button_delete_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Delete the selection"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_delete),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_selection_items = g_list_append(w->need_selection_items, GTK_WIDGET(i));

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_cursorstart_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Set selection start to cursor position"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_selstartcursor),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_cursorend_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Set selection end to cursor position"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_selendcursor),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items, GTK_WIDGET(i));

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_play_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Play from cursor position"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_play),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_playselection_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Play selected area"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_playselection),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items, GTK_WIDGET(i));

     pb = gdk_pixbuf_new_from_xpm_data (button_stop_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Stop playing"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_stop),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     w->need_chunk_items = g_list_append(w->need_chunk_items, GTK_WIDGET(i));

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_loop_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_toggle_tool_button_new();
     gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(i),b);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Loop mode (play over and over)"));
     g_signal_connect(i,"toggled",G_CALLBACK(loopmode_toggle),&(w->loopmode));
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     if ( inifile_get_gboolean("loopMode",FALSE) ) 
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(i), TRUE);

     pb = gdk_pixbuf_new_from_xpm_data (button_follow_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_toggle_tool_button_new();
     gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(i),b);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Keep view and playback together"));
     g_signal_connect(i,"toggled",G_CALLBACK(followmode_toggle),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     if ( inifile_get_gboolean("followMode",FALSE) ) {
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(i), TRUE);
	  w->followmode = TRUE;
     } else
	  w->followmode = FALSE;

     pb = gdk_pixbuf_new_from_xpm_data (button_bounce_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_toggle_tool_button_new();
     gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(i),b);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Auto return to playback start"));
     g_signal_connect(i,"toggled",G_CALLBACK(bouncemode_toggle),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     if ( inifile_get_gboolean("bounceMode",FALSE) ) {
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(i), TRUE);
	  w->bouncemode = TRUE;
     }
     
     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_record_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Record"));
     g_signal_connect(i,"clicked",G_CALLBACK(edit_record),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     gtk_widget_set_sensitive(GTK_WIDGET(i),input_supported());

     i = gtk_separator_tool_item_new();
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);

     pb = gdk_pixbuf_new_from_xpm_data (button_mixer_xpm);
     b = gtk_image_new_from_pixbuf(pb);
     i = gtk_tool_button_new(b,NULL);
     gtk_widget_set_tooltip_text(GTK_WIDGET(i),_("Launch mixer"));
     g_signal_connect(i,"clicked",G_CALLBACK(launch_mixer),w);
     gtk_toolbar_insert(GTK_TOOLBAR(t),i,-1);
     return t;
}

static gboolean setting_zoom_flag = FALSE;

static void mainwindow_view_changed(Document *d, gpointer user_data)
{
     float max_samp,min_samp,current_samp;
     Mainwindow *w = MAINWINDOW(user_data);
     w->view_adj->page_size = (d->viewend - d->viewstart);
     w->view_adj->upper = d->chunk->length;
     w->view_adj->value = d->viewstart;
     w->view_adj->step_increment = 16;
     w->view_adj->page_increment = w->view_adj->page_size / 2;     
     gtk_adjustment_changed ( w->view_adj );
     setting_zoom_flag = TRUE;
     max_samp = w->doc->chunk->length;
     min_samp = GTK_WIDGET(w->view)->allocation.width;
     current_samp = w->doc->viewend - w->doc->viewstart;
     if (current_samp < min_samp) {
	  if (current_samp < w->doc->chunk->length) {
	       /* We are zoomed in longer than the slider allows, so set it 
		* to max */
	       w->zoom_adj->value = 1.0;
	       w->zoom_adj->page_size = 0.2;
	       set_sensitive(w->zoom_items,TRUE);
	  } else {
	       /* We're viewing the whole chunk and can't zoom in or out */
	       w->zoom_adj->value = 0.0;
	       w->zoom_adj->page_size = 1.2;
	       set_sensitive(w->zoom_items,FALSE);
	  }
     } else {
	  w->zoom_adj->value = 
	       log(current_samp/max_samp)/log(min_samp/max_samp);
	  w->zoom_adj->page_size = 0.2;
	  set_sensitive(w->zoom_items,TRUE);
     }
     gtk_adjustment_changed ( w->zoom_adj );
     setting_zoom_flag = FALSE;
     update_desc(w);
}

static void mainwindow_selection_changed(Document *d, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW ( user_data );
     update_desc ( w );
     set_sensitive(w->need_selection_items, (d->selstart != d->selend));
}

static void mainwindow_cursor_changed(Document *d, gboolean rolling, 
				      gpointer user_data)
{
     Mainwindow *w;
     w = MAINWINDOW ( user_data );
     if (!rolling || status_bar_roll_cursor) update_desc(w);
}

static void mainwindow_state_changed(Document *d, gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     fix_title(w);
     update_desc(w);     
     set_sensitive(w->need_undo_items,document_can_undo(d));
     set_sensitive(w->need_redo_items,document_can_redo(d));
     set_sensitive(w->need_selection_items,(d->selstart != d->selend));
     mainwindow_view_changed(d,user_data);
}

static void mainwindow_value_changed(GtkAdjustment *adjustment, 
				     gpointer user_data)
{
     Document *d = MAINWINDOW(user_data)->doc;
     off_t s,e;
     s = (off_t)adjustment->value;
     e = (off_t)(adjustment->value + adjustment->page_size);
     /* Due to rounding, these can become out of range. 
      * Especially for GTK1 where value and page_size are gfloats */
     if (s < 0) s = 0;
     if (e <= s) e = s+1; else if (e > d->chunk->length) e = d->chunk->length;
     document_set_view(d, s, e);
}

static void mainwindow_zoom_changed(GtkAdjustment *adjustment, 
				    gpointer user_data)
{
     gboolean follow_cursor;
     float max_samp,min_samp,target_samp,current_samp;
     Mainwindow *w = MAINWINDOW(user_data);

     /* puts("mainwindow_zoom_changed"); */
     if (setting_zoom_flag) return;
     current_samp = w->doc->viewend - w->doc->viewstart;
     min_samp = GTK_WIDGET(w->view)->allocation.width;
     max_samp = w->doc->chunk->length;
     target_samp = max_samp * pow(min_samp/max_samp,adjustment->value);
     follow_cursor = target_samp <= current_samp;
     document_zoom(w->doc,current_samp/target_samp,follow_cursor);
}

static void mainwindow_vertical_zoom_changed(GtkAdjustment *adjustment,
					     gpointer user_data)
{
     gfloat s;
     Mainwindow *w = MAINWINDOW(user_data);
     gchar c[32],d[32];
     int ndec;
     s = pow(100.0, adjustment->value);
     chunk_view_set_scale(w->view, s);
     ndec = 3-(int)(adjustment->value*2.0);
     if (ndec > 0) {
	  g_snprintf(d,sizeof(d),"V: %%.0%df",ndec);
	  g_snprintf(c,sizeof(c),d,(float) s);
     } else {
	  g_snprintf(c,sizeof(c),"V: %d",(int)s);
     }
     gtk_label_set_text(w->vzoom_label,c);
}

static void mainwindow_speed_changed(GtkAdjustment *adjustment,
				     gpointer user_data)
{
     Mainwindow *w = MAINWINDOW(user_data);
     gchar c[32];
     gfloat f;
     f = adjustment->value;
#ifdef INV_SPEED     
     f = -f;
#endif
     if (w->doc == playing_document)
	  player_set_speed(f);
    g_snprintf(c,sizeof(c),"S: %d%%",(int)(f*100.0+0.5));
     gtk_label_set_text(w->speed_label,c);
}

static void dc_func(gchar *label, off_t pos, gpointer fud)
{
     off_t *ud = (off_t *)fud;
     if (pos < ud[0] && pos > ud[1]) ud[1] = pos;
     else if (pos > ud[0] && pos < ud[2]) ud[2] = pos;
}

static void mainwindow_view_double_click(ChunkView *view, 
					 off_t *sample, Mainwindow *wnd)
{
     off_t o[3] = { *sample, 0, wnd->doc->chunk->length };               
     document_foreach_mark(wnd->doc,dc_func,o);
     document_set_selection(wnd->doc,o[1],o[2]);
}

static gint hzoom_scale_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
     Mainwindow *mw = MAINWINDOW(user_data);
     if (event->button == 3) {
	  gtk_adjustment_set_value(mw->zoom_adj, 0);
	  return TRUE;
     }
     return FALSE;
}

static gint vzoom_scale_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
     Mainwindow *mw = MAINWINDOW(user_data);
     if (event->button == 3) {
	  gtk_adjustment_set_value(mw->vertical_zoom_adj, 0);
	  return TRUE;
     }
     return FALSE;
}

static gint speed_scale_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
     Mainwindow *mw = MAINWINDOW(user_data);
     if (event->button == 3) {
#ifdef INV_SPEED
	  gtk_adjustment_set_value(mw->speed_adj, -1.0);
#else
	  gtk_adjustment_set_value(mw->speed_adj, 1.0);
#endif
	  return TRUE;
     }
     return FALSE;
}

static void mainwindow_init(Mainwindow *obj)
{
     GtkWidget *a,*b,*c;
     GtkRequisition req;
     GtkTargetEntry gte;

     if (!window_geometry_stack_inited) {
	  if (inifile_get_gboolean("useGeometry",FALSE))
	       window_geometry_stack = 
		    geometry_stack_from_inifile("windowGeometry");
	  window_geometry_stack_inited = TRUE;
	  load_recent();
     }

     /* St ll in datastrukturen */
     obj->view = CHUNKVIEW(chunk_view_new());

     chunk_view_set_timescale(
	  obj->view, inifile_get_gboolean(INI_SETTING_TIMESCALE,
					  INI_SETTING_TIMESCALE_DEFAULT));
     obj->view->show_bufpos = inifile_get_gboolean
	  (INI_SETTING_BUFPOS,INI_SETTING_BUFPOS_DEFAULT);
     g_signal_connect(G_OBJECT(obj->view), "double-click",
			 G_CALLBACK(mainwindow_view_double_click), obj);
     obj->view_adj = GTK_ADJUSTMENT( gtk_adjustment_new ( 0,0,0,0,0,0 ));
     g_signal_connect(G_OBJECT(obj->view_adj), "value-changed",
			 G_CALLBACK(mainwindow_value_changed), obj);
     obj->zoom_adj = GTK_ADJUSTMENT( gtk_adjustment_new ( 0, 0, 1.2,0.01, 0.1, 
							  0.2 ));
     g_signal_connect(G_OBJECT(obj->zoom_adj), "value-changed",
			 G_CALLBACK(mainwindow_zoom_changed), obj);
     obj->vertical_zoom_adj = 
       GTK_ADJUSTMENT(gtk_adjustment_new ( 0, 0, 0.2 + log (inifile_get_guint32 ("vzoomMax", 100)) / log (100.0), 0.01, 0.1, 0.2 ));
     g_signal_connect(G_OBJECT(obj->vertical_zoom_adj), "value-changed",
			 G_CALLBACK(mainwindow_vertical_zoom_changed),
			 obj);
#ifdef INV_SPEED
     obj->speed_adj =
	  GTK_ADJUSTMENT(gtk_adjustment_new(-1.0,-2.0,0.20,0.01,0.1,0.2));
#else
     obj->speed_adj =
	  GTK_ADJUSTMENT(gtk_adjustment_new(1.0,0.0,2.2,0.01,0.1,0.2));
#endif
     g_signal_connect(G_OBJECT(obj->speed_adj),"value-changed",
			G_CALLBACK(mainwindow_speed_changed),
			obj);

     obj->statusbar = STATUSBAR(status_bar_new());
     g_signal_connect(G_OBJECT(obj->statusbar),"progress_begin",
			G_CALLBACK(procstart),obj);
     g_signal_connect(G_OBJECT(obj->statusbar),"progress_end",
			G_CALLBACK(procend),obj);
     obj->loopmode = inifile_get_gboolean("loopMode",FALSE);
     obj->bouncemode = FALSE; /* Set to true by create_toolbar */
     obj->sensitive = TRUE;
     obj->vzoom_icon = obj->vzoom_slider = NULL;
     obj->hzoom_icon = obj->hzoom_slider = NULL;
     obj->speed_icon = obj->speed_slider = NULL;
     obj->speed_label = GTK_LABEL(gtk_label_new("S: 500%"));
     gtk_misc_set_alignment(GTK_MISC(obj->speed_label),0.0,0.0);
     gtk_widget_size_request(GTK_WIDGET(obj->speed_label),&req);
     gtk_widget_set_usize(GTK_WIDGET(obj->speed_label),req.width,req.height);
     obj->vzoom_label = GTK_LABEL(gtk_label_new("V: 1.000"));
     gtk_misc_set_alignment(GTK_MISC(obj->vzoom_label),0.0,0.0);
     gtk_widget_size_request(GTK_WIDGET(obj->vzoom_label),&req);
     gtk_widget_set_usize(GTK_WIDGET(obj->vzoom_label),req.width,req.height);

     obj->need_chunk_items = NULL;
     obj->need_selection_items = NULL;
     obj->need_clipboard_items = NULL;
     obj->need_undo_items = NULL;
     obj->zoom_items = NULL;
     
     fix_title ( obj );
     
     /* Bygg upp komponenterna */
     a = gtk_vbox_new( FALSE, 0 );
     gtk_container_add(GTK_CONTAINER(obj),a);
     b = create_menu( obj );
     obj->menubar = b;
     gtk_box_pack_start(GTK_BOX(a),b,FALSE,FALSE,0);
     b = gtk_table_new(3,4,FALSE);
     gtk_box_pack_start(GTK_BOX(a),b,TRUE,TRUE,0);

     c = create_toolbar( obj );
     obj->toolbar = c;

     c = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(c),GTK_WIDGET(obj->toolbar));
     gtk_widget_size_request(c,&req);
     gtk_widget_set_usize(c,10,req.height);

     gtk_table_attach(GTK_TABLE(b),c,0,1,0,1,GTK_SHRINK|GTK_FILL,GTK_FILL,0,0);

     c = bitmap_new_from_data(vzoom_bits, vzoom_width, vzoom_height);
     bitmap_set_fg(BITMAP(c),0.8);     
     gtk_table_attach(GTK_TABLE(b),c,1,2,0,1,GTK_FILL,0,0,0);
     obj->vzoom_icon = c;

     c = bitmap_new_from_data(hzoom_bits, hzoom_width, hzoom_height);
     bitmap_set_fg(BITMAP(c),0.8);
     gtk_table_attach(GTK_TABLE(b),c,2,3,0,1,GTK_FILL,0,0,0);
     obj->hzoom_icon = c;

     c = bitmap_new_from_data(speed_bits, speed_width, speed_height);
     bitmap_set_fg(BITMAP(c),0.8);
     gtk_table_attach(GTK_TABLE(b),c,3,4,0,1,GTK_FILL,0,0,0);
     obj->speed_icon = c;

     gtk_table_attach(GTK_TABLE(b),GTK_WIDGET(obj->view),0,1,1,2,
		      GTK_EXPAND|GTK_FILL|GTK_SHRINK,
		      GTK_EXPAND|GTK_FILL|GTK_SHRINK,0,0);

     c = gtk_vscale_new ( obj->zoom_adj );
     /* GTK1 doesn't work well with using the right mouse button press event. 
      * As a work around, we use the release event instead */
     g_signal_connect(G_OBJECT(c),"button_press_event",G_CALLBACK(hzoom_scale_press),obj);
     gtk_scale_set_digits(GTK_SCALE(c),3);
     gtk_scale_set_draw_value (GTK_SCALE(c), FALSE);
     gtk_table_attach(GTK_TABLE(b),c,2,3,1,2,0,GTK_EXPAND|GTK_FILL,0,0);
     obj->need_chunk_items = g_list_append(obj->need_chunk_items,c);
     obj->hzoom_slider = c;

     c = gtk_vscale_new ( obj->vertical_zoom_adj );
     g_signal_connect(G_OBJECT(c),"button_press_event",G_CALLBACK(vzoom_scale_press),obj);
     gtk_scale_set_digits(GTK_SCALE(c),3);
     gtk_scale_set_draw_value (GTK_SCALE(c), FALSE);
     gtk_table_attach(GTK_TABLE(b),c,1,2,1,2,0,GTK_EXPAND|GTK_FILL,0,0);
     obj->need_chunk_items = g_list_append(obj->need_chunk_items,c);
     obj->vzoom_slider = c;

     c = gtk_vscale_new ( obj->speed_adj );
     g_signal_connect(G_OBJECT(c),"button_press_event",G_CALLBACK(speed_scale_press),obj);
     gtk_scale_set_digits(GTK_SCALE(c),2);
     gtk_scale_set_draw_value (GTK_SCALE(c), FALSE);
     /* gtk_range_set_update_policy(GTK_RANGE(c),GTK_UPDATE_DELAYED); */
#ifndef INV_SPEED
     gtk_range_set_inverted(GTK_RANGE(c),TRUE);
#endif
     gtk_table_attach(GTK_TABLE(b),c,3,4,1,2,0,GTK_EXPAND|GTK_FILL,0,0);
     obj->need_chunk_items = g_list_append(obj->need_chunk_items,c);
     obj->speed_slider = c;

     c = gtk_hscrollbar_new ( obj->view_adj );
     gtk_table_attach(GTK_TABLE(b),c,0,1,2,3,GTK_FILL,0,0,0);

     c = GTK_WIDGET(obj->speed_label);
     gtk_table_attach(GTK_TABLE(b),c,1,4,3,4,GTK_FILL,0,2,0);

     c = gtk_event_box_new();
     gtk_container_add(GTK_CONTAINER(c),GTK_WIDGET (obj->statusbar));
     gtk_table_attach(GTK_TABLE(b),c,0,1,3,4,GTK_FILL,0,0,0);

     c = GTK_WIDGET(obj->vzoom_label);
     gtk_table_attach(GTK_TABLE(b),c,1,4,2,3,GTK_FILL,0,2,0);

     gtk_adjustment_value_changed(obj->speed_adj);
     gtk_adjustment_value_changed(obj->vertical_zoom_adj);
     gtk_widget_show_all(a);

     if (!inifile_get_gboolean(INI_SETTING_VZOOM,INI_SETTING_VZOOM_DEFAULT)) {
	  gtk_widget_hide(obj->vzoom_icon);
	  gtk_widget_hide(obj->vzoom_slider);
	  gtk_widget_hide(GTK_WIDGET(obj->vzoom_label));
     }

     if (!inifile_get_gboolean(INI_SETTING_HZOOM,INI_SETTING_HZOOM_DEFAULT)) {
	  gtk_widget_hide(obj->hzoom_icon);
	  gtk_widget_hide(obj->hzoom_slider);
     }
     
     if (!inifile_get_gboolean(INI_SETTING_SPEED,INI_SETTING_SPEED_DEFAULT)) {
	  gtk_widget_hide(obj->speed_icon);
	  gtk_widget_hide(obj->speed_slider);
	  gtk_widget_hide(GTK_WIDGET(obj->speed_label));
     }

     obj->show_labels = inifile_get_gboolean(INI_SETTING_SLABELS,INI_SETTING_SLABELS_DEFAULT);
     if (!obj->show_labels) {
	  gtk_widget_hide(GTK_WIDGET(obj->vzoom_label));
	  gtk_widget_hide(GTK_WIDGET(obj->speed_label));
     }

     gtk_window_set_policy(GTK_WINDOW(obj),FALSE,TRUE,FALSE);
     
     set_sensitive(obj->need_chunk_items, FALSE);
     set_sensitive(obj->need_selection_items, FALSE);
     set_sensitive(obj->need_clipboard_items, clipboard != NULL);
     set_sensitive(obj->need_undo_items, FALSE);
     set_sensitive(obj->need_redo_items, FALSE);

     if (!geometry_stack_pop(&window_geometry_stack,NULL,GTK_WINDOW(obj)))
	  gtk_window_set_default_size(GTK_WINDOW(obj),540,230);

     /* Setup drag'n'drop target */
     gte.target = "text/uri-list";
     gte.flags = gte.info = 0;
     gtk_drag_dest_set(GTK_WIDGET(obj), GTK_DEST_DEFAULT_ALL, &gte, 1, 
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
}

GtkWidget *mainwindow_new(void)
{
     return GTK_WIDGET(g_object_new(MAINWINDOW_TYPE, NULL));
}

GtkWidget *mainwindow_new_with_file(char *filename, gboolean log)
{
     Mainwindow *w;
     Document *doc;
     gchar *d;
     w = MAINWINDOW ( mainwindow_new() );
     d = make_filename_rooted(filename);
     doc = document_new_with_file(d, w->statusbar);
     if (doc != NULL) {
          mainwindow_set_document(w, doc, d);
          if (log) {
	       inifile_set("lastOpenFile",d);
	       recent_file(d);
	  }
     }
     g_free(d);
     return GTK_WIDGET(w);
}

static void reset_statusbar(Mainwindow *w)
{
     status_bar_reset(w->statusbar);
}

void mainwindow_update_texts(void)
{     
     list_object_foreach(mainwindow_objects,(GFunc)fix_title,NULL);
     list_object_foreach(mainwindow_objects,(GFunc)reset_statusbar,NULL);
     list_object_foreach(mainwindow_objects,(GFunc)update_desc,NULL);
}

void mainwindow_set_sensitive(Mainwindow *mw, gboolean sensitive)
{
     gtk_widget_set_sensitive(mw->menubar,sensitive);
     gtk_widget_set_sensitive(mw->toolbar,sensitive);
     gtk_widget_set_sensitive(GTK_WIDGET(mw->view),sensitive);
     mw->sensitive = sensitive;     
}

void mainwindow_set_all_sensitive(gboolean sensitive)
{
     list_object_foreach(mainwindow_objects,(GFunc)mainwindow_set_sensitive,
			 GINT_TO_POINTER(sensitive));
}

void mainwindow_repaint_views(void)
{
     GList *l;          
     for (l=mainwindow_objects->list; l!=NULL; l=l->next) {
	  chunk_view_force_repaint(MAINWINDOW(l->data)->view);
     }
}
