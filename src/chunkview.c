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

#include <math.h>
#include "chunkview.h"
#include "main.h"
#include "inifile.h"
#include "player.h"

static GtkObjectClass *parent_class;
static guint font_height=0;

enum { VIEW_CHANGED_SIGNAL, SELECTION_CHANGED_SIGNAL, CURSOR_CHANGED_SIGNAL, 
       CHUNK_CHANGED_SIGNAL, DOUBLE_CLICK_SIGNAL, LAST_SIGNAL };
static gint chunk_view_signals[LAST_SIGNAL] = { 0 };

static void chunk_view_update_image(ChunkView *view, guint xs, guint xe);
static void chunk_view_redraw_samples(ChunkView *cv, off_t start, off_t end);
static gint calc_x(ChunkView *cv, off_t sample, off_t width);

static void chunk_view_changed(Document *d, gpointer user_data)
{
     ChunkView *cv = CHUNKVIEW(user_data);
     if (cv->image != NULL) 
	  chunk_view_update_image(cv,0,cv->image_width-1);
     gtk_widget_queue_draw(GTK_WIDGET(cv));
}

static void chunk_view_selection_changed(Document *d, gpointer user_data)
{
     off_t os,oe,ns,ne;

     ChunkView *cv = CHUNKVIEW(user_data);

     /* Calculate the user-visible part of the old selection */
     os = MAX(d->viewstart, d->old_selstart);
     oe = MIN(d->viewend,   d->old_selend);
     
     /* Calculate user-visible part of the new selection */
     ns = MAX(d->viewstart, d->selstart);
     ne = MIN(d->viewend,   d->selend);

     /* Different cases:
      *  1. Neither the old nor the new selection is in view.
      *  2. The old selection wasn't in view but the new is..
      *  3. The new selection is in view but the old wasn't..
      *  4. Both are in view and overlap
      *  5. Both are in view and don't overlap
      */

     if (os >= oe && ns >= ne) {
	  /* Case 1 - Do nothing */
     } else if (os >= oe) {
	  /* Case 2 - Draw the entire new sel. */
	  chunk_view_redraw_samples(cv,ns,ne);
     } else if (ns >= ne) {
	  /* Case 3 - Redraw the old sel. */
	  chunk_view_redraw_samples(cv,os,oe);
     } else if ((ns >= os && ns < oe) || (os >= ns && os < ne)) {
	  /* Case 4 - Redraw the changes on both sides */
	  if (os != ns)
	       chunk_view_redraw_samples(cv,MIN(os,ns),MAX(os,ns));
	  if (oe != ne)
	       chunk_view_redraw_samples(cv,MIN(oe,ne),MAX(oe,ne));
     } else {
	  /* Case 5 - Redraw both selections */
	  chunk_view_redraw_samples(cv,os,oe);
	  chunk_view_redraw_samples(cv,ns,ne);
     }
     
}

static void chunk_view_cursor_changed(Document *d, gboolean rolling, 
				      gpointer user_data)
{     
     ChunkView *cv = CHUNKVIEW(user_data);
     gboolean old_in_view,new_in_view;
     int curpix=0,oldpix=0;

     old_in_view = (d->old_cursorpos >= d->viewstart && 
		    d->old_cursorpos <  d->viewend);
     new_in_view = (d->cursorpos     >= d->viewstart && 
		    d->cursorpos     <  d->viewend);
     if (old_in_view)
	  oldpix = calc_x(cv,d->old_cursorpos,
			  GTK_WIDGET(cv)->allocation.width);
     if (new_in_view)
	  curpix = calc_x(cv,d->cursorpos,GTK_WIDGET(cv)->allocation.width);
     if (old_in_view && (!new_in_view || oldpix != curpix))
	  gtk_widget_queue_draw_area(GTK_WIDGET(cv),oldpix,0,1,
				     cv->image_height);
     if (new_in_view && (!old_in_view || oldpix != curpix))
	  gtk_widget_queue_draw_area(GTK_WIDGET(cv),curpix,0,1,
				     cv->image_height);
}

