/*
 * Copyright (C) 2002 2003 2004 2005 2008 2009 2010 2011 2012, Magnus Hjorth
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


#include "main.h"
#include <math.h>

#include "sound.h"
#include "inifile.h"
#include "configdialog.h"
#include "um.h"
#include "mainwindow.h"
#include "tempfile.h"
#include "player.h"
#include "rateconv.h"
#include "filetypes.h"

G_DEFINE_TYPE (ConfigDialog, config_dialog, GTK_TYPE_DIALOG)

static void config_dialog_destroy (GObject *obj)
{
   // this is triggered by gtk_widget_destroy()
   G_OBJECT_CLASS(config_dialog_parent_class)->dispose(obj);
}

static void config_dialog_class_init (ConfigDialogClass *klass)
{
     GObjectClass *oc = G_OBJECT_CLASS(klass);
     oc->dispose = config_dialog_destroy;
}

static void config_dialog_ok(GtkButton *button, gpointer user_data)
{
    gchar *c;
    gboolean b = FALSE;
    GList *l;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean valid;
    int index;
    ConfigDialog *cd = CONFIG_DIALOG(user_data);
    if (intbox_check(cd->sound_buffer_size) || 
        intbox_check(cd->disk_threshold) ||
	intbox_check(cd->view_quality) ||
	intbox_check_limit(cd->recent_files,0,MAINWINDOW_RECENT_MAX,
			   _("number of recent files")) ||
        intbox_check_limit(cd->vzoom_max,1,9999, _("maximum vertical zoom")) ||
	format_selector_check(cd->fallback_format))
        return;
    index = gtk_combo_box_get_active (GTK_COMBO_BOX (cd->sound_driver));
    c = sound_driver_id_from_index (index);
    g_assert(c != NULL);

    if (gtk_toggle_button_get_active(cd->driver_autodetect))
	 inifile_set(INI_SETTING_SOUNDDRIVER,"auto");
    else
	 b = inifile_set(INI_SETTING_SOUNDDRIVER,c) || b;

    b = inifile_set_guint32(INI_SETTING_SOUNDBUFSIZE,
			    cd->sound_buffer_size->val) || b;
    if (b) user_info(_("Some of the settings you have changed will not be "
		     "activated until you restart the program"));

    inifile_set_guint32(INI_SETTING_REALMAX,cd->disk_threshold->val*1024);
    inifile_set_guint32(INI_SETTING_VIEW_QUALITY,
			cd->view_quality->val);
    inifile_set_gboolean(INI_SETTING_TIMESCALE,
			 gtk_toggle_button_get_active(cd->time_scale_default));
    inifile_set_gboolean(INI_SETTING_VZOOM,
			 gtk_toggle_button_get_active(cd->vzoom_default));
    inifile_set_gboolean(INI_SETTING_HZOOM,
			 gtk_toggle_button_get_active(cd->hzoom_default));
    inifile_set_gboolean(INI_SETTING_SPEED,
			 gtk_toggle_button_get_active(cd->speed_default));
    inifile_set_gboolean(INI_SETTING_SLABELS,
			 gtk_toggle_button_get_active(cd->labels_default));
    inifile_set_gboolean(INI_SETTING_BUFPOS,
			 gtk_toggle_button_get_active(cd->bufpos_default));
    inifile_set(INI_SETTING_MIXER,
		(char *)gtk_entry_get_text(cd->mixer_utility));
    inifile_set_gboolean("useGeometry",
			 gtk_toggle_button_get_active(cd->remember_geometry));
    inifile_set_gboolean("drawImprove",
			 gtk_toggle_button_get_active(cd->improve));
    inifile_set_gboolean("mainwinFront",
			 gtk_toggle_button_get_active(cd->mainwin_front));

    default_time_mode = combo_selected_index(cd->time_display);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY,default_time_mode);

    default_time_mode = combo_selected_index(cd->time_display);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY,default_time_mode);

    default_timescale_mode = combo_selected_index(cd->time_display_timescale);
    inifile_set_guint32(INI_SETTING_TIME_DISPLAY_SCALE,default_timescale_mode);

    sound_lock_driver = gtk_toggle_button_get_active(cd->sound_lock);
    inifile_set_gboolean("soundLock",sound_lock_driver);

    status_bar_roll_cursor = gtk_toggle_button_get_active(cd->roll_cursor);
    inifile_set_gboolean("rollCursor",status_bar_roll_cursor);

    output_byteswap_flag = gtk_toggle_button_get_active(cd->output_bswap);
    inifile_set_gboolean("outputByteswap",output_byteswap_flag);

    output_stereo_flag = gtk_toggle_button_get_active(cd->output_stereo);
    inifile_set_gboolean("outputStereo",output_stereo_flag);

    view_follow_strict_flag = 
	 gtk_toggle_button_get_active(cd->center_cursor);
    inifile_set_gboolean("centerCursor",view_follow_strict_flag);

    autoplay_mark_flag = gtk_toggle_button_get_active(cd->mark_autoplay);
    inifile_set_gboolean("autoPlayMark",autoplay_mark_flag);

    l = NULL;
    model = GTK_TREE_MODEL(cd->tempdirs);
    valid = gtk_tree_model_get_iter_first(model,&iter);
    while(valid) {
         gtk_tree_model_get (model, &iter, 0, &c, -1);
         l = g_list_append(l,c);
         valid = gtk_tree_model_iter_next (model, &iter);
    }
    set_temp_directories(l);
    g_list_free(l);

    inifile_set_guint32("recentFiles",cd->recent_files->val);

    inifile_set_guint32("vzoomMax",cd->vzoom_max->val);

    format_selector_get(cd->fallback_format,&player_fallback_format);
    format_selector_save_to_inifile(cd->fallback_format,"playerFallback");

    chunk_filter_use_floating_tempfiles = 
	 gtk_toggle_button_get_active(cd->floating_tempfiles);
    inifile_set_gboolean("tempfilesFP",chunk_filter_use_floating_tempfiles);   

    varispeed_reset_flag = 
	 gtk_toggle_button_get_active(cd->varispeed_autoreset);
    inifile_set_gboolean("speedReset",varispeed_reset_flag);

    varispeed_smooth_flag = 
	 !gtk_toggle_button_get_active(cd->varispeed_fast);
    inifile_set_gboolean("speedSmooth",varispeed_smooth_flag);

    b = gtk_toggle_button_get_active(cd->varispeed_enable);
    inifile_set_gboolean("varispeed",b);
    mainwindow_set_speed_sensitive(b);
    inifile_set_guint32("varispeedConv",
			combo_selected_index(cd->varispeed_method));
    inifile_set_guint32("speedConv",combo_selected_index(cd->speed_method));

    dither_editing = gtk_toggle_button_get_active(cd->dither_editing);
    inifile_set_guint32("ditherEditing",
			dither_editing?DITHER_MINIMAL:DITHER_NONE);
    dither_playback = gtk_toggle_button_get_active(cd->dither_playback);
    inifile_set_guint32("ditherPlayback",
			dither_playback?DITHER_MINIMAL:DITHER_NONE);

    if (sndfile_ogg_supported())
	 inifile_set_guint32("sndfileOggMode",
			     combo_selected_index(cd->oggmode));

    sample_convert_mode = combo_selected_index(cd->convmode);
    inifile_set_guint32("sampleMode",sample_convert_mode);

    gtk_widget_destroy(GTK_WIDGET(cd));

    mainwindow_update_texts();
}

static void sound_settings_click(GtkButton *button, gpointer user_data)
{
    ConfigDialog *cd = CONFIG_DIALOG(user_data);
    int index = gtk_combo_box_get_active (GTK_COMBO_BOX (cd->sound_driver));
    sound_driver_show_preferences(sound_driver_id_from_index (index));
}

static void sound_driver_changed(GtkComboBox *combo, gpointer user_data)
{
    ConfigDialog *cd = CONFIG_DIALOG(user_data);
    gchar *driver_name;
    driver_name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
    if (driver_name) {
        gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver_prefs),
            sound_driver_has_preferences (sound_driver_id_from_name (driver_name)));
        g_free(driver_name);
    }
}

static void color_select(GtkTreeSelection *sel, gpointer user_data)
{
     GtkColorSelection *cs = GTK_COLOR_SELECTION(user_data);
     GdkColor *c;
     GtkTreeModel *model;
     GtkTreeIter iter;
     if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
          gtk_tree_model_get (model, &iter, 2, &c, -1);
          gtk_color_selection_set_current_color(cs, c);
          gtk_color_selection_set_previous_color(cs, c);
     }
}

static void color_set(GtkColorSelection *cs, gpointer user_data)
{
     GtkTreeSelection *sel = GTK_TREE_SELECTION(user_data);
     GdkColor c;
     GtkTreeModel *model;
     GtkTreeIter iter;
     GdkPixbuf *pixbuf;
     guint32 px;
     gtk_color_selection_get_current_color(cs,&c);
     if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,FALSE,8,20,20);
	  px = 0;
	  px += c.red / 256 << 24;
	  px += c.green / 256 << 16;
	  px += c.blue / 256 << 8;
	  px += 255;
	  gdk_pixbuf_fill(pixbuf,px);
	  gtk_list_store_set(GTK_LIST_STORE(model),&iter,
	                     0,pixbuf,2,&c,-1);
     }
}

static void color_apply(GtkButton *button, gpointer user_data)
{
     GtkListStore *store = GTK_LIST_STORE(user_data);
     GdkColor *c;
     GdkColor *ctable;
     GtkTreeModel *model;
     GtkTreeIter iter;
     gint i;
     gboolean valid;
     ctable = g_malloc((LAST_COLOR-FIRST_CUSTOM_COLOR)*sizeof(GdkColor));
     model = GTK_TREE_MODEL(store);
     valid = gtk_tree_model_get_iter_first(model,&iter);
     i = 0;
     while(valid) {
          gtk_tree_model_get (model, &iter, 2, &c, -1);
          memcpy(&ctable[i],c,sizeof(GdkColor));
          valid = gtk_tree_model_iter_next (model, &iter);
          i++;
     }
     set_custom_colors(ctable);
     g_free(ctable);
}

static void colors_dlg_response (GtkDialog * dlg, int response, gpointer user_data)
{
   switch (response)
   {
      case GTK_RESPONSE_OK:
         color_apply (NULL, user_data);
         save_colors ();
         break;
      case GTK_RESPONSE_APPLY: /* preview */
         color_apply (NULL, user_data);
         g_signal_stop_emission_by_name (dlg, "response");
         return;
         break;
      default:
         set_custom_colors (NULL);
         break;
   }
   gtk_widget_destroy (GTK_WIDGET (dlg));
}

