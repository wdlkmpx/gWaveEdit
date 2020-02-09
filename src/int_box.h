/*
 * Copyright (C) 2002 2003 2005, Magnus Hjorth
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


#ifndef INTBOX_H_INCLUDED
#define INTBOX_H_INCLUDED

#define INTBOX_TYPE          (intbox_get_type ())
#define INTBOX(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), INTBOX_TYPE, Intbox))
#define IS_INTBOX(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), INTBOX_TYPE))
#define INTBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  INTBOX_TYPE, IntboxClass))

typedef struct _Intbox Intbox;
typedef struct _IntboxClass IntboxClass;

struct _Intbox {
     GtkEntry parent;
     long int val;
     GtkAdjustment *adj;
};

struct _IntboxClass {
	GtkEntryClass parent;
	void (*numchange)(Intbox *, long int);
	};

GType intbox_get_type(void);
GtkWidget *intbox_new(long value);
void intbox_set(Intbox *box, long value);

/* TRUE = Contains invalid value */
gboolean intbox_check(Intbox *box);
gboolean intbox_check_limit(Intbox *box, long int lowest, long int highest,
			    gchar *valuename);
GtkWidget *intbox_create_scale(Intbox *box, long minval, long maxval);


#endif



