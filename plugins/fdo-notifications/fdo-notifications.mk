# Freedesktop.org notifications event collection plugin

plugins_LTLIBRARIES += \
	fdo-notifications.la \
	$(null)

man1_MANS += \
	plugins/fdo-notifications/man/eventd-fdo-notifications.1 \
	$(null)

dist_event_DATA += \
	plugins/fdo-notifications/events/libnotify.event \
	$(null)

dbussessionservice_DATA += \
	plugins/fdo-notifications/services/org.eventd.fdo-notifications.service \
	$(null)


fdo_notifications_la_SOURCES = \
	plugins/fdo-notifications/src/fdo-notifications.c \
	$(null)

fdo_notifications_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-fdo-notifications\" \
	-D FDONOTIFICATIONSCAPABILITIESDIR=\"$(fdonotificationscapabilitiesdir)\" \
	$(GIO_CPPFLAGS) \
	$(GOBJECT_CPPFLAGS) \
	$(GLIB_CPPFLAGS) \
	$(null)

fdo_notifications_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

fdo_notifications_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

fdo_notifications_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