static void colors_click(GtkButton *button, gpointer user_data)
{
     GtkWidget * dialog,*b,*c,*d;
     GtkWidget *cs;
     gint i;
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GdkColor *ctable;
     GtkListStore *store;
     GtkTreeSelection *sel;
     GtkTreeIter iter;
     GtkTreeViewColumn *col1, *col2;
     GtkCellRenderer *rend1, *rend2;
          
     ctable = g_malloc((LAST_COLOR-FIRST_CUSTOM_COLOR)*sizeof(GdkColor));
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++)
	  memcpy(&ctable[i-FIRST_CUSTOM_COLOR],get_color(i),sizeof(GdkColor));

     cs = gtk_color_selection_new();
     gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION(cs), 
						  FALSE);
     gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION(cs), TRUE);

     dialog = gtk_dialog_new();
     gtk_window_set_modal (GTK_WINDOW (dialog),TRUE);
     gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (cd));
     gtk_window_set_title (GTK_WINDOW (dialog),_("Colors"));
     gtk_window_set_resizable (GTK_WINDOW (dialog),FALSE);

     b = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
     gtk_box_set_spacing (GTK_BOX (b), 5);
     gtk_container_set_border_width(GTK_CONTAINER(b),5);
     c = gtk_hbox_new(FALSE,10);
     gtk_box_pack_start(GTK_BOX(b),c,FALSE,FALSE,0);
     d = gtk_tree_view_new();
     gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(d),FALSE);
     rend1 = gtk_cell_renderer_pixbuf_new();
     col1 = gtk_tree_view_column_new_with_attributes(NULL,rend1,"pixbuf",0,NULL);
     rend2 = gtk_cell_renderer_text_new();
     col2 = gtk_tree_view_column_new_with_attributes(NULL,rend2,"text",1,NULL);
     gtk_tree_view_append_column(GTK_TREE_VIEW(d),col1);
     gtk_tree_view_append_column(GTK_TREE_VIEW(d),col2);
     sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(d));
     g_signal_connect(sel,"changed",
                      G_CALLBACK(color_select),cs);
     g_signal_connect(G_OBJECT(cs),"color_changed",
		      G_CALLBACK(color_set),sel);
     gtk_tree_selection_set_mode(sel,GTK_SELECTION_BROWSE);
     gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
     store = gtk_list_store_new(3,GDK_TYPE_PIXBUF,G_TYPE_STRING,GDK_TYPE_COLOR);
     gtk_tree_view_set_model(GTK_TREE_VIEW(d),GTK_TREE_MODEL(store));
     GdkPixbuf *pixbuf;
     guint32 px;
     for (i=FIRST_CUSTOM_COLOR; i<LAST_COLOR; i++) {
	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,FALSE,8,20,20);
	  px = 0;
	  px += ctable[i-FIRST_CUSTOM_COLOR].red / 256 << 24;
	  px += ctable[i-FIRST_CUSTOM_COLOR].green / 256 << 16;
	  px += ctable[i-FIRST_CUSTOM_COLOR].blue / 256 << 8;
	  px += 255;
	  gdk_pixbuf_fill(pixbuf,px);
	  gtk_list_store_append(store,&iter);
	  gtk_list_store_set(store,&iter,0,pixbuf,
	                                 1,_(color_names[i]),
	                                 2,&ctable[i-FIRST_CUSTOM_COLOR],-1);
     }
     g_free(ctable);
     d = cs;
     gtk_box_pack_start(GTK_BOX(c),d,TRUE,TRUE,0);

     gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
     gtk_tree_selection_select_iter (sel, &iter);

     d = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Preview"), GTK_RESPONSE_APPLY);
     d = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);
     d = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);

     g_signal_connect (dialog, "response",
                       G_CALLBACK (colors_dlg_response), store);
     gtk_widget_show_all(dialog);
}