static void chunk_view_destroy (GtkObject *object)
{
     ChunkView *cv = CHUNKVIEW(object);
     if (cv->image) {
	  gdk_pixmap_unref(cv->image);
	  cv->image = NULL;
     }
     if (cv->doc) gtk_object_unref(GTK_OBJECT(cv->doc));
     cv->doc = NULL;
     parent_class->destroy(object);
     if (cv->cache) view_cache_free(cv->cache);
     cv->cache = NULL;
}

static gint calc_x(ChunkView *cv, off_t sample, off_t width)
{
     Document *d = cv->doc;
     gfloat f,g;
     if (sample < d->viewstart)
	  return -1;
     if (sample > d->viewend )
	  return width;
     else {
	  f = sample - d->viewstart;
	  g = d->viewend - d->viewstart;
	  return width * (f/g);
     }
}

static void chunk_view_update_image_main(ChunkView *cv, GdkDrawable *image,
					 guint xs, guint xe)
{
     int i,j,y;
     gint w,h;
     GtkWidget *wid = GTK_WIDGET(cv);
     Document *d = cv->doc;
     w = wid->allocation.width;
     h = wid->allocation.height;
     if (cv->timescale) h-=font_height;
     if (xe < xs) return;
     /* Svart bakgrund */
     gdk_draw_rectangle( image, get_gc(BACKGROUND,wid), TRUE, xs, 0, 
			 1+xe-xs,h);
     /* Om det inte finns n�gon sampling laddad, stanna h�r. */
     if (d == NULL) return;
     /* Rita ut markeringen med speciell f�rg */
     if (d->selstart != d->selend && d->selstart < d->viewend &&
	 d->selend > d->viewstart) {
	  if (d->selstart > d->viewstart)
	       i = calc_x( cv, d->selstart, w );
	  else 
	       i = 0;
	  if (i < xs) i=xs;
	  if (i <= xe) {
	       if (d->selend < d->viewend)
		    j = calc_x( cv, d->selend, w );
	       else
		    j = xe;
	       if (j > xe) j = xe;
	       if (j >= xs) {
		    gdk_draw_rectangle( image, get_gc(SELECTION,wid),TRUE, 
					i, 0, 1+j-i, h );
	       }
	  }
	  
	  // printf("i = %d, j = %d\n",i,j);
     }
     /* Rita gr�a streck */
     j = h/d->chunk->format.channels;
     for (i=0; i<d->chunk->format.channels; i++) {
	  y = j/2 + i*j;	  
	  gdk_draw_line(image,get_gc(BARS,wid),xs,y,xe,y);
     }
     player_work();
     /* Drawing one pixel to the left of xs is a workaround. Some part of the
      * code seems to draw one pixel wrong to the left. */
     view_cache_draw_part(cv->cache, image, (xs==0)?0:xs-1, xe, h, 
			  GTK_WIDGET(cv),
			  cv->scale_factor);
}

static void chunk_view_update_image(ChunkView *cv, guint xs, guint xe)
{
#if GTK_MAJOR_VERSION < 2
     GtkWidget *wid = GTK_WIDGET(cv);
     gint w,h;
     /* printf("chunk_view_update_image: %d->%d\n",xs,xe); */
     /* gint cx,v1,v2,pv1,pv2; */
     
     /* Skapa pixmap */          
     w = wid->allocation.width;
     h = wid->allocation.height;
     if (cv->timescale) h-=font_height;
     if (cv->image!=NULL && (w != cv->image_width || h != cv->image_height)) {
	  gdk_pixmap_unref(cv->image);
	  cv->image = NULL;	  
     }
     if (cv->image == NULL) {
	  cv->image = gdk_pixmap_new(wid->window, w, h, -1);
	  cv->image_width = w;
	  cv->image_height = h;
	  xs = 0;
	  xe = w-1;
	  /* Make the view cache aware of the new size */
	  view_cache_update(cv->cache,cv->chunk,cv->viewstart,cv->viewend,w,
			    NULL,NULL);
     }
     chunk_view_update_image_main(cv,cv->image,xs,xe);
#endif
}

