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

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "um.h"
#include "inifile.h"
#include "rawdialog.h"
#include "pipedialog.h"
#include "main.h"
#include "filetypes.h"
#include "tempfile.h"
#include "gettext.h"

#define MAX_REAL_SIZE inifile_get_guint32(INI_SETTING_REALMAX,INI_SETTING_REALMAX_DEFAULT)

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IEEE_FLOAT 3

struct file_type;
struct file_type {
     gchar *name;
     gchar *extension;
     gboolean (*typecheck)(gchar *filename);
     Chunk *(*load)(gchar *filename, int dither_mode, StatusBar *bar);
     gboolean (*save)(Chunk *chunk, gchar *filename, 
		      struct file_type *type, int dither_mode, StatusBar *bar,
		      gboolean *fatal);
     gint extra_data;
};

static GList *file_types = NULL;

static gboolean wav_check(gchar *filename);
static Chunk *wav_load(gchar *filename, int dither_mode, StatusBar *bar);
static gboolean wav_save(Chunk *chunk, gchar *filename, 
			 struct file_type *type, int dither_mode, 
			 StatusBar *bar,gboolean *fatal);

#ifdef HAVE_LIBSNDFILE
static gboolean sndfile_check(gchar *filename);
static Chunk *sndfile_load(gchar *filename, int dither_mode, StatusBar *bar);
static gboolean sndfile_save(Chunk *chunk, gchar *filename, 
			     struct file_type *type, int dither_mode, 
			     StatusBar *bar, gboolean *fatal);
static gboolean sndfile_save_main(Chunk *chunk, gchar *filename, 
				  int format, int dither_mode, StatusBar *bar,
				  gboolean *fatal);
#endif

static Chunk *raw_load(gchar *filename, int dither_mode, StatusBar *bar);
static gboolean raw_save(Chunk *chunk, gchar *filename, 
			 struct file_type *type, int dither_mode,
			 StatusBar *bar, gboolean *fatal);

static Chunk *ogg_load(gchar *filename, int dither_mode, StatusBar *bar);
static gboolean ogg_save(Chunk *chunk, gchar *filename, struct file_type *type,
			 int dither_mode, StatusBar *bar, gboolean *fatal);


static Chunk *mp3_load(gchar *filename, int dither_mode, StatusBar *bar);
static gboolean mp3_save(Chunk *chunk, gchar *filename, struct file_type *type,
			 int dither_mode, StatusBar *bar, gboolean *fatal);

static Chunk *try_mplayer(gchar *filename, int dither_mode, StatusBar *bar);

static gboolean xputenv(char *string)
{
     if (putenv(string)) {
	  user_error(_("putenv failed!"));
	  return TRUE;
     }
     return FALSE;
}

static gboolean xunsetenv(char *varname)
{
     if (unsetenv(varname) != 0) {
	  console_message(_("unsetenv failed!"));
	  return TRUE;
     }
     return FALSE;
}

static void register_file_type(gchar *name, gchar *ext, 
			       gboolean (*typecheck)(gchar *filename),
			       Chunk *(*load)(gchar *filename, 
					      int dither_mode, StatusBar *bar),
			       gboolean (*save)(Chunk *chunk, gchar *filename, 
						struct file_type *type,
						int dither_mode,
						StatusBar *bar,
						gboolean *fatal),
			       int extra_data)
{
     struct file_type *t;
     t = g_malloc(sizeof(*t));
     t->name = g_strdup(name);
     t->extension = g_strdup(ext);
     t->typecheck = typecheck;
     t->load = load;
     t->save = save;
     t->extra_data = extra_data;
     file_types = g_list_append(file_types,t);
}

static void setup_types(void)
{
#if defined(HAVE_LIBSNDFILE)
     SF_FORMAT_INFO info;
     int major_count,i;
     gchar buf[32];
#endif
     if (file_types) return;
     register_file_type(_("Microsoft WAV format"), ".wav", wav_check, 
			wav_load, wav_save, 0);
#if defined(HAVE_LIBSNDFILE)
     sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(int));
     for (i=0; i<major_count; i++) {
	  info.format = i;
	  if (sf_command(NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof(info))) {
	       console_message(sf_strerror(NULL));
	       continue;
	  }
	  if ((info.format&SF_FORMAT_TYPEMASK) == SF_FORMAT_RAW ||
	      (info.format&SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV) continue;
	  snprintf(buf,sizeof(buf),".%s",info.extension);
	  register_file_type((gchar *)info.name, buf, sndfile_check, 
			     sndfile_load, sndfile_save, 
			     info.format&SF_FORMAT_TYPEMASK);
     }
#endif
     if (program_exists("oggenc") || program_exists("oggdec"))
	  register_file_type(_("Ogg Vorbis"),".ogg", NULL, ogg_load, 
			     ogg_save, 0);
     if (program_exists("lame"))
	    register_file_type("MP3",".mp3",NULL,mp3_load,mp3_save,0);
     register_file_type(_("Raw PCM data"), ".raw", NULL, raw_load, 
			raw_save, 0);
}

