# Sound plugin
plugins_LTLIBRARIES += \
	sound.la

sound_la_SOURCES = \
	src/plugins/sound/pulseaudio-internal.h \
	src/plugins/sound/pulseaudio.h \
	src/plugins/sound/sound.c

sound_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

sound_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

sound_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
sound_la_SOURCES += \
	src/plugins/sound/pulseaudio.c
else
sound_la_SOURCES += \
	src/plugins/sound/pulseaudio-dummy.c
endif
