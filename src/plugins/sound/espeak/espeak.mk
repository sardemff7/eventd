# espeak plugin
if ENABLE_ESPEAK
soundplugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/sound/espeak/pulseaudio.h \
	src/plugins/sound/espeak/espeak.h \
	src/plugins/sound/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(sound_la_LDFLAGS)

espeak_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
espeak_la_SOURCES += \
	src/plugins/sound/espeak/pulseaudio.c
else
espeak_la_SOURCES += \
	src/plugins/sound/espeak/pulseaudio-dummy.c
endif
endif