guint fileformat_count(void)
{
     setup_types();
     return g_list_length(file_types);
}

gchar *fileformat_name(guint fileformat)
{
     struct file_type *t;
     t = g_list_nth_data(file_types, fileformat);
     if (t) return t->name;
     return NULL;
}

gchar *fileformat_extension(guint fileformat)
{
     struct file_type *t;
     t = g_list_nth_data(file_types, fileformat);
     if (t) return t->extension;
     return NULL;
}

static Chunk *chunk_load_main(gchar *filename, int dither_mode, StatusBar *bar)
{
     GList *l;
     struct file_type *ft;
     gchar *c;
     Chunk *chunk;

     setup_types();

     /* Search through the extensions */
     for (l=file_types; l!=NULL; l=l->next) {
	  ft = (struct file_type *)l->data;
	  if (!ft->extension) continue;
	  c = strchr(filename,0) - strlen(ft->extension);
	  if (c<filename || strcmp(c,ft->extension)!=0) continue;
	  if (ft->typecheck==NULL || ft->typecheck(filename)) 
	       return ft->load(filename,dither_mode,bar);
     }
     /* Use the file checking functions */
     for (l=file_types; l!=NULL; l=l->next) {
	  ft = (struct file_type *)l->data;
	  if (ft->typecheck == NULL) continue;
	  if (ft->typecheck(filename))
	       return ft->load(filename,dither_mode,bar);
     }
     /* Try mplayer if available */
     chunk = try_mplayer(filename,dither_mode,bar);
     if (chunk != NULL) return chunk;
     /* Use the raw loader */
     return raw_load(filename,dither_mode,bar);
}

Chunk *chunk_load(gchar *filename, int dither_mode, StatusBar *bar )
{
     Chunk *chunk;
     gchar *c;
     EFILE *f;

     /* First, see if the file exists */
     if (!file_exists(filename)) {
	  c = g_strdup_printf(_("The file %s does not exist!"),filename);
	  user_error(c);
	  g_free(c);
	  return NULL;
     }

     if (!file_is_normal(filename)) {
	  c = g_strdup_printf(_("The file %s is not a regular file!"),filename);
	  user_error(c);
	  g_free(c);
	  return NULL;	  
     }

     /* Try to open it here so we don't get a lot of 'permission
      * denied' and similar errors later */
     f = e_fopen(filename,EFILE_READ);
     if (f == NULL) return NULL;
     e_fclose(f);

     status_bar_begin_progress(bar,1,_("Loading"));
     chunk = chunk_load_main(filename,dither_mode,bar);

     return chunk;
}


static struct file_type *choose_format(gchar *filename)
{
     struct file_type *ft;
     GList *l;
     gchar *c, **z, **z2;
     guint i;
     gint x;
     setup_types();
     /* Search through the extensions */
     for (l=file_types; l!=NULL; l=l->next) {
	  ft = (struct file_type *)l->data;
	  if (!ft->extension) continue;
	  c = strchr(filename,0) - strlen(ft->extension);
	  if (c<filename || strcmp(c,ft->extension)!=0) continue;
	  if (ft->save != NULL)
	       return ft;
     }
     /* Ask for file type */
     z = g_malloc((g_list_length(file_types)+1) * sizeof(char *));
     for (l=file_types,z2=z; l!=NULL; l=l->next,z2++) {
	  ft = (struct file_type *)l->data;
	  *z2 = ft->name;
     }
     *z2 = NULL;
     c = g_strdup_printf(_("The file name '%s' has an extension unknown to the "
			 "program. Please specify in which format this file "
			 "should be saved."),filename);
     i = (guint)inifile_get_guint32("lastSaveFileType",0);
     if (i>=g_list_length(file_types)) i=0;
     x = user_choice(z, (guint)i, _("Unknown file type"), c, TRUE);
     g_free(z);
     g_free(c);
     if (x==-1) return NULL;
     inifile_set_guint32("lastSaveFileType",(guint32)x);
     ft = g_list_nth_data( file_types, x );
     return ft;
}