struct draw_mark_data {
     ChunkView *view;
     GdkEventExpose *event;
};

static void draw_mark(gchar *label, off_t position, gpointer user_data)
{
     struct draw_mark_data *dd = (struct draw_mark_data *)user_data;
     ChunkView *cv = dd->view;
     GdkEventExpose *event = dd->event;
     GtkWidget *widget = GTK_WIDGET(cv);
     Document *d = cv->doc;
     guint i;

     if ( position >= d->viewstart && position <= d->viewend ) {
	  i = calc_x( cv, position, widget->allocation.width );
	  if (event->area.x <= i+10 && event->area.x+event->area.width > i) {
	       gdk_draw_line( widget->window, get_gc(MARK,widget), i, 
			      event->area.y, i, 
			      event->area.y+event->area.height-1);
	       /* Render the text */
	       gdk_gc_set_foreground(widget->style->fg_gc[0],get_color(MARK));
	       gtk_draw_string( widget->style, widget->window, 
				GTK_STATE_NORMAL, i+2, font_height+2, label );
	       gdk_gc_set_foreground(widget->style->fg_gc[0],get_color(BLACK));
/*	       gdk_draw_text( widget->window, widget->style->font, 
			      gc_mark(widget), i+2, 12, (gchar *)key, 
			      strlen(key) ); */
	  }
     }
}

/* Returns: 0 - No bars drawn (too short distance)
 * 1 - Large bars drawn but no text.
 * 2 - Large bars drawn with text or small bars drawn */
static guint draw_time_bars(ChunkView *view, GdkEventExpose *event, 
			    gfloat pixelspersec, gfloat secs, 
			    gboolean wantsmall, gboolean text)
{
     off_t x;
     char buf[32];
     gboolean big;
     gfloat f,g,h,q;
     gint i;
     long int mysec;
#if GTK_MAJOR_VERSION == 2
     PangoLayout *pl;
#endif
     GtkWidget *widget = GTK_WIDGET(view);
     Document *d = view->doc;
     g = pixelspersec*secs;
     /* printf("draw_time_bars (pixelspersec=%f,secs=%f,g=%f)\n",pixelspersec,secs,g); */
     if (g<6.0) return 0;
big = (!wantsmall && g>65.0);	// was 35.0
     /* printf("draw_time_bars: text=%d\n",(int)text); */
     f = (gfloat)d->viewstart / (gfloat)d->chunk->format.samplerate;
     h = 0.0;
     /* Avrunda f ner�t till n�rmaste multipel av secs */
     x = (off_t)(f/secs);
     q = ((gfloat)x)*secs;
     h = (q-f)*pixelspersec;
     f = q;
     /* Rita */
     do {
	  /* printf("draw_time_bars: f=%f, h=%f\n",f,h); */
	  gdk_draw_line( widget->window, widget->style->white_gc, h, 
			 widget->allocation.height-font_height-(big?7:3), 
			 h, widget->allocation.height-font_height);
	  if (big && text) {
	       mysec = f;
	       if (mysec < 60) g_snprintf(buf,sizeof(buf),"%ld", mysec);
	       else {
		    if (mysec < 3600) 
			 g_snprintf(buf,sizeof(buf),"%ld:%02ld", mysec/60, 
				    mysec%60);
		    else 
			 g_snprintf(buf,sizeof(buf),"%ld'%02ld:%02ld", 
				    mysec/3600, (mysec/60)%60, mysec%60);
	       }

#if GTK_MAJOR_VERSION == 1
	       i = gdk_string_width( widget->style->font, buf ) / 2;
	       gtk_draw_string( widget->style, widget->window, 
				GTK_STATE_NORMAL, h-i, 
				widget->allocation.height-1, buf );
#else
	       pl = gtk_widget_create_pango_layout( widget, buf );
	       pango_layout_get_pixel_size(pl, &i, NULL);
	       gdk_draw_layout( widget->window, widget->style->black_gc,
				h-i/2, widget->allocation.height-font_height,
				pl);
	       g_object_unref(G_OBJECT(pl));
#endif
	  }
	  h += g;
	  f += secs;
     } while (h<event->area.x+event->area.width);
     /* puts("draw_time_bars_finished"); */
     return (big)?2:1;
}

