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


#include <config.h>

#include "sound.h"
#include "inifile.h"
#include "main.h"
#include "um.h"
#include "gettext.h"

gboolean output_byteswap_flag;
static gchar *output_byteswap_buffer=NULL;
static guint output_byteswap_bufsize=0;
gboolean sound_lock_driver;
static gboolean sound_delayed_quit=FALSE;
static Dataformat playing_format;

#ifdef HAVE_ALSALIB
  #include "sound-alsalib.c"
#endif

#ifdef HAVE_OSS
  #include "sound-oss.c"
#endif

#ifdef HAVE_JACK
  #include "sound-jack.c"
#endif

#ifdef HAVE_SUN
  #include "sound-sun.c"
#endif

#if defined (HAVE_PORTAUDIO)
  #include "sound-portaudio.c"
#endif

#if defined (HAVE_SDL)
  #include "sound-sdl.c"
#endif

#ifdef HAVE_ESOUND
#include "sound-esound.c"
#endif

#include "sound-dummy.c"

struct sound_driver {
     gchar *name, *id;
     
     void (*preferences)(void);

     void (*init)(void);
     void (*quit)(void);

     gint (*output_select_format)(Dataformat *format, gboolean silent);
     gboolean (*output_want_data)(void);
     guint (*output_play)(gchar *buffer, guint bufsize);
     gboolean (*output_stop)(gboolean);
     void (*output_clear_buffers)(void);
     gboolean (*output_suggest_format)(Dataformat *format, Dataformat *result);

     gboolean (*input_supported)(void);
     gint (*input_select_format)(Dataformat *format, gboolean silent);
     void (*input_store)(Ringbuf *buffer);
     void (*input_stop)(void);   
     void (*input_stop_hint)(void);
     int (*input_overrun_count)(void);
};

static struct sound_driver drivers[] = {

#ifdef HAVE_OSS
     { "Open Sound System", "oss", oss_preferences, oss_init, oss_quit, 
       oss_output_select_format,
       oss_output_want_data, oss_output_play, oss_output_stop, 
       oss_output_clear_buffers,NULL,
       oss_input_supported, oss_input_select_format,
       oss_input_store, oss_input_stop },
#endif

#ifdef HAVE_ALSALIB
     { "ALSA", "alsa", alsa_show_preferences, alsa_init, alsa_quit, 
       alsa_output_select_format,
       alsa_output_want_data, alsa_output_play, alsa_output_stop, 
       alsa_output_clear_buffers,NULL,
       alsa_input_supported,  
       alsa_input_select_format, alsa_input_store, alsa_input_stop,
       alsa_input_stop_hint,alsa_input_overrun_count },     
#endif

#ifdef HAVE_JACK
     { "JACK", "jack", mhjack_preferences, mhjack_init, mhjack_quit, 
       mhjack_output_select_format,
       mhjack_output_want_data, mhjack_output_play, mhjack_output_stop, 
       mhjack_clear_buffers, mhjack_output_suggest_format,
       mhjack_input_supported,  
       mhjack_input_select_format, mhjack_input_store, mhjack_input_stop,
       mhjack_input_stop, mhjack_get_xrun_count },
#endif

#ifdef HAVE_SUN
     { "Sun audio", "sun", NULL, sunaud_init, sunaud_quit,
       sunaud_output_select_format, sunaud_output_want_data,
       sunaud_output_play, sunaud_output_stop,
       sunaud_output_clear_buffers,
       sunaud_output_suggest_format,
       sunaud_input_supported,sunaud_input_select_format,sunaud_input_store,
       sunaud_input_stop,NULL,sunaud_input_overrun_count },
#endif

#if defined (HAVE_PORTAUDIO)
     { "PortAudio", "pa", NULL, portaudio_init, portaudio_quit, 
       portaudio_output_select_format,
       portaudio_output_want_data, portaudio_output_play, 
       portaudio_output_stop,
       portaudio_output_clear_buffers, 
       NULL,
       portaudio_input_supported,  
       portaudio_input_select_format, portaudio_input_store, 
       portaudio_input_stop, NULL, portaudio_input_overrun_count }, 
#endif

#if defined (HAVE_SDL)
     { N_("SDL (output only)"), "sdl", NULL, sdl_init, sdl_quit, 
       sdl_output_select_format, 
       sdl_output_want_data, sdl_output_play, sdl_output_stop, 
       sdl_output_clear_buffers, NULL,
       NULL, NULL, NULL, NULL, NULL },
#endif

#ifdef HAVE_ESOUND

     { "ESound", "esound", esound_preferences, esound_init, esound_quit,
       esound_output_select_format, 
       esound_output_want_data, esound_output_play, esound_output_stop,
       esound_output_clear_buffers, NULL, 
       esound_input_supported,  
       esound_input_select_format, esound_input_store, esound_input_stop },

#endif


     { N_("Dummy (no sound)"), "dummy", NULL, dummy_init, dummy_quit,
       dummy_output_select_format, 
       dummy_output_want_data, dummy_output_play, dummy_output_stop,
       dummy_output_clear_buffers, NULL,
       dummy_input_supported,
       dummy_input_select_format, dummy_input_store, dummy_input_stop }

};

static guint current_driver = 0;

gchar *sound_driver_name(void)
{
     return _(drivers[current_driver].name);
}

gchar *sound_driver_id(void)
{
     return drivers[current_driver].id;
}