gboolean chunk_save(Chunk *chunk, gchar *filename, int dither_mode, 
		    StatusBar *bar)
{
     gchar *d;
     gboolean b=FALSE,res;
     EFILE *f;
     struct file_type *ft;
     
     /* Let the user choose a format first, so that if the user presses cancel,
      * we haven't done any of the backup/unlink stuff.. */	
     ft = choose_format(filename);
     if (ft == NULL) return TRUE;

     if (file_exists(filename)) {

          /* Check that the file is writeable by us. */
          f = e_fopen(filename,EFILE_APPEND);
          if (f == NULL) return TRUE;
          e_fclose(f);

          /* we may need to move or copy the file into a tempfile... */
          if (datasource_backup_unlink(filename)) return TRUE;
     }

     status_bar_begin_progress(bar,chunk->size,_("Saving"));
     res = ft->save(chunk,filename,ft,dither_mode,bar,&b);

     if (res && b && !status_bar_progress(bar,0)) {
	  d = g_strdup_printf(_("The file %s may be destroyed since the"
			      " saving failed. Try to free up some disk space "
			      "and save again. If you exit now the "
			      "file's contents could be in a bad state. "),
			      filename);
	  user_warning(d);
	  g_free(d);
     }

     return res;
}

gboolean chunk_save_ff(Chunk *chunk, gchar *filename,
		       guint fileformat, int dither_mode, StatusBar *bar)
{
     struct file_type *ft;
     gboolean b;
     setup_types();
     ft = g_list_nth_data( file_types, fileformat );
     g_assert (ft != NULL);
     return ft->save(chunk, filename, ft, dither_mode, bar, &b);
}


/* WAV */


static gboolean wav_check(gchar *filename)
{
     EFILE *f;
     char buf[12];
     f = e_fopen ( filename, EFILE_READ );
     if (!f) return FALSE;
     if (e_fread(buf,12,f)) { e_fclose(f); return FALSE; }
     e_fclose(f); 
     if ((memcmp(buf,"RIFF",4) && memcmp(buf,"RIFX",4)) || memcmp(buf+8,"WAVE",4)) return FALSE; 
     return TRUE;     
}

static off_t find_wav_chunk(EFILE *f, char *chunkname, gboolean be)
{
     char buf[4];
     unsigned long l;
     while (1) {
	  if (e_fread(buf,4,f) ||
	      e_fread_xe(&l,4,f,be)) return -1;
	  if (!memcmp(buf,chunkname,4)) {
#if SIZEOF_OFF_T > 4
	       return (off_t) l;
#else
	       return (l>=0x80000000) ? 0x7FFFFFFFL : l;
#endif
	  }
	  if (e_fseek(f,(off_t)l,SEEK_CUR)) return -1;
     }     
}

