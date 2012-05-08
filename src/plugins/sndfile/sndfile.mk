# sndfile plugin
plugins_LTLIBRARIES += \
	sndfile.la

sndfile_la_SOURCES = \
	src/plugins/sndfile/pulseaudio.h \
	src/plugins/sndfile/pulseaudio.c \
	src/plugins/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-sndfile\" \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex 'eventd_plugin_(id|get_info)'

sndfile_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