static void draw_timescale(ChunkView *view, GdkEventExpose *event, gboolean text)
{
     GtkWidget *widget = GTK_WIDGET(view);
     Document *d = view->doc;
     guint i;
     gfloat f;
     /* This table specify at which intervals big or small lines can be 
      * drawn. */
     gfloat bigsizes[] = { 1.0, 2.0, 5.0, 10.0, 20.0, 30.0, 
			   60.0, 120.0, 180.0, 300.0, 600.0, 900.0, 
			   1500.0, 3000.0 };
     /* This table is TRUE whenever the entry in the table above is
      * not an even divisor in the  entry that follows it. */ 
     gboolean bigskip[] = { FALSE, TRUE, FALSE, FALSE, TRUE, FALSE, 
			    FALSE, TRUE, TRUE, FALSE, TRUE, TRUE,
			    FALSE, FALSE };
     /* This table specifiy smaller intervals where small brs are only 
      * allowed. */
     gfloat smallsizes[] = { 0.1, 0.2, 0.5 };
     /* puts("draw_timescale"); */
     /* gdk_gc_set_foreground(widget->style->fg_gc[0],color_white()); */
     if (text) gdk_window_clear_area(widget->window, event->area.x, 
				     view->image_height,event->area.width,
				     font_height);
     /* Draw the horizontal line */
     i = widget->allocation.height-font_height;
     gdk_draw_line( widget->window, widget->style->white_gc, event->area.x, 
		    i, event->area.x+event->area.width-1, i);
     /* pixels/sec */
     f = (gfloat)(widget->allocation.width * d->chunk->format.samplerate) / 
	  (gfloat)(d->viewend - d->viewstart); 
     /* if (f > view->chunk->format.samplerate) 
	f=view->chunk->format.samplerate; */
     /* Draw the big bars */
     
     for (i=0; i<ARRAY_LENGTH(bigsizes); i++)
	  switch(draw_time_bars(view,event,f,bigsizes[i],FALSE,text)) {
	  case 0: break;
	  case 1: while (bigskip[i]) i++; break;
	  case 2: goto outer;
	  }
outer:
     for (i=0; i<ARRAY_LENGTH(smallsizes); i++)
	  if (draw_time_bars(view,event,f,smallsizes[i],TRUE,text)) break;

     
     /* gdk_gc_set_foreground(widget->style->fg_gc[0],color_black()); */
     /* puts("draw_timescale finished."); */
}

static gint chunk_view_expose(GtkWidget *widget, GdkEventExpose *event)
{
     ChunkView *cv = CHUNKVIEW(widget);
     Document *d = cv->doc;
     guint i;
     struct draw_mark_data dd;
     gboolean expose_timescale=FALSE, expose_text=FALSE;

     /* printf("Expose: (%d,%d)+(%d,%d)\n",event->area.x,event->area.y,
	event->area.width,event->area.height); */
#if GTK_MAJOR_VERSION < 2
     /* This call creates the pixmap if it doesn't exist,
      * otherwise does nothing (because xe<xs) */	     
     chunk_view_update_image(cv,1,0);

     gdk_draw_pixmap ( widget->window, get_gc(BLACK,widget), cv->image,
		       event->area.x, event->area.y, event->area.x, 
		       event->area.y, event->area.width, 
		       event->area.height );
#else
     chunk_view_update_image_main( cv, widget->window, event->area.x, 
				   event->area.x + event->area.width - 1 );
#endif

     if (d == NULL) return FALSE;

     /* Determine if the time scale or text/bars at the bottom needs
      * redrawing */
     if (cv->timescale) 
	  if (event->area.y+event->area.height > cv->image_height-7) {
	       expose_timescale=TRUE;
	       if (event->area.y+event->area.height > cv->image_height) {
		    expose_text = TRUE;
		    event->area.height = cv->image_height-event->area.y;
	       }
	  }

     /* Draw the marks */
     dd.view = cv;
     dd.event = event;
     document_foreach_mark( d, draw_mark, &dd );
     /* Draw the cursor */
     if ( d->cursorpos >= d->viewstart && 
	  d->cursorpos <= d->viewend ) {
	  i = calc_x( cv, d->cursorpos, widget->allocation.width );
	  if (event->area.x <= i && event->area.x+event->area.width > i) {
	       gdk_draw_line( widget->window, get_gc(CURSOR,widget), i, 
			      event->area.y, i, 
			      event->area.y+event->area.height-1 );
	  }
     }
     /* Draw the time scale */
     if (d->chunk && cv->timescale && expose_timescale) 
	  draw_timescale( cv, event, expose_text );
     return FALSE;
}