static void tempdir_add_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gchar *ch;
     GtkTreeIter iter;
     ch = (gchar *)gtk_entry_get_text(cd->tempdir_add_entry);
     if (ch[0] == 0) return;
     gtk_list_store_append(cd->tempdirs,&iter);
     gtk_list_store_set(cd->tempdirs,&iter,0,ch,-1);
     gtk_entry_set_text(cd->tempdir_add_entry,"");
}

static void tempdir_remove_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkTreeModel *model;
     GtkTreeIter iter;
     if (gtk_tree_selection_get_selected (cd->tempsel, &model, &iter))
          gtk_list_store_remove (cd->tempdirs, &iter);
}

static void tempdir_up_down_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkTreeModel *model;
     GtkTreeIter iter1, iter2;
     GtkTreePath *path1, *path2;
     if (gtk_tree_selection_get_selected (cd->tempsel, &model, &iter1)) {
          path1 = gtk_tree_model_get_path (model, &iter1);
          path2 = gtk_tree_path_copy (path1);
          if (button == cd->tempdir_up)
	       gtk_tree_path_prev (path2);
	  else
	       gtk_tree_path_next (path2);
          if (gtk_tree_path_compare (path1,path2)) {
               if (gtk_tree_model_get_iter (model, &iter2, path2))
                    gtk_list_store_swap (cd->tempdirs, &iter1, &iter2);
          }
          gtk_tree_path_free(path1);
          gtk_tree_path_free(path2);
     }
}

