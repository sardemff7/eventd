# sndfile plugin
if ENABLE_SNDFILE
plugins_LTLIBRARIES += \
	sndfile.la

sndfile_la_SOURCES = \
	src/plugins/sndfile/pulseaudio.h \
	src/plugins/sndfile/sndfile-internal.h \
	src/plugins/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

sndfile_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
sndfile_la_SOURCES += \
	src/plugins/sndfile/pulseaudio.c
else
sndfile_la_SOURCES += \
	src/plugins/sndfile/pulseaudio-dummy.c
endif
endif
