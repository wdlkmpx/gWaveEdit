bin_PROGRAMS = gwaveedit

gwaveedit_SOURCES = chunk.c int_box.c sound.c effectdlg_speed.c              \
                     chunkview.c main.c um.c                              \
                     effectdialog.c mainwindow.c ringbuf.c                \
                     effectdlg_volume.c float_box.c player.c                  \
                     effectdlg_samplerate.c gtkfiles.c effectdlg_samplesize.c     \
                     formatselector.c recorddialog.c vu_meter.c inifile.c \
                     configdialog.c effectdlg_combinechannels.c rawdialog.c   \
		     gotodialog.c effectdlg_pipe.c historybox.c viewcache.c   \
		     filetypes.c datasource.c statusbar.c effectbrowser.c \
		     effectdlg_sox.c help.c ladspacore.c effectdlg_ladspa.c       \
		     tempfile.c listobject.c documentlist.c combo.c       \
		     rateconv.c dataformat.c rateest.c document.c         \
		     session.c effectdlg_mapchannels.c mainloop.c             \
		     recordformatcombo.c effectdlg_sandwich.c

AM_CPPFLAGS = $(GTK_CFLAGS) $(SDL_CFLAGS) $(SNDFILE_CFLAGS) $(JACK_CFLAGS) \
	   $(LIBSAMPLERATE_CFLAGS) \
           $(PULSE_CFLAGS) -DDATADIR='"$(datadir)"'
LDADD = $(GTK_LIBS) $(SDL_LIBS) $(PORTAUDIO_LIBS) $(SNDFILE_LIBS) $(ALSALIB_LIBS) \
        $(JACK_LIBS) $(LIBSAMPLERATE_LIBS) \
        $(PULSE_LIBS)

localedir = $(datadir)/locale
AM_CPPFLAGS += -DLOCALEDIR=\"$(localedir)\"

CLEANFILES = *~ core core.* #*#
DISTCLEANFILES = .deps/*.P
