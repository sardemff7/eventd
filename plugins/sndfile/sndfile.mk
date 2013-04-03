# sndfile plugin

if ENABLE_SNDFILE
plugins_LTLIBRARIES += \
	sndfile.la

man5_MANS += \
	plugins/sndfile/man/eventd-sndfile.event.5
endif


sndfile_la_SOURCES = \
	plugins/sndfile/src/pulseaudio.h \
	plugins/sndfile/src/pulseaudio.c \
	plugins/sndfile/src/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-sndfile\" \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

sndfile_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
