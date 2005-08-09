/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 */


#ifndef OUTPUT_H_INCLUDED
#define OUTPUT_H_INCLUDED

#include <gtk/gtk.h>
#include "main.h"
#include "dataformat.h"
#include "ringbuf.h"



/* Possible states for sound driver:
   1. Uninitialized (starting state)
   2. Idle
   3. Playing (or waiting for output)
   4. Recording
   5. Quitted (ending state)
*/

/* Global variable for selecting whether the sound driver should "lock" itself
 * onto the device. 
 */

extern gboolean sound_lock_driver;

/* Flag that decides whether output data should be byte-swapped. */

extern gboolean output_byteswap_flag;


/* ---------------------------------
   Functions available in all states 
   --------------------------------- */


/* Returns the name of the current output driver. The string should not be 
 * freed or modified.
 */

gchar *sound_driver_name(void);


/* Returns the ID of the current output driver. The string should not be
 * freed or modified.
 */

gchar *sound_driver_id(void);


/* Returns the index of the current output driver. */

int sound_driver_index(void);


/* Returns a list of valid soundDriver names */

GList *sound_driver_valid_names(void);


/* Translates a soundDriver name into an id. Returns NULL if name is not a 
 * valid name. 
 */

gchar *sound_driver_id_from_name(gchar *name);


gchar *sound_driver_id_from_index(int index);

/* Translates a soundDriver id into a name. Returns NULL if id is not a valid 
 * driver ID.
 */

gchar *sound_driver_name_from_id(gchar *id);


/* Returns whether a sound driver has a preferences dialog. If id==NULL the 
 * current driver is used.
 */

gboolean sound_driver_has_preferences(gchar *id);


/* Show the preferences dialog for a sound driver (if it exists). If id==NULL
 * the current driver is used.
 */

void sound_driver_show_preferences(gchar *id);



/* ----------------------------------------------
 * Functions available in state 1 (Uninitialized) 
 * ---------------------------------------------- */


/* Sound module initialization.
 * Called before gtk_init.
 * Changes state to state 2 (Idle).
 */

void sound_init(void);




/* -------------------------------------
 * Functions available in state 2 (Idle)
 * ------------------------------------- */


/* Sound module cleanup.
 * Changes state to state 5 (Quitted)
 */

void sound_quit(void);


/* Select which format to play and setup playing.
 * If the format isn't supported by the driver, the function does nothing and 
 * returns <0 if no message displayed or >0 if message was displayed. 
 * Changes state to state 3 (Playing) and returns FALSE if successful
 */

gint output_select_format(Dataformat *format, gboolean silent);


/* Suggest a format to use for playing back data of the input format.
 * Returns FALSE if no suggestion is available */
gboolean output_suggest_format(Dataformat *format, Dataformat *result);


/* output_stop() - does nothing in this state */


/* Returns TRUE if input is supported by the driver. 
 * The other input_*-routines will not be called if this function returns FALSE
 */

gboolean input_supported(void);




/* Select which format to record and setup recording.
 * If the format isn't supported by the driver, the function does nothing and 
 * returns <0 if no message displayed or >0 if message was displayed. 
 * Changes state to state 4 (Recording) and returns FALSE if successful.
 */

gint input_select_format(Dataformat *format, gboolean silent);



/* input_stop() - does nothing in this state */





/* ----------------------------------------
 * Functions available in state 3 (Playing)
 * ---------------------------------------- */


/* Stops playback.
 * Changes state to state 2 (Idle).
 * Can also be called in state 2, but should do nothing in that case.
 *
 * If must_flush is true the call will output all currently buffered
 * data before stopping. If must_flush is false it can still do so if
 * the driver's buffers are small.
 *
 * Returns true if all buffered output was sent. 
 */

gboolean output_stop(gboolean must_flush);


/* Returns TRUE iff it's possible to output more data (with output_play) */

gboolean output_want_data(void); 

/* Wait for output_want_data to return TRUE or a timeout expires. This call is
 * not supported on all drivers. It returns FALSE immediately if the call is not
 * supported
 */

gboolean output_wait(guint timeout);

/* Send as much data as possible to the output device.
 * Return the amount of data sent. 
 * If bufsize=0, the driver sends out as much pre-buffered data
 * as possible without blocking and returns the amount of data left in the
 * buffers.
 */

guint output_play(gchar *buffer, guint bufsize);

/* Skips currently buffered data so we can start playing new data as quickly as
 * possible.
 */

void output_clear_buffers(void);



/* ------------------------------------------
 * Functions available in state 4 (Recording)
 * ------------------------------------------ */


/* Stops recording.
 * Changes state to state 2 (Idle).
 * Can also be called in state 2, but should do nothing in that case.
 */

void input_stop(void);


/* Hints to the sound driver that we're about to stop and we really don't want 
 * to gather any new data. */

void input_stop_hint(void);


/* Stores recorded data into the buffer parameter. */

void input_store(Ringbuf *buffer);

/* Returns number of input overruns since recording started or -1 if unknown. 
 */

int input_overrun_count(void);



#endif