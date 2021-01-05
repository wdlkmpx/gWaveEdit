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


#ifndef VU_METER_H_INCLUDED
#define VU_METER_H_INCLUDED

#define VU_METER_TYPE          (vu_meter_get_type ())
#define VU_METER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), VU_METER_TYPE, VuMeter))
#define IS_VU_METER(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VU_METER_TYPE))
#define VU_METER_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  VU_METER_TYPE, VuMeterClass))

typedef struct {
     GtkDrawingArea da;
     gfloat value; /* Between 0 and 1 */
     gfloat goal;
     GTimeVal valuetime;
} VuMeter;

typedef struct {
     GtkDrawingAreaClass dac;
} VuMeterClass;

GType vu_meter_get_type(void);
GtkWidget *vu_meter_new(gfloat value);
void vu_meter_set_value(VuMeter *v, gfloat value);
void vu_meter_update(VuMeter *v);

#endif