int sound_driver_index(void)
{
     return current_driver;
}

GList *sound_driver_valid_names(void)
{
    GList *l = NULL;
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        l = g_list_append(l, _(drivers[i].name));
    return l;
}

gchar *sound_driver_id_from_name(gchar *name)
{
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        if (!strcmp(_(drivers[i].name),name))
            return drivers[i].id;
    return NULL;
}

gchar *sound_driver_id_from_index(int index)
{
     return drivers[index].id;
}

gchar *sound_driver_name_from_id(gchar *id)
{
    int i;
    for (i=0; i<ARRAY_LENGTH(drivers); i++)
        if (!strcmp(drivers[i].id,id))
            return _(drivers[i].name);
    return NULL;
}

gboolean sound_driver_has_preferences(gchar *id)
{
     int i;

     if (!id) return (drivers[current_driver].preferences != NULL);

     for (i=0; i<ARRAY_LENGTH(drivers); i++)
	  if (!strcmp(drivers[i].id,id))
	       return (drivers[i].preferences != NULL);
     g_assert_not_reached();
     return FALSE;
}

void sound_driver_show_preferences(gchar *id)
{
     int i;

     if ((!id) && sound_driver_has_preferences(id)) 
	  drivers[current_driver].preferences();

     for (i=0; i<ARRAY_LENGTH(drivers); i++)
	  if (!strcmp(drivers[i].id,id))
	       drivers[i].preferences();
}

void sound_init(void)
{
     gchar *c,*d;
     int i;
     sound_lock_driver = inifile_get_gboolean("soundLock",FALSE);
     output_byteswap_flag = inifile_get_gboolean("outputByteswap",FALSE);
     if (driver_option != NULL)
	  c = driver_option;
     else {
	  c = inifile_get(INI_SETTING_SOUNDDRIVER, DEFAULT_DRIVER);
	  if (!strcmp(c,"default")) c = drivers[0].id;
     }
     for (i=0; i<ARRAY_LENGTH(drivers); i++) {
	  if (!strcmp(drivers[i].id,c)) {
	       current_driver = i;
	       break;
	  }
     }
     if (i == ARRAY_LENGTH(drivers)) {
	  d = g_strdup_printf(_("Invalid driver name: %s\nUsing '%s' driver "
				"instead"),c,drivers[0].name);
	  user_error(d);
	  current_driver = 0;
     }

     drivers[current_driver].init();
}

static void delayed_output_stop(void)
{
     drivers[current_driver].output_stop(FALSE);
     sound_delayed_quit=FALSE;
}

void sound_quit(void)
{
     if (sound_delayed_quit) delayed_output_stop();
     drivers[current_driver].quit();
}

gint output_select_format(Dataformat *format, gboolean silent)
{
     gint i;
     if (sound_delayed_quit) {
	  if (dataformat_equal(format,&playing_format)) {
	       sound_delayed_quit = FALSE;
	       return FALSE;
	  }
	  else delayed_output_stop();
     }     
     i = drivers[current_driver].output_select_format(format,silent);
     if (i == 0) 
	  memcpy(&playing_format,format,sizeof(Dataformat));
     return i;
}

gboolean input_supported(void)
{
     return drivers[current_driver].input_supported ? 
	  drivers[current_driver].input_supported() : 
	  FALSE;
}

gint input_select_format(Dataformat *format, gboolean silent)
{
     if (sound_delayed_quit) delayed_output_stop();
     return drivers[current_driver].input_select_format(format,silent);
}

gboolean output_stop(gboolean must_flush)
{
     if (sound_lock_driver) {
	  if (must_flush)
	       while (output_play(NULL,0) > 0) { }
	  else
	       output_clear_buffers();	  
	  sound_delayed_quit=TRUE;
	  return must_flush;
     } else {
	  return drivers[current_driver].output_stop(must_flush);
     }
}

gboolean output_want_data(void)
{
     return drivers[current_driver].output_want_data();
}

guint output_play(gchar *buffer, guint bufsize)
{     
     if (output_byteswap_flag) {
	  if (output_byteswap_bufsize < bufsize) {
	       g_free(output_byteswap_buffer);
	       output_byteswap_buffer = g_malloc(bufsize);
	       output_byteswap_bufsize = bufsize;
	  }
	  memcpy(output_byteswap_buffer,buffer,bufsize);
	  byteswap(output_byteswap_buffer,playing_format.samplesize,bufsize);
	  buffer = output_byteswap_buffer;
     }
     return drivers[current_driver].output_play(buffer,bufsize);
}

gboolean output_suggest_format(Dataformat *format, Dataformat *result)
{
     if (drivers[current_driver].output_suggest_format)
	  return drivers[current_driver].output_suggest_format(format,result);
     else
	  return FALSE;
}

void input_stop(void)
{
     if (!sound_delayed_quit) 
     	  drivers[current_driver].input_stop();
}

void input_store(Ringbuf *buffer)
{
     drivers[current_driver].input_store(buffer);
}

void output_clear_buffers(void)
{
     drivers[current_driver].output_clear_buffers();
}

void input_stop_hint(void)
{
     if (drivers[current_driver].input_stop_hint)
	  drivers[current_driver].input_stop_hint();
}

int input_overrun_count(void)
{
     if (drivers[current_driver].input_overrun_count)
	  return drivers[current_driver].input_overrun_count();
     else
	  return -1;
}