static Chunk *wav_load(char *filename, int dither_mode, StatusBar *bar)
{
     EFILE *f;
     char *c;
     char buf[12];
     long int m;
     unsigned short int s,channels,bits;
     unsigned long int samplerate;
     off_t l,e;
     gboolean rifx = FALSE;
     Datasource *ds;
     Dataformat fmt;
     f = e_fopen ( filename, EFILE_READ );
     if (!f) return NULL;
     if (e_fread(buf,12,f)) { e_fclose(f); return NULL; }
     if ((memcmp(buf,"RIFF",4) && memcmp(buf,"RIFX",4)) || memcmp(buf+8,"WAVE",4)) {
	  c = g_strdup_printf (_("%s is not a valid wav file"),filename);
	  user_error(c);
	  g_free(c);
	  e_fclose(f);
	  return NULL;
     }
     if (!memcmp(buf,"RIFX",4)) rifx=TRUE;
     l = find_wav_chunk(f,"fmt ",rifx);
     if ((l == -1) || e_fread_xe(&s,2,f,rifx)) { e_fclose(f); return NULL; }
     if ( l<16 || (s != 0x0001 && s != 0x0003) ) {
	  e_fclose(f);
#if defined(HAVE_LIBSNDFILE)
	  /* Fall back on Libsndfile for reading non-PCM wav files. */
	  return sndfile_load(filename,dither_mode,bar);
#else
	  c = g_strdup_printf (_("The file %s is a compressed wav file. This "
			       "program can only work with uncompressed wav "
			       "files."),filename);
	  user_error(c);
	  free(c);
	  return NULL;
#endif
     }
     if (e_fread_xe(&channels,2,f,rifx) ||
	 e_fread_xe(&samplerate,4,f,rifx) ||
	 e_fseek(f,6,SEEK_CUR) ||
	 e_fread_xe(&bits,2,f,rifx) ||
	 (l>16 && e_fseek(f,l-16,SEEK_CUR))) { 
	  e_fclose(f); 
	  return NULL; 
     }
     l = find_wav_chunk(f,"data",rifx);
     m = e_ftell(f);
     if (l == -1 || m == -1) {
	  e_fclose(f);
	  return NULL;
     }

     /* Find length of file and close it*/
     if (e_fseek(f,0,SEEK_END)) { e_fclose(f); return NULL; }
     e = e_ftell(f);
     e_fclose(f);    
     if (e == -1) { return NULL; }

     /* Is the file smaller than expected? */
     if (e-m<l) l=e-m; /* Borde det vara l=e-m+1?? */

#if SIZEOF_OFF_T > 4
     /* Special code to handle very large files. If the specified data
      * chunk length makes the expected file length within 32 bytes of
      * the 4GB limit, assume the data chunk continues until EOF */ 
     if (m+l >= (off_t)0xFFFFFFE0) l=e-m;
#endif

     fmt.samplerate = samplerate;
     fmt.channels = channels;
     if (s == 1)
	  fmt.type = DATAFORMAT_PCM;
     else
	  fmt.type = DATAFORMAT_FLOAT;
     fmt.samplesize = (bits+7) >> 3;
     fmt.samplebytes = fmt.samplesize * fmt.channels;
     fmt.sign = (bits>8);
     fmt.bigendian = rifx;

     ds = gtk_type_new(datasource_get_type());

     memcpy(&(ds->format),&fmt,sizeof(fmt));
     ds->type = DATASOURCE_VIRTUAL;
     ds->bytes = l;
     ds->length = l / (off_t)fmt.samplebytes;
     ds->data.virtual.filename = g_strdup(filename);
     ds->data.virtual.offset = m;
     /* printf("wav_load: samplebytes: %d, length: %ld\n",(int)fmt.samplebytes,
	    (long int)ds->length);
	    printf("wav_load: bigendian: %d\n",fmt.bigendian); */

     if (ds->bytes <= MAX_REAL_SIZE)
	  datasource_realize(ds,dither_mode);

     return chunk_new_from_datasource(ds);
}

static gboolean wav_save(Chunk *chunk, char *filename, 
			 struct file_type *type, int dither_mode, 
			 StatusBar *bar, gboolean *fatal)
{
     EFILE *f;
     Datasource *ds;
     DataPart *dp;
     Dataformat *fmt;
     fmt = &(chunk->format);

     /* Check if the format is OK */     

#ifndef HAVE_LIBSNDFILE
     if (chunk->format.type == DATAFORMAT_FLOAT &&
	 !ieee_le_compatible) {
	  user_error(_("On this system, libsndfile is required to save "
		     "floating-point wav files."));
	  return TRUE;
     }
#endif

     if (fmt->type == DATAFORMAT_PCM && fmt->samplesize==1 && fmt->sign) {
	  user_error(_("8-bit wav-files must be in unsigned format!"));
	  return TRUE;
     }
     if (fmt->type == DATAFORMAT_PCM && fmt->samplesize>1 && !fmt->sign) {
	  user_error(_("16/24/32-bit wav-files must be in signed format!"));
	  return TRUE;
     }    
     if (fmt->type == DATAFORMAT_PCM && fmt->bigendian == TRUE) {
	  user_error(_("wav files must be in little endian format!"));
	  return TRUE;
     }

     /* Give a warning once if we're saving a file larger than 2GB */
     if (chunk->size > (off_t)0x7FFFFFD3 && 
	 !inifile_get_gboolean("warnLargeWav",FALSE)) {
	  user_warning(_("You are saving a wav file larger than 2048MB."
		       " Such large files are non-standard and may not be "
		       "readable by all programs.\n\n"
		       " (this warning will not be displayed again)"));
	  inifile_set_gboolean("warnLargeWav",TRUE);		       
     }

     /* If we're saving an entire TEMPFILE and it's in WAV format, we can just 
      * move it (this speeds up things like saving directly after 
      * recording. */
     dp = (DataPart *)chunk->parts->data;
     ds = dp->ds;
     if (chunk->parts->next == NULL &&
	 dp->position == 0 && dp->length == ds->length && 
	 (ds->type == DATASOURCE_TEMPFILE) &&
	 (ds->data.virtual.offset == 44 || 
	  (ieee_le_compatible && ds->data.virtual.offset == 58)) && 
	 wav_check(ds->data.virtual.filename)) {
	  if (xunlink(filename)) return TRUE;
	  *fatal = TRUE;
	  if (xrename(ds->data.virtual.filename,filename,TRUE)) return TRUE;
	  ds->type = DATASOURCE_VIRTUAL;
	  g_free(ds->data.virtual.filename);
	  ds->data.virtual.filename = g_strdup(filename);
	  return FALSE;
     } else {
          /* Regular saving */


     /* By default, delegate the FP chunk saving to libsndfile. */
#ifdef HAVE_LIBSNDFILE
     if (chunk->format.type == DATAFORMAT_FLOAT)
	  return sndfile_save_main(chunk,filename,SF_FORMAT_WAV,dither_mode,
				   bar,fatal);
#endif

	  f = e_fopen(filename,EFILE_WRITE);
	  if (!f) return TRUE;
	  *fatal = TRUE;
	  
	  if (write_wav_header(f,fmt,chunk->size)) {
	       e_fclose(f);
	       return TRUE;
	  }
	  if (chunk_dump(chunk,f,FALSE,dither_mode,bar)) {
	       e_fclose_remove(f);
	       return TRUE;
	  }
	  e_fclose ( f );
     }    
     return FALSE;
}

