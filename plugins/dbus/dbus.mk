# dbus plugin

XSLTPROC_CONDITIONS += enable_dbus


plugins_LTLIBRARIES += \
	dbus.la

libexec_PROGRAMS += \
	eventd-notification-daemon

man1_MANS += \
	plugins/dbus/man/eventd-dbus.1

dist_pkgdata_DATA += \
	plugins/dbus/events/libnotify.event

dbussessionservice_DATA += \
	plugins/dbus/services/org.eventd.dbus.service


dbus_la_SOURCES = \
	plugins/dbus/src/dbus.c

dbus_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-dbus\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

dbus_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

dbus_la_LIBADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)


eventd_notification_daemon_SOURCES = \
	plugins/dbus/helper/main.c

eventd_notification_daemon_CFLAGS = \
	$(AM_CFLAGS) \
	-D BINDIR=\"$(bindir)\"
