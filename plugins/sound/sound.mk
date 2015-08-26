# sound plugin

if ENABLE_SOUND
plugins_LTLIBRARIES += \
	sound.la \
	$(null)

man5_MANS += \
	plugins/sound/man/eventd-sound.event.5 \
	$(null)

fdonotificationscapabilities_DATA += \
	plugins/sound/fdonotificationscapabilities/sound.capabilities \
	$(null)
endif


sound_la_SOURCES = \
	plugins/sound/src/pulseaudio.h \
	plugins/sound/src/pulseaudio.c \
	plugins/sound/src/sound.c \
	$(null)

sound_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-sound\" \
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
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(SNDFILE_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


sound_fdo_notifications_capabilities = \
	sound \
	$(null)

plugins/sound/fdonotificationscapabilities/sound.capabilities: src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	echo $(sound_fdo_notifications_capabilities) > $@

CLEANFILES += \
	plugins/sound/fdonotificationscapabilities/sound.capabilities \
	$(null)