/* RAW */

static Chunk *raw_load(gchar *filename, int dither_mode, StatusBar *bar)
{
     Datasource *ds;
     Dataformat *fmt;
     off_t i;
     fmt = rawdialog_execute(filename);
     if (!fmt) return NULL;
     i = errdlg_filesize(filename);
     if (i==-1) return NULL;
     ds = (Datasource *)gtk_type_new(datasource_get_type());
     memcpy(&(ds->format),fmt,sizeof(Dataformat));
     ds->bytes = i;
     ds->length = i/fmt->samplebytes;
     ds->type = DATASOURCE_VIRTUAL;
     ds->data.virtual.filename = g_strdup(filename);
     ds->data.virtual.offset = 0;
     return chunk_new_from_datasource(ds);
}

static gboolean raw_save(Chunk *chunk, gchar *filename, 
			 struct file_type *type, int dither_mode, 
			 StatusBar *bar, gboolean *fatal)
{
     EFILE *f;
     f = e_fopen(filename,EFILE_WRITE);
     if (!f) return TRUE;
     if (chunk_dump(chunk,f,IS_BIGENDIAN,dither_mode,bar)) {
          e_fclose(f);
	  *fatal = TRUE;
          return TRUE;
     }
     e_fclose(f);
     return FALSE;
}

/* SNDFILE */

#if defined(HAVE_LIBSNDFILE)

static gboolean sndfile_check(gchar *filename)
{
     SF_INFO info = {};
     SNDFILE *s;
     s = sf_open(filename, SFM_READ, &info);
     if (s) sf_close(s);
     return (s!=NULL && ((info.format&SF_FORMAT_SUBMASK) != SF_FORMAT_RAW));
}

