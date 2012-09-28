# EVENT protocol plugin
plugins_LTLIBRARIES += \
	evp.la

evp_la_SOURCES = \
	src/plugins/evp/evp.c

evp_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-evp\" \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

evp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

evp_la_LIBADD = \
	libeventd-event.la \
	libeventd-evp.la \
	libeventd.la \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

man1_MANS += \
	man/eventd-evp.1

man5_MANS += \
	man/eventd-evp.conf.5 \
	man/eventd-evp.event.5


if ENABLE_AVAHI

evp_la_SOURCES += \
	src/plugins/evp/avahi.h \
	src/plugins/evp/avahi.c

endif


if ENABLE_SYSTEMD

systemduserunit_DATA += \
	data/units/eventd-evp.socket

endif