static void tempdir_browse_click(GtkButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gchar *c,*d;
     c = (gchar *)gtk_entry_get_text(cd->tempdir_add_entry);
     d = get_directory(c,_("Browse directory"));
     if (d != NULL) {
	  gtk_entry_set_text(cd->tempdir_add_entry,d);
	  g_free(d);
     }
}


static void tempdir_select_changed(GtkTreeSelection *sel,
			   gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     GtkTreeModel *model;
     GtkTreeIter iter;
     gboolean state;
     state = gtk_tree_selection_get_selected (sel, &model, &iter);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_remove),state);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_up),state);
     gtk_widget_set_sensitive(GTK_WIDGET(cd->tempdir_down),state);
}

static void driver_autodetect_toggled (GtkToggleButton *button, gpointer user_data)
{
     ConfigDialog *cd = CONFIG_DIALOG(user_data);
     gboolean b;
     b = gtk_toggle_button_get_active(button);
     if (b) {
         gtk_combo_box_set_active (GTK_COMBO_BOX (cd->sound_driver),
                                   sound_driver_index());
     }
     gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver), !b);
}


static void config_dlg_response (GtkDialog * dlg, int response, gpointer user_data)
{
   if (response == GTK_RESPONSE_OK) {
      config_dialog_ok (NULL, user_data);
   }
   gtk_widget_destroy (GTK_WIDGET (dlg));
}

