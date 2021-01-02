/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
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


#ifndef SPEED_DIALOG_H_INCLUDED
#define SPEED_DIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "mainwindow.h"
#include "float_box.h"
#include "effectdialog.h"

#define SPEED_DIALOG_TYPE          (speed_dialog_get_type ())
#define SPEED_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPEED_DIALOG_TYPE, SpeedDialog))
#define IS_SPEED_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPEED_DIALOG_TYPE))
#define SPEED_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  SPEED_DIALOG_TYPE, SpeedDialogClass))

typedef struct _SpeedDialog {
     EffectDialog ed;
     Floatbox *speed;
} SpeedDialog;

typedef struct _SpeedDialogClass {
     EffectDialogClass ed_class;
} SpeedDialogClass;

GType speed_dialog_get_type(void);

#endif