static void chunk_view_size_request(GtkWidget *widget, GtkRequisition *req)
{
     req->width = 300;
     req->height = 50;
}

static void chunk_view_size_allocate(GtkWidget *widget, GtkAllocation *all)
{
     ChunkView *cv = CHUNKVIEW(widget);
     Document *d = cv->doc;
     GTK_WIDGET_CLASS(parent_class)->size_allocate(widget,all);
     if (cv->image == NULL) { 	  /* Hack */
	  cv->image_width = all->width;
	  cv->image_height = all->height;
	  if (cv->timescale) cv->image_height -= font_height;
     }
     if (d == NULL) return;
     /* Call view_cache_update to make sure the view cache knows the new 
      * size. */
     view_cache_update(cv->cache,d->chunk,d->viewstart,d->viewend,
		       widget->allocation.width,NULL,NULL);
}

static off_t calc_sample(ChunkView *cv, gfloat x, gfloat xmax)
{
     return cv->doc->viewstart + (x/xmax) * 
	  (cv->doc->viewend-cv->doc->viewstart);
}

static gint dragmode; /* 0 = dragging sel. start, 1 = dragging sel. end, */
static off_t dragstart;
static off_t dragend;
static gboolean autoscroll = FALSE;
static ChunkView *autoscroll_view;
static gfloat autoscroll_amount;
static GTimeVal autoscroll_start;

static gint scroll_wheel(GtkWidget *widget, gdouble mouse_x, int direction)
{
     ChunkView *cv = CHUNKVIEW(widget);
     Document *d = cv->doc;
     gfloat zf;
     off_t ui1,ui2;
     if (d == NULL) return FALSE;
     dragstart = dragend = 
	  calc_sample( cv, mouse_x, (gfloat)widget->allocation.width);
	
     if (direction == -1)	// Scroll wheel down
     {
       if (d->viewend - d->viewstart == widget->allocation.width)
	    return FALSE;
       zf = 1.0/1.4;
     }
     else zf = 1.4;		// Scroll wheel up
     ui1 = dragstart-(off_t)((gfloat)(dragstart-d->viewstart))*zf;
     ui2 = dragstart+(off_t)((gfloat)(d->viewend-dragstart))*zf;
     if (ui1 < 0) ui1 = 0;
     if (ui2 < dragstart || ui2 > d->chunk->length) ui2 = d->chunk->length;
     document_set_view(d,ui1,ui2);
     return FALSE;
}

#if GTK_MAJOR_VERSION == 2
static gint chunk_view_scrollwheel(GtkWidget *widget, GdkEventScroll *event)
{
     if (event->direction == GDK_SCROLL_DOWN) return scroll_wheel(widget, event->x, -1);
     else return scroll_wheel(widget, event->x, 1);
}
#endif