static Chunk *sndfile_load(gchar *filename, int dither_mode, StatusBar *bar)
{
     SF_INFO info = {};
     SNDFILE *s;
     Datasource *ds;
     Dataformat f;
     gchar *ch;
     int rawbuf_size;
     void *rawbuf;
     guint st;
     gboolean raw_readable=FALSE;
     TempFile tmp;
     s = sf_open(filename, SFM_READ, &info);
     if (!s) {
          ch = g_strdup_printf(_("Failed to open '%s'!"),filename);
          user_error(ch);
          g_free(ch);
          return NULL;
     }
     /* printf("info.seekable = %d, info.samples = %d\n",info.seekable,info.samples); */
     f.type = DATAFORMAT_PCM;
     f.samplerate = info.samplerate;
     f.channels = info.channels;
     /* Fix samplesize parameter */
     switch (info.format&SF_FORMAT_SUBMASK) {
     case SF_FORMAT_PCM_U8: f.sign=FALSE;f.samplesize=1;raw_readable=TRUE;break;
     case SF_FORMAT_PCM_S8: f.sign=TRUE; f.samplesize=1;raw_readable=TRUE;break;
     case SF_FORMAT_PCM_16: f.sign=TRUE; f.samplesize=2;break;
	  /*     case SF_FORMAT_PCM_24: f.sign=TRUE; f.samplesize=3;raw_readable=FALSE;break;
		 case SF_FORMAT_PCM_32: f.sign=TRUE; f.samplesize=4;raw_readable=FALSE;break; */
	  /* Default to floating point */	  
     default: f.type = DATAFORMAT_FLOAT; f.samplesize=sizeof(sample_t); break;
     }
     f.samplebytes = f.channels * f.samplesize;
     ds = gtk_type_new(datasource_get_type());
     ds->type = DATASOURCE_SNDFILE;
     memcpy(&(ds->format),&f,sizeof(f));
     ds->length = (off_t) info.frames;
     ds->bytes = (off_t) (ds->length * f.samplebytes);
     ds->opencount = 1;
     ds->data.sndfile.filename = g_strdup(filename);
     ds->data.sndfile.raw_readable = raw_readable;	  
     ds->data.sndfile.handle = s;
     ds->data.sndfile.pos = 0;
     /* Handle seekable chunks */
     if (info.seekable) {
	  datasource_close(ds);
	  return chunk_new_from_datasource(ds);
     }
     /* Chunk is not seekable, we must dump it into a tempfile. */
     /* puts("Non-native format!!"); */
     tmp = tempfile_init(&f,FALSE);
     status_bar_begin_progress(bar,info.frames,NULL);
     rawbuf_size = f.samplesize * f.channels * 1024;
     rawbuf = g_malloc(rawbuf_size);

     while (ds->data.sndfile.pos < ds->length) {

	  st = datasource_read_array(ds,ds->data.sndfile.pos,rawbuf_size,
				     rawbuf, dither_mode);
	  
	  if (st == 0 || tempfile_write(tmp,rawbuf,st) || 
	      status_bar_progress(bar, st/f.samplebytes)) {
	       datasource_close(ds);
	       gtk_object_sink(GTK_OBJECT(ds));
	       tempfile_abort(tmp);
	       g_free(rawbuf);
	       return NULL;
	  }	  
     }
     g_free(rawbuf);
     datasource_close(ds);
     gtk_object_sink(GTK_OBJECT(ds));
     return tempfile_finished(tmp);
}

static gboolean find_nearest_sndfile_format(Dataformat *fmt, int format, 
					    SF_INFO *info, gchar *filename)
{
  int suggestions[] = { SF_FORMAT_PCM_U8, SF_FORMAT_PCM_S8, SF_FORMAT_PCM_16,
			   SF_FORMAT_PCM_24, SF_FORMAT_PCM_32, SF_FORMAT_FLOAT,
                           SF_FORMAT_DOUBLE };
     int i,j,k;
     SF_INFO sfi;
     info->samplerate = fmt->samplerate;
     info->channels = fmt->channels;
     /* Try suggestions first... */
     if (fmt->type == DATAFORMAT_PCM) {
	  i = fmt->samplesize;
	  if (i == 1) i--;
     } else if (fmt->samplesize == sizeof(float)) 
	  i = 5;
     else
	  i = 6;
     for (; i<ARRAY_LENGTH(suggestions); i++) {
	  info->format = format | suggestions[i];
	  if (sf_format_check(info)) return FALSE;
     }
     /* Now try the formats that were not in the suggestion array... */
     sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &i, sizeof(int));
     for (j=0; j<i; j++) {
	  sfi.format = j;
	  sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &sfi, sizeof(sfi));
	  for (k=0; k<ARRAY_LENGTH(suggestions); k++)
	       if ((sfi.format & SF_FORMAT_SUBMASK) == suggestions[k])
		    break;
	  if (k<ARRAY_LENGTH(suggestions)) continue;
	  info->format = (sfi.format&SF_FORMAT_SUBMASK) | format;
	  if (sf_format_check(info)) return FALSE;
     }
     /* Try the left out suggestions finally... */
     i = fmt->samplesize;
     if (i > 1) {
	  i--;
	  for (; i>=0; i--) {
	       info->format = format | suggestions[i];
	       if (sf_format_check(info)) return FALSE;
	  }
     }

     user_error(_("Invalid sample format or number of channels for this file"
		" format"));
     return TRUE;
     
}


