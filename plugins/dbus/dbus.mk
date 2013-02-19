# dbus plugin

plugins_LTLIBRARIES += \
	dbus.la

man1_MANS += \
	plugins/dbus/man/eventd-dbus.1

pkgconfig_DATA += \
	plugins/dbus/pkgconfig/eventd-dbus.pc

dist_event_DATA += \
	plugins/dbus/events/libnotify.event

dbussessionservice_DATA += \
	plugins/dbus/services/org.eventd.dbus.service


dbus_la_SOURCES = \
	plugins/dbus/src/dbus.c

dbus_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-dbus\" \
	-D DBUSCAPABILITIESDIR=\"$(dbuscapabilitiesdir)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

dbus_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

dbus_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