static gint chunk_view_button_press(GtkWidget *widget, GdkEventButton *event)
{     
     ChunkView *cv = CHUNKVIEW(widget);
     gdouble sp,ep;
     off_t o;
     Document *d = cv->doc;
     if (d == NULL) return FALSE;
     if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
	  o = calc_sample(cv,event->x,(gfloat)widget->allocation.width);
	  gtk_signal_emit(GTK_OBJECT(cv),
			  chunk_view_signals[DOUBLE_CLICK_SIGNAL],&o);
	  return FALSE;
     }
     dragstart = dragend = 
	  calc_sample( cv, event->x, (gfloat)widget->allocation.width);

     /* Snap to sel start/end */
     sp = (gdouble)calc_x(cv,d->selstart,widget->allocation.width);
     ep = (gdouble)calc_x(cv,d->selend,widget->allocation.width);
     if (fabs(event->x - sp) < 3.0)
	  dragstart = dragend = d->selstart;
     else if (fabs(event->x - ep) < 3.0)
	  dragstart = dragend = d->selend;

     if (event->button == 1 || event->button == 2) {
	  dragmode = 1;
	  if (d->selend != d->selstart) {
	       if (d->selstart >= d->viewstart && 
		   dragstart == d->selstart){
		    dragmode = 0;
		    dragstart = d->selend;
		    dragend = d->selstart;
		    return FALSE;
	       } else if (d->selend <= d->viewend &&
			  dragstart == d->selend) {
		    dragmode = 1;
		    dragstart = d->selstart;
		    dragend = d->selend;
		    return FALSE;
	       }
	  }
	  document_set_selection( d, dragstart, dragend );
     } else if (event->button == 3) {
	  document_set_cursor(d,dragstart);
     } else if (event->button == 4 || event->button == 5) {
	  if (event->button == 5) scroll_wheel(widget, event->x, -1);
	  else scroll_wheel(widget, event->x, 1);
     }
     return FALSE;
}

static gint chunk_view_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
     gint x1,x2;
     static GdkCursor *arrows=NULL;
     ChunkView *cv = CHUNKVIEW(widget);
     Document *d = cv->doc;
     if (d == NULL) return FALSE;
     if ((event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)) != 0) {
	  if (event->x < widget->allocation.width)
	       dragend = calc_sample ( cv, (event->x > 0.0) ? event->x : 0.0, 
				       (gfloat)widget->allocation.width );
	  else
	       dragend = d->viewend;
       	  if (dragmode == 1) 
	       document_set_selection ( d, dragstart, dragend );
	  else
	       document_set_selection ( d, dragend, dragstart);
	  if (event->x < 0.0 || event->x > widget->allocation.width) {
	       if (!autoscroll) {
		    autoscroll = TRUE;
		    autoscroll_view = cv;
		    g_get_current_time(&autoscroll_start);
	       }
	       if (event->x < 0.0) autoscroll_amount = event->x;
	       else autoscroll_amount = event->x - widget->allocation.width;
	  } else {
	       autoscroll = FALSE;
	  }
     } else if (d->selstart != d->selend) {
	  if (d->selstart >= d->viewstart) 
	       x1 = calc_x(cv,d->selstart,widget->allocation.width);
	  else
	       x1 = -500;
	  if (d->selend <= d->viewend)
	       x2 = calc_x(cv,d->selend,widget->allocation.width);
	  else
	       x2 = -500;
	  if (fabs(event->x-(double)x1)<3.0 ||
	      fabs(event->x-(double)x2)<3.0) {
	       if (arrows == NULL)
		    arrows = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
	       gdk_window_set_cursor(widget->window,arrows);
	  } else {
	       gdk_window_set_cursor(widget->window,NULL);
	  }
     }
     return FALSE;
}

#define FLOAT(x) ((gfloat)(x))

