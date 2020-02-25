/*
 * Copyright (C) 2020
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

#include <stdio.h>
#include <stdlib.h>

#include <sndio.h>
#include "sound.h"
#include "ringbuf.h"

static struct
{
	struct sio_hdl *hdl;
	gchar *device;
	int stopped;
	Ringbuf *output_buffer;
} sndio_data;

static gboolean sndio_init(gboolean silent)
{
	printf("sndio_init\n");
	sndio_data.stopped = 1;
	char *device = getenv("AUDIODEVICE");
	if (device) {
		sndio_data.device = g_strdup (SIO_DEVANY);
	} else {
		sndio_data.device = NULL;
	}
	/*
	sndio_data.output_buffer = ringbuf_new ( 
								inifile_get_guint32(INI_SETTING_SOUNDBUFSIZE,
								INI_SETTING_SOUNDBUFSIZE_DEFAULT) );
	*/
	return TRUE;
}

static void sndio_quit(void)
{
	printf("sndio_quit\n");
	if (sndio_data.hdl) {
		sio_close(sndio_data.hdl);
	}
	if (sndio_data.device) {
		g_free(sndio_data.device);
	}
}

static gint sndio_output_select_format(Dataformat *format, gboolean silent,
					GVoidFunc ready_func)
{
	printf("sndio_output_select_format\n");
	struct sio_par par;
	sio_initpar(&par);

	sndio_data.hdl = sio_open (sndio_data.device, SIO_PLAY, 0); // 1 = non-blocking
	if (!sndio_data.hdl) {
		printf("sndio error: Could not open sndio device\n");
		sio_close(sndio_data.hdl);
		return -1;
	}

	par.sig   = format->sign;
	par.le    = format->bigendian ? 0 : 1;
	par.rate  = format->samplerate;
	par.pchan = format->channels;

	switch (format->samplesize) {
		case 1: par.bits = 8;
		case 2: par.bits = 16;
		case 3: par.bits = 24;
		case 4:
			par.bits = 32;
			if (format->packing == 0 || format->packing == 1) {
				/* Use 32-bit for 24-in-32 with data in MSB */
				par.msb = 1;
			}
	}

	if (!sio_setpar(sndio_data.hdl, &par)) {
		printf("sndio error: Could not set paramaters\n");
		return -1;
	}

	if (!sio_start(sndio_data.hdl)) {
		printf("sndio error: Cannot make device ready\n");
		return -1;
	}

	return 0;
}

static gboolean sndio_output_stop(gboolean must_flush)
{
	printf("sndio_output_stop\n");
/*
	if (must_flush) {
		while (ringbuf_available(sdl_data.output_buffer) > 0) {
			do_yield(TRUE);
		}
	}
*/
	if(sndio_data.hdl) {
		sio_close(sndio_data.hdl);
		sndio_data.hdl = NULL;
	}
	return must_flush; //FALSE
}

static gboolean sndio_output_want_data(void)
{
	printf("sndio_output_want_data\n");
	//return !ringbuf_isfull(sndio_data.output_buffer);
	return TRUE;
}

static guint sndio_output_play(gchar *buffer, guint bufsize)
{
	printf("sndio_output_play\n");
/*
	guint i;
	if (!bufsize) {
		return ringbuf_available(sndio_data.output_buffer);
	}

	printf("output_play: before: %d,%d\n",ringbuf_available(
	sndio_data.output_buffer),ringbuf_freespace(sndio_data.output_buffer));
	i = (guint)ringbuf_enqueue( sndio_data.output_buffer, buffer, bufsize );
	printf("output_play: after: %d,%d\n",
	ringbuf_available(sndio_data.output_buffer),
	ringbuf_freespace(sndio_data.output_buffer));
	sio_write(sndio_data.hdl, sndio_data.output_buffer, i);
	return i;
*/
	size_t io_res, offset, nbytes = bufsize;
	io_res = offset = 0;
	while (nbytes > 0)
	{
		io_res = sio_write(sndio_data.hdl, buffer + offset, nbytes);
		if (io_res == 0) {
			printf("sndio_driver: sio_write() failed\n");
			break;
		}
		offset += io_res;
		nbytes -= io_res;
	}
	//memset(buffer, 0, bufsize);
	return bufsize;
}

static void sndio_output_clear_buffers(void)
{
	printf("sndio_output_clear_buffers\n");
	sio_stop(sndio_data.hdl);
	ringbuf_drain(sndio_data.output_buffer);
}

// REC

static GList *sndio_input_supported_formats(gboolean *complete)
{
     return NULL;
}

static gint sndio_input_select_format(Dataformat *format, gboolean silent,
				      GVoidFunc ready_func)
{
     return 0;
}

static void sndio_input_stop(void)
{
}

static void sndio_input_store(Ringbuf *buffer)
{
}