static gboolean sndfile_save_main(Chunk *chunk, gchar *filename, 
				  int format, int dither_mode, StatusBar *bar,
				  gboolean *fatal)
{
     gchar *c;
     off_t i;
     guint n;
     SNDFILE *s;
     SF_INFO info;
     sample_t *samplebuf;
     ChunkHandle *ch;

     if (find_nearest_sndfile_format(&(chunk->format),format,&info,filename)) 
	  return TRUE;
     
     s = sf_open(filename, SFM_WRITE, &info);
     if (!s) {
          c = g_strdup_printf(_("Failed to open '%s'!"),filename);
          user_error(c);
          g_free(c);
          return TRUE;
     }

     sf_command ( s, SFC_SET_NORM_FLOAT, NULL, SF_TRUE );
     sf_command ( s, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE );

     ch = chunk_open(chunk);
     if (ch == NULL) {
	  sf_close(s);
	  return TRUE;
     }

     /* We could speed this up a LOT esp. on output to PCM files. */
     samplebuf = g_malloc(sizeof(sample_t) * chunk->format.channels * 1024);

     for (i=0; i<chunk->length; i+=n) {
	  /* FIXME: Use chunk_read_array for FP chunks */
	  n = chunk_read_array_fp(ch,i,1024,samplebuf,dither_mode);
	  if (!n) {
	       chunk_close(ch);
	       sf_close(s);
	       g_free(samplebuf);
	       *fatal = TRUE;
	       return TRUE;
	  }
	  if (sf_writef_sample_t(s,samplebuf,n) != n) {
	       c = g_strdup_printf(_("Failed to write to '%s'!"),filename);
	       user_error(c);
	       g_free(c);
	       chunk_close(ch);
	       sf_close(s);
	       g_free(samplebuf);
	       *fatal = TRUE;
	       return TRUE;  
	  }
	  if (status_bar_progress(bar, n*chunk->format.samplebytes)) {
	       chunk_close(ch);
	       sf_close(s);
	       g_free(samplebuf);
	       xunlink(filename);
	       return TRUE;
	  }
     }
     chunk_close(ch);
     sf_close(s);
     g_free(samplebuf);

     return FALSE;

}

static gboolean sndfile_save(Chunk *chunk, gchar *filename, 
			     struct file_type *type, int dither_mode, 
			     StatusBar *bar, gboolean *fatal)
{
     return sndfile_save_main(chunk,filename,type->extra_data,dither_mode,
			      bar,fatal);
}

#endif

/* OGG and MP3 */

static Chunk *run_decoder(gchar *filename, gchar *tempname, gchar *progname, 
			  gchar **argv, int dither_mode, StatusBar *bar)
{
     gchar *c;
     Chunk *r=NULL;
     Datasource *ds;
     pid_t p,q;
     gboolean b;
     off_t o;
     /* Using oggdec in raw mode to decode to stdout, it seems there is no
      * way to determine the sample rate of the output, so we decode it to a
      * temporary wave file instead */
     p = fork();
     if (p == -1) {
	  c = g_strdup_printf("Error: fork: %s",strerror(errno));
	  user_error(c);
	  g_free(c);
	  return NULL;
     } else if (p == 0) {
	  close_all_files();
	  execvp(progname,argv);
	  fprintf(stderr,"execvp: %s: %s\n",progname,strerror(errno));
	  _exit(1);
     }
     b = um_use_gtk;
     um_use_gtk = FALSE;
     /* Estimate the output wav's size as 20 times the original's size or 64MB */
     o = errdlg_filesize(filename);
     if (o<0) o=64*1024*1024;
     else o *= 20;
     status_bar_begin_progress(bar,o,_("Decoding"));
     while (1) {
	  mainloop(TRUE);
	  /* See if the child has exited */
	  q = waitpid(p,NULL,WNOHANG);
	  if (q == p) break;
	  if (q == -1) {
	       console_perror("waitpid");
	       break;
	  }
	  /* See how large the file is */
	  if (!file_exists(tempname)) continue;
	  o = errdlg_filesize(tempname);	  
	  if (o > 0 && status_bar_progress(bar,o-bar->progress_cur)) {
	       kill(p,SIGKILL);
	       waitpid(p,NULL,0);
	       xunlink(tempname);
	       break;
	  }
     }
     um_use_gtk = b;
     if (file_exists(tempname)) {
	  r=wav_load(tempname,dither_mode,bar);
	  if (r != NULL) {
	       ds = ((DataPart *)(r->parts->data))->ds;
	       if (ds->type == DATASOURCE_VIRTUAL)
	            ds->type = DATASOURCE_TEMPFILE;
	  }
     }
     return r;
}

static Chunk *ogg_load(gchar *filename, int dither_mode, StatusBar *bar)
{
     gchar *tempname = get_temp_filename(0);
     gchar *argv[] = { "oggdec", "--quiet", "-o", tempname, filename, NULL };
     Chunk *c;
     c = run_decoder(filename,tempname,"oggdec",argv,dither_mode,bar);
     g_free(tempname);
     return c;
}

