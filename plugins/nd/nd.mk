# notification plugin

AM_CPPFLAGS += \
	-I $(srcdir)/plugins/nd/include \
	$(null)


if ENABLE_NOTIFICATION_DAEMON
plugins_LTLIBRARIES += \
	nd.la \
	$(null)

pkginclude_HEADERS += \
	plugins/nd/include/eventd-nd-backend.h \
	$(null)

man1_MANS += \
	plugins/nd/man/eventdctl-nd.1 \
	$(null)

man5_MANS += \
	plugins/nd/man/eventd-nd.conf.5 \
	plugins/nd/man/eventd-nd.event.5 \
	$(null)

dist_fdonotificationscapabilities_DATA += \
	plugins/nd/fdonotificationscapabilities/nd.capabilities \
	$(null)

if ENABLE_GDK_PIXBUF
dist_fdonotificationscapabilities_DATA += \
	plugins/nd/fdonotificationscapabilities/nd-icons.capabilities \
	$(null)
endif

endif


nd_la_SOURCES = \
	plugins/nd/src/icon.h \
	plugins/nd/src/cairo.c \
	plugins/nd/src/cairo.h \
	plugins/nd/src/style.c \
	plugins/nd/src/style.h \
	plugins/nd/include/eventd-nd-backend.h \
	plugins/nd/src/backends.c \
	plugins/nd/src/backends.h \
	plugins/nd/src/nd.c \
	$(null)

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GDK_PIXBUF_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

nd_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GDK_PIXBUF_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS) \
	$(null)

if ENABLE_GDK_PIXBUF
nd_la_SOURCES += \
	plugins/nd/src/icon.c \
	$(null)
endif

ndbackendsdir = $(pluginsdir)/nd

ndbackends_LTLIBRARIES =

include plugins/nd/xcb/xcb.mk
include plugins/nd/linux/linux.mk


#
# Hooks
#

 ifneq (,$(strip $(ndbackends_LTLIBRARIES)))
install-data-hook la-files-install-hook: nd-la-files-install-hook
uninstall-hook la-files-uninstall-hook: nd-la-files-uninstall-hook
 endif

# *.la files cleanup
nd-la-files-install-hook:
	cd $(DESTDIR)$(ndbackendsdir) && \
		rm $(ndbackends_LTLIBRARIES)

# Remove *.so files at uninstall since
# we remove *.la files at install
nd-la-files-uninstall-hook:
	cd $(DESTDIR)$(ndbackendsdir) && \
		rm $(ndbackends_LTLIBRARIES:.la=.so)
