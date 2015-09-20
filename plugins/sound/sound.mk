# sound plugin

if ENABLE_SOUND
plugins_LTLIBRARIES += \
	sound.la \
	$(null)

dist_man5_MANS += \
	%D%/man/eventd-sound.event.5 \
	$(null)

dist_fdonotificationscapabilities_DATA += \
	%D%/fdonotificationscapabilities/sound.capabilities \
	$(null)
endif


sound_la_SOURCES = \
	%D%/src/pulseaudio.h \
	%D%/src/pulseaudio.c \
	%D%/src/sound.c \
	$(null)

sound_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SNDFILE_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

sound_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

sound_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