static gboolean ogg_save(Chunk *chunk, gchar *filename, struct file_type *type,
			 int dither_mode, StatusBar *bar, gboolean *fatal)
{
     Chunk *x=NULL,*y;
     Dataformat fmt;
     gchar *c,*d;
     gboolean b;
     xunlink(filename);
     d = g_strdup_printf("OUTFILE=%s",filename);     
     if (xputenv(d)) { g_free(d); return TRUE; }
     if (chunk->format.type == DATAFORMAT_FLOAT) {
	  memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
	  fmt.type = DATAFORMAT_PCM;
	  fmt.samplesize = 4;
	  fmt.samplebytes = fmt.samplesize * fmt.channels;
	  x = chunk_convert_sampletype(chunk,&fmt);
	  g_assert(x != NULL);
     }
     y = (x==NULL) ? chunk : x;
     c = g_strdup_printf("oggenc --raw -B %d -C %d -R %d --raw-endianness %d "
			 "--quiet -o \"$OUTFILE\" -", 
			 y->format.samplesize*8, 
			 y->format.channels, y->format.samplerate,
			 y->format.bigendian?1:0);
     b = pipe_dialog_send_chunk(y,c,FALSE,dither_mode,bar);
     g_free(c);     
     if (x != NULL) gtk_object_sink(GTK_OBJECT(x));
     if (!xunsetenv("OUTFILE")) g_free(d);
     if (b || !file_exists(filename)) {
	  *fatal = TRUE;
	  return TRUE;
     }
     return FALSE;
}



static Chunk *mp3_load(gchar *filename, int dither_mode, StatusBar *bar)
{
     gchar *tempname = get_temp_filename(0);
     gchar *argv[] = { "lame", "--decode", filename, tempname, NULL };
     Chunk *c;
     c = run_decoder(filename,tempname,"lame",argv,dither_mode,bar);
     g_free(tempname);
     return c;
}

static gboolean wav_regular_format(Dataformat *fmt)
{
     if (fmt->type == DATAFORMAT_FLOAT) return FALSE;
     return (XOR(fmt->samplesize == 1, fmt->sign));
}

static gboolean mp3_save(Chunk *chunk, gchar *filename, struct file_type *type,
			 int dither_mode, StatusBar *bar, gboolean *fatal)
{
     Chunk *x;
     Dataformat fmt;
     gchar *c;
     gboolean b;
     xunlink(filename);
     c = g_strdup_printf("OUTFILE=%s",filename);
     if (xputenv(c)) { g_free(c); return TRUE; }
     if (wav_regular_format(&(chunk->format)))
	 b = pipe_dialog_send_chunk(chunk,
				    "lame --silent --preset standard - "
				    "\"$OUTFILE\"",TRUE,dither_mode,bar);
     else {
	  memcpy(&fmt,&(chunk->format),sizeof(Dataformat));
	  if (fmt.type == DATAFORMAT_FLOAT || fmt.samplesize == 3) {
	       fmt.type = DATAFORMAT_PCM;
	       fmt.samplesize = 4;
	  }
	  fmt.sign = (fmt.samplesize > 1);
	  fmt.bigendian = FALSE;
	  fmt.samplebytes = fmt.samplesize * fmt.channels;
	  x = chunk_convert_sampletype(chunk,&fmt);
	  if (x == NULL) b=TRUE;
	  else {
	       b = pipe_dialog_send_chunk(x,
					  "lame --silent --preset standard - "
					  "\"$OUTFILE\"",TRUE,dither_mode,bar);
	       gtk_object_sink(GTK_OBJECT(x));
	  }
     }
     if (!xunsetenv("OUTFILE")) g_free(c);
     if (b || !file_exists(filename)) {
	  *fatal = TRUE;
	  return TRUE;
     }
     return FALSE;
}


/* Mplayer */

static Chunk *try_mplayer(gchar *filename, int dither_mode, StatusBar *bar)
{
     char *tempname = get_temp_filename(0);
     char *argv[] = { "mplayer", "-quiet", "-noconsolecontrols","-ao","pcm",
		      "-aofile",tempname,"-vc","dummy","-vo","null",
		      filename,NULL };
     if (!program_exists("mplayer")) return NULL;
     return run_decoder(filename,tempname,"mplayer",argv,dither_mode,bar);
}