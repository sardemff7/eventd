# tts plugin

if ENABLE_TTS
plugins_LTLIBRARIES += \
	tts.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-tts.conf.5 \
	$(null)
endif


tts_la_SOURCES = \
	%D%/src/tts.c \
	$(null)

tts_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SPEECHD_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

tts_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

tts_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(SPEECHD_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