gboolean chunk_view_autoscroll(void)
{
     GTimeVal current_time,diff;
     gfloat smp;
     off_t ismp;
     if (!autoscroll) return FALSE;
     g_get_current_time(&current_time);
     timeval_subtract(&diff,&current_time,&autoscroll_start);
     if (diff.tv_sec == 0 && diff.tv_usec < 100000) return FALSE;
     current_time.tv_usec -= current_time.tv_usec % 100000;
     memcpy(&autoscroll_start,&current_time,sizeof(autoscroll_start));
     smp = FLOAT(diff.tv_usec/100000 + diff.tv_sec*10) * 
	  autoscroll_amount *
	  FLOAT(autoscroll_view->doc->viewend-autoscroll_view->doc->viewstart)/
	  FLOAT(GTK_WIDGET(autoscroll_view)->allocation.width);
     if (smp < 0.0) {
	  ismp = (off_t)(-smp);
	  if (ismp > dragend)
	      ismp = dragend;
	  dragend -= ismp;
	  document_set_selection(autoscroll_view->doc,dragend,dragstart);
	  document_set_view(autoscroll_view->doc, dragend,
			      autoscroll_view->doc->viewend-ismp);
     } else {
	  ismp = (off_t)smp;
	  if (dragend+ismp > autoscroll_view->doc->chunk->length)
	       ismp = autoscroll_view->doc->chunk->length - dragend;
	  dragend += ismp;
	  document_set_selection(autoscroll_view->doc,dragstart,dragend);
	  document_set_view(autoscroll_view->doc,
			    autoscroll_view->doc->viewstart+ismp,
			    dragend);
     }
     return TRUE;
}

static gint chunk_view_button_release(GtkWidget *widget, GdkEventButton *event)
{
     ChunkView *cv = CHUNKVIEW(widget);
     if (event->button == 2) {
	  document_play_selection(cv->doc,FALSE,1.0);
     }
     autoscroll = FALSE;
     return FALSE;
}

static void chunk_view_class_init(GtkObjectClass *klass)
{
     GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
     ChunkViewClass *cvc = CHUNKVIEW_CLASS(klass);
     parent_class = gtk_type_class( gtk_drawing_area_get_type() );

     klass->destroy = chunk_view_destroy;
     wc->expose_event = chunk_view_expose;
     wc->size_request = chunk_view_size_request;
     wc->size_allocate = chunk_view_size_allocate;
     wc->button_press_event = chunk_view_button_press;
     wc->motion_notify_event = chunk_view_motion_notify;
     wc->button_release_event = chunk_view_button_release;
#if GTK_MAJOR_VERSION == 2
     wc->scroll_event = chunk_view_scrollwheel;
#endif
     cvc->double_click = NULL;

     chunk_view_signals[DOUBLE_CLICK_SIGNAL] = 
         gtk_signal_new("double-click", GTK_RUN_FIRST, GTK_CLASS_TYPE(klass),
                        GTK_SIGNAL_OFFSET(ChunkViewClass,double_click),
                        gtk_marshal_NONE__POINTER, GTK_TYPE_NONE, 1,
			GTK_TYPE_POINTER );

     gtk_object_class_add_signals(klass,chunk_view_signals,LAST_SIGNAL);
}

static void chunk_view_init(GtkObject *obj)
{
     ChunkView *cv;
     cv = CHUNKVIEW(obj);
     cv->doc = NULL;
     cv->image = NULL;
     cv->timescale = TRUE;
     cv->scale_factor = 1.0;
     cv->cache = view_cache_new();
     gtk_widget_add_events( GTK_WIDGET(obj), GDK_BUTTON_MOTION_MASK | 
			    GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK );
     if (!font_height) {
#if GTK_MAJOR_VERSION == 1
	  font_height = gdk_string_height( GTK_WIDGET(obj)->style->font, 
					   "0123456789")+3;
#else
	  PangoLayout *pl;
	  pl = gtk_widget_create_pango_layout( GTK_WIDGET(obj), "0123456789" );
	  pango_layout_get_pixel_size(pl, NULL, &font_height);
	  g_object_unref(pl);
#endif
     }
}

GtkType chunk_view_get_type(void)
{
     static GtkType id = 0;
     if (!id) {
	  GtkTypeInfo info = {
	       "ChunkView",
	       sizeof(ChunkView),
	       sizeof(ChunkViewClass),
	       (GtkClassInitFunc) chunk_view_class_init,
	       (GtkObjectInitFunc) chunk_view_init 
	  };
	  id = gtk_type_unique( gtk_drawing_area_get_type(), &info );
     }
     return id;
}

GtkWidget *chunk_view_new(void)
{
     return GTK_WIDGET(gtk_type_new(chunk_view_get_type()));
}

