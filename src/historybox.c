/*
 * Copyright (C) 2003 2004 2010, Magnus Hjorth
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

#include <string.h>
#include "historybox.h"
#include "inifile.h"

#define HISTORY_BOX_MAXENTRIES 20

G_DEFINE_TYPE(HistoryBox,history_box,GTK_TYPE_COMBO_BOX_TEXT)

typedef struct {
     gchar *name;
     GList *entries;
} HistoryBoxHistory;

static GHashTable *history_hash = NULL; /* key=gchar* (malloc'd), value=GList* */

static void history_box_init(HistoryBox *box)
{
}

static void history_box_class_init(HistoryBoxClass *klass)
{
}

static void load_history(HistoryBoxHistory *hist)
{
     gint i;
     gchar c[128],*d;
     for (i=1; i<=HISTORY_BOX_MAXENTRIES; i++) {
	  g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
	  d = inifile_get(c,NULL);
	  if (!d) break;
	  hist->entries = g_list_append(hist->entries,g_strdup(d));
     }
}

static void save_history(HistoryBoxHistory *hist)
{
     gint i;
     gchar c[128];
     GList *l;
     for (i=1,l=hist->entries; l!=NULL; i++,l=l->next) {
	  g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
	  inifile_set(c,l->data);
     }
     g_snprintf(c,sizeof(c),"history_%s_%d",hist->name,i);
     inifile_set(c,NULL);
     g_assert(i<=HISTORY_BOX_MAXENTRIES+1);
}

GtkWidget *history_box_new(gchar *historyname, gchar *value)
{
     HistoryBox *hb = g_object_new(HISTORY_BOX_TYPE,
                                  "has-entry",        TRUE,
                                  "entry-text-column",   0,
                                  NULL);
     history_box_set_history(hb, historyname);
     history_box_set_value(hb, value);
     return GTK_WIDGET(hb);
}

gchar *history_box_get_value(HistoryBox *box)
{
     return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(box));
}

void history_box_set_history(HistoryBox *box, gchar *historyname)
{
     HistoryBoxHistory *hist;
     GList *glist;
     if (!history_hash) history_hash=g_hash_table_new(g_str_hash,g_str_equal);
     hist = g_hash_table_lookup(history_hash, historyname);
     if (!hist) {
	  hist = g_malloc(sizeof(*hist));
	  hist->name = g_strdup(historyname);
	  hist->entries = NULL;
	  load_history(hist);
	  g_hash_table_insert(history_hash,hist->name,hist);
     }

     glist = hist->entries;
     gtk_list_store_clear( GTK_LIST_STORE(gtk_combo_box_get_model( GTK_COMBO_BOX(box) )) );
     while (glist) {
       if (glist->data) {
          gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(box), glist->data);
       }
       glist = glist->next;
     }

     box->history = hist;
}

void history_box_set_value(HistoryBox *box, gchar *value)
{
     GtkWidget *combo_entry;
     combo_entry = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(box)));
     gtk_entry_set_text (GTK_ENTRY (combo_entry), value);
}

void history_box_rotate_history(HistoryBox *box)
{
     gchar *v;
     HistoryBoxHistory *hist=box->history;
     GList *l, *n, *glist;
     v = history_box_get_value(box);
     for (l=hist->entries; l!=NULL; l=n) {
	  n = l->next;
	  if (!strcmp(l->data,v)) {
	       g_free(l->data);
	       hist->entries = g_list_remove_link(hist->entries,l);
	       g_list_free(l);
	  }
     }
     hist->entries = g_list_prepend(hist->entries, v);
     l = g_list_nth(hist->entries, HISTORY_BOX_MAXENTRIES-1);
     if (l && l->next) {
	  g_assert(l->next->next == NULL);
	  g_free(l->next->data);
	  g_list_free_1(l->next);
	  l->next = NULL;
     }

     glist = hist->entries;
     gtk_list_store_clear( GTK_LIST_STORE(gtk_combo_box_get_model( GTK_COMBO_BOX(box) )) );
     while (glist) {
       if (glist->data) {
          gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(box), glist->data);
       }
       glist = glist->next;
     }

     save_history(hist);
}
