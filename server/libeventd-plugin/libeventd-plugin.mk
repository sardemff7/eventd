# Plugin and core interfaces manipulation library

LIBEVENTD_PLUGIN_CURRENT=0
LIBEVENTD_PLUGIN_REVISION=0
LIBEVENTD_PLUGIN_AGE=0


AM_CPPFLAGS += \
	-I $(srcdir)/server/libeventd-plugin/include \
	$(null)


lib_LTLIBRARIES += \
	libeventd-plugin.la \
	$(null)

pkginclude_HEADERS += \
	server/libeventd-plugin/include/eventd-plugin.h \
	$(null)

pkgconfig_DATA += \
	server/libeventd-plugin/pkgconfig/libeventd-plugin.pc \
	$(null)


libeventd_plugin_la_SOURCES = \
	server/libeventd-plugin/include/eventd-plugin-interfaces.h \
	server/libeventd-plugin/src/core.c \
	server/libeventd-plugin/src/plugin.c \
	$(null)

libeventd_plugin_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"libeventd-plugin\" \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

libeventd_plugin_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBEVENTD_PLUGIN_CURRENT):$(LIBEVENTD_PLUGIN_REVISION):$(LIBEVENTD_PLUGIN_AGE) \
	$(null)

libeventd_plugin_la_LIBADD = \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
