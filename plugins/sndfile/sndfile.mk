# sndfile plugin

XSLTPROC_CONDITIONS += enable_sndfile


plugins_LTLIBRARIES += \
	sndfile.la


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
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
