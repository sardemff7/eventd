# notification plugin

if ENABLE_NOTIFICATION_DAEMON
plugins_LTLIBRARIES += \
	nd.la \
	$(null)

man1_MANS += \
	%D%/man/eventdctl-nd.1 \
	$(null)

man5_MANS += \
	%D%/man/eventd-nd.conf.5 \
	$(null)

ndeventdir = $(eventdir)/nd
dist_ndevent_DATA = \
	%D%/events/eventd-nd-more.event \
	%D%/events/eventd-nd-more.action \
	%D%/events/eventd-nd-more-top-left.action \
	%D%/events/eventd-nd-more-top.action \
	%D%/events/eventd-nd-more-top-right.action \
	%D%/events/eventd-nd-more-bottom-left.action \
	%D%/events/eventd-nd-more-bottom.action \
	%D%/events/eventd-nd-more-bottom-right.action \
	$(null)
endif


nd_la_SOURCES = \
	%D%/src/types.h \
	%D%/src/nd.h \
	%D%/src/nd.c \
	%D%/src/notification.h \
	%D%/src/notification.c \
	%D%/src/draw.c \
	%D%/src/draw.h \
	%D%/src/style.c \
	%D%/src/style.h \
	%D%/src/pixbuf.h \
	%D%/src/pixbuf.c \
	%D%/src/backend.h \
	%D%/src/backends.h \
	%D%/src/backends.c \
	$(null)

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
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
	$(NKUTILS_LIBS) \
	$(GLIB_LIBS) \
	$(null)


#
# Backends
#

ndbackendsdir = $(pluginsdir)/nd
ndbackends_LTLIBRARIES =

ND_BACKENDS_CFLAGS = \
	$(AM_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

ND_BACKENDS_LIBS = \
	libeventd.la \
	libeventd-helpers.la \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS) \
	$(null)

if ENABLE_ND_WAYLAND
include src/libgwater/wayland.mk

ndbackends_LTLIBRARIES += \
	wayland.la \
	$(null)

wayland_la_SOURCES = \
	%D%/src/backend.h \
	%D%/src/backend-wayland.c \
	$(null)

nodist_wayland_la_SOURCES = \
	%D%/src/notification-area-unstable-v1-protocol.c \
	%D%/src/notification-area-unstable-v1-client-protocol.h \
	$(null)

wayland_la_CFLAGS = \
	-I $(builddir)/%D%/src \
	$(ND_BACKENDS_CFLAGS) \
	$(GW_WAYLAND_CFLAGS) \
	$(null)

wayland_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

wayland_la_LIBADD = \
	$(ND_BACKENDS_LIBS) \
	$(GW_WAYLAND_LIBS) \
	$(null)

CLEANFILES += \
	%D%/src/notification-area-unstable-v1-protocol.c \
	%D%/src/notification-area-unstable-v1-client-protocol.h \
	$(null)

wayland.la %D%/src/wayland_la-backend-wayland.lo: %D%/src/notification-area-unstable-v1-client-protocol.h
endif

if ENABLE_ND_XCB
include src/libgwater/xcb.mk

ndbackends_LTLIBRARIES += \
	xcb.la \
	$(null)

xcb_la_SOURCES = \
	%D%/src/backend.h \
	%D%/src/backend-xcb.c \
	$(null)

xcb_la_CFLAGS = \
	$(ND_BACKENDS_CFLAGS) \
	$(GW_XCB_CFLAGS) \
	$(null)

xcb_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

xcb_la_LIBADD = \
	$(ND_BACKENDS_LIBS) \
	$(GW_XCB_LIBS) \
	$(null)
endif

if ENABLE_ND_FBDEV
ndbackends_LTLIBRARIES += \
	fbdev.la \
	$(null)

fbdev_la_SOURCES = \
	%D%/src/backend.h \
	%D%/src/backend-fbdev.c \
	$(null)

fbdev_la_CFLAGS = \
	$(ND_BACKENDS_CFLAGS) \
	$(null)

fbdev_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

fbdev_la_LIBADD = \
	$(ND_BACKENDS_LIBS) \
	$(null)
endif

if ENABLE_ND_WINDOWS
include src/libgwater/win.mk

ndbackends_LTLIBRARIES += \
	win.la \
	$(null)

win_la_SOURCES = \
	%D%/src/backend.h \
	%D%/src/backend-win.c \
	$(null)

win_la_CFLAGS = \
	$(ND_BACKENDS_CFLAGS) \
	$(GW_WIN_CFLAGS) \
	$(null)

win_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

win_la_LIBADD = \
	$(ND_BACKENDS_LIBS) \
	$(GW_WIN_LIBS) \
	$(null)
endif


#
# Special rules
#

# Wayland protocol code generation rules
%D%/src/%-protocol.c : $(wnadir)/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < $< > $@

%D%/src/%-server-protocol.h : $(wnadir)/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) server-header < $< > $@

%D%/src/%-client-protocol.h : $(wnadir)/%.xml
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < $< > $@



#
# Hooks
#

install-data-hook la-files-install-hook: nd-la-files-install-hook
uninstall-hook la-files-uninstall-hook: nd-la-files-uninstall-hook

# *.la files cleanup
nd-la-files-install-hook:
	$(call ev_remove_la,ndbackends)

# Remove *.so files at uninstall since
# we remove *.la files at install
nd-la-files-uninstall-hook:
	$(call ev_remove_so_from_la,ndbackends)
