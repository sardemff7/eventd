# Freedesktop.org notifications event collection plugin

plugins_LTLIBRARIES += \
	fdo-notifications.la \
	$(null)

dist_event_DATA += \
	%D%/events/notification.event \
	%D%/events/notification.action \
	$(null)

if EV_OS_UNIX
dbussessionservice_DATA += \
	%D%/services/org.eventd.fdo-notifications.service \
	$(null)
endif


fdo_notifications_la_SOURCES = \
	%D%/src/fdo-notifications.c \
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
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