void chunk_view_set_document(ChunkView *cv, Document *doc)
{
     if (cv->doc == doc) return;
     if (cv->doc) {
	  gtk_signal_disconnect_by_data(GTK_OBJECT(cv->doc),cv);
	  gtk_object_unref(GTK_OBJECT(cv->doc));
     }
     cv->doc = doc;
     if (doc != NULL) { 
	  gtk_object_ref(GTK_OBJECT(doc)); 
	  gtk_object_sink(GTK_OBJECT(doc)); 
	  gtk_signal_connect(GTK_OBJECT(doc),"view_changed",
			     GTK_SIGNAL_FUNC(chunk_view_changed),cv);
	  gtk_signal_connect(GTK_OBJECT(doc),"state_changed",
			     GTK_SIGNAL_FUNC(chunk_view_changed),cv);
	  gtk_signal_connect(GTK_OBJECT(doc),"selection_changed",
			     GTK_SIGNAL_FUNC(chunk_view_selection_changed),cv);
	  gtk_signal_connect(GTK_OBJECT(doc),"cursor_changed",
			     GTK_SIGNAL_FUNC(chunk_view_cursor_changed),cv);
     }
     chunk_view_changed(cv->doc,cv);
}

static void chunk_view_redraw_samples(ChunkView *cv, off_t start, off_t end)
{
     guint startpix,endpix;

     startpix = calc_x(cv,start,GTK_WIDGET(cv)->allocation.width);
     endpix = calc_x(cv,end,GTK_WIDGET(cv)->allocation.width);

     if (cv->image)
	  chunk_view_update_image(cv,startpix,endpix);

     /* printf("Calling queue_draw on: (%d,%d)+(%d,%d)\n",startpix,0,
	    1+endpix-startpix,GTK_WIDGET(cv)->allocation.height-
	    (cv->timescale?font_height:0)); */

     gtk_widget_queue_draw_area(GTK_WIDGET(cv),startpix,0,1+endpix-startpix,
				GTK_WIDGET(cv)->allocation.height-
				(cv->timescale?font_height:0));
}

void chunk_view_set_timescale(ChunkView *cv, gboolean scale_visible)
{
     if (cv->timescale == scale_visible) return;
     cv->timescale = scale_visible;
     chunk_view_changed(cv->doc,cv);
}

gboolean chunk_view_update_cache(ChunkView *view)
{
     gboolean b;
     gint i,j;     
     b = view_cache_update(view->cache, view->doc->chunk, view->doc->viewstart,
			   view->doc->viewend, view->image_width, &i, &j);

     if (!b && view->need_redraw_left != -1) {
	  gtk_widget_queue_draw_area(GTK_WIDGET(view),
				     view->need_redraw_left,
				     0,view->need_redraw_right-
				     view->need_redraw_left,
				     view->image_height);	      
	  view->need_redraw_left = -1;
     }

     if (b && i!=j) {
	  if (view->need_redraw_left == -1) {
	       view->need_redraw_left = i;
	       view->need_redraw_right = j;
	  } else {
	       if (i < view->need_redraw_left) view->need_redraw_left=i;
	       if (j > view->need_redraw_right) view->need_redraw_right=j;
	  }
	  chunk_view_update_image(view,i,j);
	  /* Repaint after 20 pixels have been updated or once per second 
	   * or we're just finished. */
	  if (view->need_redraw_right-view->need_redraw_left > 20 || 
	      (view->last_redraw_time != time(0)) ||
	      view_cache_uptodate(view->cache)) {
	       gtk_widget_queue_draw_area(GTK_WIDGET(view),
					  view->need_redraw_left,
					  0,view->need_redraw_right-
					  view->need_redraw_left,
					  view->image_height);
	       view->last_redraw_time = time(0);
	       view->need_redraw_left = -1;
	  }
     }
     return b;
}

void chunk_view_force_repaint(ChunkView *cv)
{
     chunk_view_changed(cv->doc,cv);
}

void chunk_view_set_scale(ChunkView *cv, gfloat scale)
{
     cv->scale_factor = scale;
     chunk_view_changed(cv->doc,cv);
}