# espeak plugin
if ENABLE_ESPEAK
plugins_LTLIBRARIES += \
	espeak.la

espeak_la_SOURCES = \
	src/plugins/espeak/pulseaudio.h \
	src/plugins/espeak/espeak.h \
	src/plugins/espeak/espeak.c

espeak_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS)

espeak_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

espeak_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(ESPEAK_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_PULSEAUDIO
espeak_la_SOURCES += \
	src/plugins/espeak/pulseaudio.c
else
espeak_la_SOURCES += \
	src/plugins/espeak/pulseaudio-dummy.c
endif
endif
