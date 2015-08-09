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
	server/libeventd-plugin/include/eventd-plugin.h \
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

EventdPlugin-0.gir: EventdEvent-0.gir libeventd-plugin.la
EventdPlugin_0_gir_INCLUDES = GObject-2.0 Gio-2.0 EventdEvent-0
EventdPlugin_0_gir_CFLAGS = $(AM_CPPFLAGS) $(libeventd_plugin_la_CFLAGS) $(CPPFLAGS) $(CFLAGS)
EventdPlugin_0_gir_LIBS = libeventd-plugin.la
EventdPlugin_0_gir_FILES = $(filter-out %-private.h,$(libeventd_plugin_la_SOURCES))
INTROSPECTION_GIRS += EventdPlugin-0.gir
