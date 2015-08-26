# dbus plugin

plugins_LTLIBRARIES += \
	dbus.la \
	$(null)

man1_MANS += \
	plugins/dbus/man/eventd-dbus.1 \
	$(null)

dist_event_DATA += \
	plugins/dbus/events/libnotify.event \
	$(null)

dbussessionservice_DATA += \
	plugins/dbus/services/org.eventd.dbus.service \
	$(null)


dbus_la_SOURCES = \
	plugins/dbus/src/dbus.c \
	$(null)

dbus_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-dbus\" \
	-D DBUSCAPABILITIESDIR=\"$(dbuscapabilitiesdir)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

dbus_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

dbus_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