static void config_dialog_init(ConfigDialog *cd)
{
    GtkWidget *w,*a,*b,*c,*d,*e,*f,*g,*tempview;
    GList *l;
    guint i,j;
    gchar *ch;
    GtkTreeIter iter;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;

    gtk_window_set_title(GTK_WINDOW(cd),_("Preferences"));
    gtk_window_set_modal(GTK_WINDOW(cd),TRUE);
    gtk_window_set_position(GTK_WINDOW (cd), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(cd),10,400);
    gtk_container_set_border_width(GTK_CONTAINER(cd),5);

    /* Create the main widgets */

    w = intbox_new( inifile_get_guint32( INI_SETTING_REALMAX, 
					 INI_SETTING_REALMAX_DEFAULT )/1024);
    cd->disk_threshold = INTBOX(w);

    w = intbox_new(inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
                                       INI_SETTING_SOUNDBUFSIZE_DEFAULT));
    cd->sound_buffer_size = INTBOX(w);

    cd->sound_driver = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
    l = sound_driver_valid_names();
    w_gtk_glist_to_combo (GTK_COMBO_BOX (cd->sound_driver), l,
                          sound_driver_index ());
    g_signal_connect(G_OBJECT(cd->sound_driver),"changed",
                     G_CALLBACK(sound_driver_changed),cd);
    g_list_free (l);

    i = rateconv_driver_count(TRUE);
    for (l=NULL,j=0; j<i; j++)
	 l = g_list_append(l,(gpointer)rateconv_driver_name(TRUE,j));

    w = combo_new();
    cd->varispeed_method = COMBO(w);
    j = inifile_get_guint32("varispeedConv",i-1);
    if (j >= i) j = i-1;
    combo_set_items(cd->varispeed_method, l, j);

    g_list_free(l);

    i = rateconv_driver_count(FALSE);
    for (l=NULL,j=0; j<i; j++)
	 l = g_list_append(l,(gpointer)rateconv_driver_name(FALSE,j));

    w = combo_new();
    cd->speed_method = COMBO(w);
    j = inifile_get_guint32("speedConv",0);
    if (j >= i) j = 0;
    combo_set_items(cd->speed_method, l, j);

    g_list_free(l);

    w = gtk_entry_new();
    cd->mixer_utility = GTK_ENTRY(w);
    gtk_entry_set_text(cd->mixer_utility,inifile_get(INI_SETTING_MIXER,
						     INI_SETTING_MIXER_DEFAULT));


    w = intbox_new(inifile_get_guint32(INI_SETTING_VIEW_QUALITY,
				       INI_SETTING_VIEW_QUALITY_DEFAULT));
    cd->view_quality = INTBOX(w);

    w = gtk_check_button_new_with_mnemonic(_("Show _time scale by default"));
    cd->time_scale_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->time_scale_default,inifile_get_gboolean(
				      INI_SETTING_TIMESCALE, 
				      INI_SETTING_TIMESCALE_DEFAULT));

    w = gtk_check_button_new_with_mnemonic(_("Show _horizontal zoom slider by "
                                             "default"));
    cd->hzoom_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->hzoom_default,inifile_get_gboolean(
				      INI_SETTING_HZOOM, 
				      INI_SETTING_HZOOM_DEFAULT));

    w = gtk_check_button_new_with_mnemonic(_("Show _vertical zoom slider by "
                                             "default"));
    cd->vzoom_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->vzoom_default,inifile_get_gboolean(
				      INI_SETTING_VZOOM, 
				      INI_SETTING_VZOOM_DEFAULT));

    w = gtk_check_button_new_with_mnemonic(_("Show _speed slider by default"));
    cd->speed_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->speed_default,inifile_get_gboolean(
				      INI_SETTING_SPEED, 
				      INI_SETTING_SPEED_DEFAULT));

    w = gtk_check_button_new_with_mnemonic(_("Show slider l_abels by default"));
    cd->labels_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->labels_default,
				 inifile_get_gboolean(INI_SETTING_SLABELS,
						      INI_SETTING_SLABELS_DEFAULT));

    w = gtk_check_button_new_with_mnemonic(_("Show playback buffer positio_n "
                                             "by default"));
    cd->bufpos_default = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->bufpos_default,
				 inifile_get_gboolean(INI_SETTING_BUFPOS,
						      INI_SETTING_BUFPOS_DEFAULT));

    w = gtk_button_new_with_mnemonic (_("_Settings"));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(sound_settings_click),cd);
    gtk_widget_set_sensitive(w,sound_driver_has_preferences(NULL));
    cd->sound_driver_prefs = GTK_BUTTON(w);

    w = gtk_check_button_new_with_mnemonic(_("_Keep sound driver opened (to "
                                             "avoid start/stop clicks)"));
    cd->sound_lock = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->sound_lock,sound_lock_driver);

    w = gtk_check_button_new_with_mnemonic(_("_Byte-swap output (try this if "
                                             "playback sounds horrible)"));
    cd->output_bswap = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->output_bswap,output_byteswap_flag);

    w = gtk_check_button_new_with_mnemonic(_("Play _mono files as stereo"));
    cd->output_stereo = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->output_stereo,output_stereo_flag);

    w = gtk_check_button_new_with_mnemonic(_("_Update cursor information "
                                             "while playing"));
    cd->roll_cursor = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->roll_cursor,status_bar_roll_cursor);

    w = gtk_check_button_new_with_mnemonic(_("_Keep cursor in center of view "
                                             "when following playback"));
    cd->center_cursor = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->center_cursor,
				 view_follow_strict_flag);

    w = gtk_check_button_new_with_mnemonic(_("_Auto-start playback when "
                                             "jumping to mark"));
    cd->mark_autoplay = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->mark_autoplay,autoplay_mark_flag);

    w = gtk_check_button_new_with_mnemonic(_("Enable _variable speed playback"));
    cd->varispeed_enable = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_enable,
				 inifile_get_gboolean("varispeed",TRUE));

    w = gtk_check_button_new_with_mnemonic(_("Auto-_reset speed"));
    cd->varispeed_autoreset = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_autoreset,varispeed_reset_flag);
    
    w = gtk_check_button_new_with_mnemonic(_("Use fast and noisy method"));
    cd->varispeed_fast = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->varispeed_fast,!varispeed_smooth_flag);
    
    w = combo_new();
    cd->time_display = COMBO(w);
    l = NULL;
    l = g_list_append(l,_("(H')MM:SS.t"));
    l = g_list_append(l,_("(H')MM:SS.mmmm"));
    l = g_list_append(l,translate_strip(N_("TimeDisplay|Samples")));
    l = g_list_append(l,_("Time Code 24fps"));
    l = g_list_append(l,_("Time Code 25fps (PAL)"));
    l = g_list_append(l,_("Time Code 29.97fps (NTSC)"));
    l = g_list_append(l,_("Time Code 30fps"));
    i = inifile_get_guint32(INI_SETTING_TIME_DISPLAY,0);
    combo_set_items(cd->time_display,l,i);

    w = combo_new();
    cd->time_display_timescale = COMBO(w);
    i = inifile_get_guint32(INI_SETTING_TIME_DISPLAY_SCALE,i);
    combo_set_items(cd->time_display_timescale,l,i);

    g_list_free(l);

    w = gtk_check_button_new_with_mnemonic(_("_Remember window "
                                             "sizes/positions"));
    cd->remember_geometry = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->remember_geometry,
				 inifile_get_gboolean("useGeometry",FALSE));

    w = gtk_check_button_new_with_mnemonic(_("_Draw waveform a second time "
                                             "with improved quality"));
    cd->improve = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->improve,
				 inifile_get_gboolean("drawImprove",TRUE));


    tempview = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tempview),FALSE);
    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(NULL,renderer,"text",0,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tempview),col);
    cd->tempsel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tempview));
    gtk_tree_selection_set_mode(cd->tempsel,GTK_SELECTION_SINGLE);
    cd->tempdirs = gtk_list_store_new(1,G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(tempview),GTK_TREE_MODEL(cd->tempdirs));
    for (i=0; 1; i++) {
	 ch = get_temp_directory(i);
	 if (ch == NULL) break;
         gtk_list_store_append(cd->tempdirs,&iter);
         gtk_list_store_set(cd->tempdirs,&iter,0,ch,-1);
    }
    g_signal_connect(cd->tempsel,"changed",
                     G_CALLBACK(tempdir_select_changed),cd);

    w = gtk_entry_new();
    cd->tempdir_add_entry = GTK_ENTRY(w);


    w = gtk_button_new_with_mnemonic(_("_Remove"));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(tempdir_remove_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_remove = GTK_BUTTON(w);
    w = gtk_button_new_with_mnemonic(_("_Add"));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(tempdir_add_click),cd);
    cd->tempdir_add = GTK_BUTTON(w);
    w = gtk_button_new_with_mnemonic(_("_Browse..."));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(tempdir_browse_click),cd);
    cd->tempdir_browse = GTK_BUTTON(w);
    w = gtk_button_new_with_mnemonic(_("_Up"));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(tempdir_up_down_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_up = GTK_BUTTON(w);
    w = gtk_button_new_with_mnemonic(_("_Down"));
    g_signal_connect(G_OBJECT(w),"clicked",
		       G_CALLBACK(tempdir_up_down_click),cd);
    gtk_widget_set_sensitive(w,FALSE);
    cd->tempdir_down = GTK_BUTTON(w);

    w = intbox_new(inifile_get_guint32("recentFiles",4));
    cd->recent_files = INTBOX(w);

    w = intbox_new(inifile_get_guint32("vzoomMax",100));
    cd->vzoom_max = INTBOX(w);
    
    w = gtk_check_button_new_with_mnemonic(_("Keep main _window in front after "
				             "applying effect"));
    cd->mainwin_front = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->mainwin_front,
				 inifile_get_gboolean("mainwinFront",TRUE));


    w = format_selector_new(TRUE);
    format_selector_set(FORMAT_SELECTOR(w),&player_fallback_format);
    cd->fallback_format = FORMAT_SELECTOR(w);


    w = gtk_check_button_new_with_mnemonic(_("_Use floating-point temporary "
                                             "files"));
    cd->floating_tempfiles = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->floating_tempfiles,
				 chunk_filter_use_floating_tempfiles);

    w = gtk_check_button_new_with_mnemonic(_("Enable _dithering for editing"));
    cd->dither_editing = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->dither_editing,dither_editing);
    
    w = gtk_check_button_new_with_mnemonic(_("Enable dithering for _playback"));
    cd->dither_playback = GTK_TOGGLE_BUTTON(w);
    gtk_toggle_button_set_active(cd->dither_playback,dither_playback);

    w = gtk_check_button_new_with_mnemonic(_("Auto dete_ct driver on each "
                                             "start-up"));
    cd->driver_autodetect = GTK_TOGGLE_BUTTON(w);
    if (!strcmp(inifile_get(INI_SETTING_SOUNDDRIVER,DEFAULT_DRIVER),"auto")) {
	 gtk_widget_set_sensitive(GTK_WIDGET(cd->sound_driver),FALSE);
	 gtk_toggle_button_set_active(cd->driver_autodetect,TRUE);
    }
    g_signal_connect(G_OBJECT(w),"toggled",
		       G_CALLBACK(driver_autodetect_toggled),cd);

    w = combo_new();
    l = NULL;
    l = g_list_append(l,_("Direct access"));
    l = g_list_append(l,_("Decompress"));
    l = g_list_append(l,_("Bypass"));
    combo_set_items(COMBO(w),l,inifile_get_guint32("sndfileOggMode",1));
    g_list_free(l);
    if (!sndfile_ogg_supported()) {
	 combo_set_selection(COMBO(w),2);
	 gtk_widget_set_sensitive(w,FALSE);
    }
    cd->oggmode = COMBO(w);

    w = combo_new();
    l = NULL;
    l = g_list_append(l,_("Biased"));
    l = g_list_append(l,_("Pure-Scaled"));
    combo_set_items(COMBO(w),l,sample_convert_mode);
    g_list_free(l);
    cd->convmode = COMBO(w);

    /* Layout the window */

    a = gtk_dialog_get_content_area (GTK_DIALOG (cd));
    b = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(b),GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(a),b,TRUE,TRUE,0);



    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Interface")));
    d = gtk_frame_new(_(" Main window "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->remember_geometry);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->mainwin_front);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Number of recent files in File menu: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->recent_files);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);

    d = gtk_frame_new(_(" View "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->improve);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_button_new_with_mnemonic(_("Customize co_lors..."));
    g_signal_connect(G_OBJECT(g),"clicked",G_CALLBACK(colors_click),
		       cd);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Window contents "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->time_scale_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->vzoom_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->speed_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->labels_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->bufpos_default);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Vertical zoom limit: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->vzoom_max);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);
    g = gtk_label_new(_("x"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);


    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Sound")));
    d = gtk_frame_new(_(" Driver options "));    
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new_with_mnemonic(_("_Driver:"));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g), GTK_WIDGET(cd->sound_driver));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->sound_driver);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);
    g = GTK_WIDGET(cd->sound_driver_prefs);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,3);
    f = GTK_WIDGET(cd->driver_autodetect);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->output_bswap);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->sound_lock);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->output_stereo);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Fallback format "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_label_new(_("Sample format to try when the sound file's format"
		        " isn't supported."));
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->fallback_format);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    
    


    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Playback")));

    d = gtk_frame_new(_(" Playback settings "));    
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->mark_autoplay);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->roll_cursor);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->center_cursor);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    
    d = gtk_frame_new(_(" Variable speed "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = GTK_WIDGET(cd->varispeed_enable);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->varispeed_fast);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->varispeed_autoreset);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);    
    

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Files")));

    d = gtk_frame_new(_(" Temporary file directories "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_table_new(5,3,FALSE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_scrolled_window_new(NULL,NULL);
    gtk_table_attach(GTK_TABLE(f),g,0,2,0,4,GTK_EXPAND|GTK_FILL,GTK_FILL,0,0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g),GTK_POLICY_NEVER,
				   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(g),tempview);
    g = GTK_WIDGET(cd->tempdir_up);
    gtk_table_attach(GTK_TABLE(f),g,2,3,0,1,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_down);
    gtk_table_attach(GTK_TABLE(f),g,2,3,1,2,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_remove);
    gtk_table_attach(GTK_TABLE(f),g,2,3,2,3,GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->tempdir_add_entry);
    gtk_table_attach(GTK_TABLE(f),g,0,1,4,5,GTK_EXPAND|GTK_FILL,0,0,4);
    g = GTK_WIDGET(cd->tempdir_browse);
    gtk_table_attach(GTK_TABLE(f),g,1,2,4,5,GTK_FILL,0,0,4);
    g = GTK_WIDGET(cd->tempdir_add);
    gtk_table_attach(GTK_TABLE(f),g,2,3,4,5,GTK_FILL,0,0,4);

    d = gtk_frame_new(_(" Temporary file settings "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_label_new(_("To avoid rounding errors when applying more than one "
		      "effect on the same data, floating-point temporary "
		      "files can be used. However, this will increase disk "
		      "and CPU usage."));
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->floating_tempfiles);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = gtk_label_new(_("Some versions of libsndfile supports accessing "
			"Ogg files without decompressing to a temporary "
			"file."));
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    gtk_label_set_line_wrap(GTK_LABEL(f),TRUE);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new("Libsndfile ogg handling: ");
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->oggmode);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,0);

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Quality")));
    
    d = gtk_frame_new(_(" Rate conversions "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    f = gtk_table_new(2,2,FALSE);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    attach_label(_("Varispeed: "),f,0,0);
    attach_label(_("Speed effect: "),f,1,0);
    g = GTK_WIDGET(cd->varispeed_method);
    gtk_table_attach(GTK_TABLE(f),g,1,2,0,1,GTK_EXPAND|GTK_FILL,0,0,0);
    g = GTK_WIDGET(cd->speed_method);
    gtk_table_attach(GTK_TABLE(f),g,1,2,1,2,GTK_EXPAND|GTK_FILL,0,0,0);
    gtk_table_set_row_spacings(GTK_TABLE(f),4);

    d = gtk_frame_new(_(" Dithering "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);

    f = GTK_WIDGET(cd->dither_editing);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    f = GTK_WIDGET(cd->dither_playback);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);

    d = gtk_frame_new(_(" Sample conversion "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    f = gtk_hbox_new(FALSE,0);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new(_("Normalization mode: "));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->convmode);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,0);

    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Other")));

    d = gtk_frame_new(_(" Time format "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);    

    e = gtk_table_new(2,2,FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    gtk_table_set_row_spacings(GTK_TABLE(e),4);

    g = gtk_label_new_with_mnemonic(_("Display t_imes as: "));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g), GTK_WIDGET(cd->time_display));
    gtk_misc_set_alignment(GTK_MISC(g),0.0,0.5);
    gtk_table_attach(GTK_TABLE(e),g,0,1,0,1,GTK_FILL,0,0,0);

    g = GTK_WIDGET(cd->time_display);
    gtk_table_attach(GTK_TABLE(e),g,1,2,0,1,GTK_FILL|GTK_EXPAND,0,0,0);

    g = gtk_label_new_with_mnemonic(_("Times_cale format: "));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g),
                                   GTK_WIDGET(cd->time_display_timescale));
    gtk_misc_set_alignment(GTK_MISC(g),0.0,0.5);
    gtk_table_attach(GTK_TABLE(e),g,0,1,1,2,GTK_FILL,0,0,0);

    g = GTK_WIDGET(cd->time_display_timescale);
    gtk_table_attach(GTK_TABLE(e),g,1,2,1,2,GTK_FILL|GTK_EXPAND,0,0,0);



    d = gtk_frame_new(_(" External applications "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,3);
    g = gtk_label_new_with_mnemonic(_("Mi_xer utility: "));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g), GTK_WIDGET(cd->mixer_utility));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->mixer_utility);
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,8);

    
    c = gtk_vbox_new(FALSE,14);
    gtk_container_set_border_width(GTK_CONTAINER(c),4);
    gtk_notebook_append_page(GTK_NOTEBOOK(b),c,gtk_label_new(_("Advanced")));
    d = gtk_frame_new(_(" Advanced settings "));
    gtk_box_pack_start(GTK_BOX(c),d,FALSE,FALSE,0);
    e = gtk_vbox_new(FALSE,3);
    gtk_container_set_border_width(GTK_CONTAINER(e),5);
    gtk_container_add(GTK_CONTAINER(d),e);
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new_with_mnemonic(_("Disk editing _threshold: "));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g),
                                   GTK_WIDGET(cd->disk_threshold));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);    
    g = GTK_WIDGET(cd->disk_threshold);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new("K ");
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    
    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);
    g = gtk_label_new_with_mnemonic(_("View _quality:"));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g), GTK_WIDGET(cd->view_quality));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->view_quality);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new(_("samples/pixel"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);    

    f = gtk_hbox_new(FALSE,3);
    gtk_box_pack_start(GTK_BOX(e),f,FALSE,FALSE,0);    
    g = gtk_label_new_with_mnemonic(_("Output _buffer size:"));
    gtk_label_set_mnemonic_widget (GTK_LABEL(g),
                                   GTK_WIDGET(cd->sound_buffer_size));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    g = GTK_WIDGET(cd->sound_buffer_size);
    gtk_box_pack_start(GTK_BOX(f),g,TRUE,TRUE,8);
    g = gtk_label_new(_("bytes"));
    gtk_box_pack_start(GTK_BOX(f),g,FALSE,FALSE,0);
    

    gtk_dialog_add_button (GTK_DIALOG (cd), _("_OK"), GTK_RESPONSE_OK);
    gtk_dialog_add_button (GTK_DIALOG (cd), _("_Close"), GTK_RESPONSE_OK);
    g_signal_connect (cd, "response", G_CALLBACK (config_dlg_response), cd);
    gtk_widget_show_all(a);
}

GtkWidget *config_dialog_new(void)
{
     return g_object_new(CONFIG_DIALOG_TYPE, NULL);
}
