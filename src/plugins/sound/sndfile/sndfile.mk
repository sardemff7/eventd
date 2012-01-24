# sndfile plugin
if ENABLE_SNDFILE
soundplugins_LTLIBRARIES += \
	sndfile.la

sndfile_la_SOURCES = \
	src/plugins/sound/sndfile/pulseaudio.h \
	src/plugins/sound/sndfile/sndfile-internal.h \
	src/plugins/sound/sndfile/sndfile.c

sndfile_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sndfile_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

sndfile_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
sndfile_la_SOURCES += \
	src/plugins/sound/sndfile/pulseaudio.c
else
sndfile_la_SOURCES += \
	src/plugins/sound/sndfile/pulseaudio-dummy.c
endif
endif
