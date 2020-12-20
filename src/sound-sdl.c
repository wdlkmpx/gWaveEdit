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

// How to record audio:
//     https://lazyfoo.net/tutorials/SDL/34_audio_recording/index.php
//     https://stackoverflow.com/questions/42990071/recording-microphone-with-sdl2-gets-delayed-by-2-seconds

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#ifdef HAVE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif

#include "sound.h" 
#include "ringbuf.h"
#include "gettext.h"

// ==============================================================

#if !defined(HAVE_SDL2)
#define SDL_AudioDeviceID            int
#define SDL_CloseAudioDevice(id)     SDL_CloseAudio()
#define SDL_GetAudioDeviceStatus(id) SDL_GetAudioStatus()
#define SDL_PauseAudioDevice(id, x)  SDL_PauseAudio(x)
#define SDL_LockAudioDevice(id)      SDL_LockAudio()
#define SDL_UnlockAudioDevice(id)    SDL_UnlockAudio()
#endif

static struct {
   Ringbuf *output_buffer;
   SDL_AudioDeviceID devId;
} sdl_data;

// ==============================================================

static gboolean sdl_init(gboolean silent)
{
   //printf ("sdl_init\n");
   gchar *c;
   if (SDL_Init(SDL_INIT_AUDIO) == -1) {
      c = g_strdup_printf (_("Could not initialize SDL: %s"), 
                          SDL_GetError());
      console_message(c);
      g_free(c);
      exit(1);
   }
   sdl_data.output_buffer = ringbuf_new ( 
                                 inifile_get_guint32 (INI_SETTING_SOUNDBUFSIZE,
                                 INI_SETTING_SOUNDBUFSIZE_DEFAULT) );
   return TRUE;
}


static void sdl_quit(void)
{
   //printf ("sdl_quit\n");
   SDL_Quit();
}


static void sdl_output_callback(void *userdata, Uint8 *stream, int len)
{
   //printf ("sdl_output_callback\n");
   guint32 ui;
   ui = ringbuf_dequeue ( sdl_data.output_buffer, stream, len );
   if (ui < (guint32) len) {
      memset(stream+ui, 0, len-ui);
   }
}


static gint sdl_output_select_format(Dataformat *format, gboolean silent,
				     GVoidFunc ready_func)
{
   //printf ("sdl_output_select_format\n");
   gchar *c;
   SDL_AudioSpec desired;

#ifdef HAVE_SDL2
   SDL_AudioSpec obtained;
   SDL_memset(&desired, 0, sizeof(desired)); /* or SDL_zero(desired) */
#endif

   if (format->type == DATAFORMAT_FLOAT || format->samplesize > 2 || format->channels > 2) {
      return -1;
   }
   desired.freq = format->samplerate;
   if (format->sign) {
      if (format->samplesize == 1) desired.format = AUDIO_S8;
      else if (format->bigendian)  desired.format = AUDIO_S16MSB;
      else                         desired.format = AUDIO_S16LSB;
   } else {
      if (format->samplesize == 1) desired.format = AUDIO_U8;
      else if (format->bigendian)  desired.format = AUDIO_U16MSB;
      else                         desired.format = AUDIO_U16LSB;
   }
   desired.channels = format->channels;
   desired.samples = 512;
   desired.callback = sdl_output_callback;

#ifdef HAVE_SDL2
   sdl_data.devId = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
   if (!sdl_data.devId) {
#else
   if (SDL_OpenAudio(&desired,NULL) < 0) {
#endif
      c = g_strdup_printf (_("SDL: Couldn't open audio: %s"), SDL_GetError());
      console_message(c);
      g_free(c);
      return -1;
   }

#ifdef HAVE_SDL2
   if (desired.format != obtained.format) {
      printf ("SDL2: Not really using the desired.format\n");
   }
#endif

   return 0;
}


static gboolean sdl_output_stop(gboolean must_flush)
{
   //printf ("sdl_output_stop\n");
   if (must_flush) {
      while (ringbuf_available(sdl_data.output_buffer) > 0) {
         do_yield(TRUE);
      }
   }

   if (SDL_GetAudioDeviceStatus (sdl_data.devId) != SDL_AUDIO_STOPPED) {
      SDL_CloseAudioDevice (sdl_data.devId);
   }

   ringbuf_drain(sdl_data.output_buffer);
   return must_flush;
}


static void sdl_output_clear_buffers(void)
{
   //printf ("sdl_output_clear_buffers\n");
   SDL_PauseAudioDevice (sdl_data.devId, 1); // stop playing
   ringbuf_drain(sdl_data.output_buffer);
}

static guint sdl_output_play(gchar *buffer, guint bufsize)
{
   //printf ("sdl_output_play\n");
   guint i;
   if (!bufsize) {
      return ringbuf_available(sdl_data.output_buffer);
   }

   SDL_LockAudioDevice (sdl_data.devId);

   /* printf ("output_play: before: %d,%d\n",ringbuf_available(
   sdl_data.output_buffer),ringbuf_freespace(sdl_data.output_buffer)); */
   i = (guint)ringbuf_enqueue( sdl_data.output_buffer, buffer, bufsize );
   /* printf ("output_play: after: %d,%d\n",
   ringbuf_available(sdl_data.output_buffer),
   ringbuf_freespace(sdl_data.output_buffer)); */

   SDL_UnlockAudioDevice (sdl_data.devId);

   if (SDL_GetAudioDeviceStatus (sdl_data.devId) == SDL_AUDIO_PAUSED) {
      SDL_PauseAudioDevice (sdl_data.devId, 0); // start playing
   }

   return i;
}


static gboolean sdl_output_want_data(void)
{
   //printf ("sdl_output_want_data\n");
   return !ringbuf_isfull(sdl_data.output_buffer);
}

