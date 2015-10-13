# Freedesktop.org notifications event collection plugin

plugins_LTLIBRARIES += \
	fdo-notifications.la \
	$(null)

dist_event_DATA += \
	%D%/events/libnotify.event \
	%D%/events/libnotify.action \
	$(null)

dbussessionservice_DATA += \
	%D%/services/org.eventd.fdo-notifications.service \
	$(null)


fdo_notifications_la_SOURCES = \
	%D%/src/fdo-notifications.c \
	$(null)

fdo_notifications_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D FDONOTIFICATIONSCAPABILITIESDIR=\"$(fdonotificationscapabilitiesdir)\" \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

fdo_notifications_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

fdo_notifications_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
