# dbus plugin
plugins_LTLIBRARIES += \
	dbus.la

dbus_la_SOURCES = \
	src/plugins/dbus/dbus.c

dbus_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-dbus\" \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

dbus_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex 'eventd_plugin_(id|get_info)'

dbus_la_LIBADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

dist_pkgdata_DATA += \
	data/libnotify.conf
