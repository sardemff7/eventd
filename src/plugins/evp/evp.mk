# EVENT protocol plugin
plugins_LTLIBRARIES += \
	evp.la

evp_la_SOURCES = \
	src/plugins/evp/evp.c

evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"evp\" \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

evp_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

if ENABLE_AVAHI
evp_la_SOURCES += \
	src/plugins/evp/avahi.h \
	src/plugins/evp/avahi.c
endif
