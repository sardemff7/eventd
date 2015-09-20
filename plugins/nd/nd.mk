# notification plugin

AM_CPPFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


if ENABLE_NOTIFICATION_DAEMON
plugins_LTLIBRARIES += \
	nd.la \
	$(null)

pkginclude_HEADERS += \
	%D%/include/eventd-nd-backend.h \
	$(null)

man1_MANS += \
	%D%/man/eventdctl-nd.1 \
	$(null)

man5_MANS += \
	%D%/man/eventd-nd.conf.5 \
	%D%/man/eventd-nd.event.5 \
	$(null)

dist_fdonotificationscapabilities_DATA += \
	%D%/fdonotificationscapabilities/nd.capabilities \
	$(null)

if ENABLE_GDK_PIXBUF
dist_fdonotificationscapabilities_DATA += \
	%D%/fdonotificationscapabilities/nd-icons.capabilities \
	$(null)
endif

endif


nd_la_SOURCES = \
	%D%/src/icon.h \
	%D%/src/cairo.c \
	%D%/src/cairo.h \
	%D%/src/style.c \
	%D%/src/style.h \
	%D%/include/eventd-nd-backend.h \
	%D%/src/backends.c \
	%D%/src/backends.h \
	%D%/src/nd.c \
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
	%D%/src/icon.c \
	$(null)
endif

ndbackendsdir = $(pluginsdir)/nd

ndbackends_LTLIBRARIES =

include %D%/xcb/xcb.mk
include %D%/linux/linux.mk


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
