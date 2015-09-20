# tts plugin

if ENABLE_TTS
plugins_LTLIBRARIES += \
	tts.la \
	$(null)

dist_man5_MANS += \
	%D%/man/eventd-tts.event.5 \
	$(null)
endif


tts_la_SOURCES = \
	%D%/src/tts.c \
	$(null)

tts_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ESPEAK_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

tts_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

tts_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(ESPEAK_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
