/*
 * Copyright (C) 2002 2003 2004 2006, Magnus Hjorth
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


#ifndef MAPCHANNELSDIALOG_H_INCLUDED
#define MAPCHANNELSDIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "effectdialog.h"
#include "int_box.h"

#define MAP_CHANNELS_DIALOG_TYPE          (map_channels_dialog_get_type ())
#define MAP_CHANNELS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAP_CHANNELS_DIALOG_TYPE, MapChannelsDialog))
#define IS_MAP_CHANNELS_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAP_CHANNELS_DIALOG_TYPE))
#define MAP_CHANNELS__DIALOGCLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  MAP_CHANNELS_DIALOG_TYPE, MapChannelsDialogClass))

typedef struct {
     EffectDialog ed;
     guint channels_in,channels_out;
     GtkToggleButton *channel_map[8*8];
     Intbox *outchannels;
     GtkWidget *outnames[8];
} MapChannelsDialog;

typedef struct {
    EffectDialogClass edc;
} MapChannelsDialogClass;

GType map_channels_dialog_get_type();

#endif
