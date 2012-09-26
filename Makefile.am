AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemduserunitdir="$$dc_install_base/$(systemduserunitdir)" \
	--disable-debug

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

pkgconfigdir = $(libdir)/pkgconfig
vapidir = $(datadir)/vala/vapi
pluginsdir = $(pkglibdir)/plugins

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
noinst_LTLIBRARIES =
pkginclude_HEADERS =
pkgconfig_DATA =
dist_vapi_DATA =
dist_pkgdata_DATA =
systemduserunit_DATA =
man1_MANS =
man5_MANS =

EXTRA_DIST =
CLEANFILES =

AM_CFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	--include config.h \
	-I $(srcdir)/include/ \
	-I $(builddir)/include/

AM_VALAFLAGS += \
	--vapidir $(srcdir)/vapi/ \
	--vapidir $(builddir)/vapi/ \
	--pkg config

AM_XSLTPROCFLAGS += \
	--path "$(srcdir)/man/" \
	--path "$(builddir)/man/"

EXTRA_DIST += \
	vapi/libeventd-evp.vapi \
	vapi/glib-local-2.0.vapi \
	vapi/config.vapi

include src/libeventd-event/libeventd-event.mk
include src/libeventd-evp/libeventd-evp.mk
include src/libeventd/libeventd.mk
include src/eventd/eventd.mk
include src/eventdctl/eventdctl.mk
include src/plugins/evp/evp.mk
if ENABLE_DBUS
include src/plugins/dbus/dbus.mk
endif
if ENABLE_SNDFILE
include src/plugins/sndfile/sndfile.mk
endif
if ENABLE_ESPEAK
include src/plugins/espeak/espeak.mk
endif
if ENABLE_NOTIFICATION_DAEMON
include src/plugins/nd/nd.mk
endif
if ENABLE_NOTIFY
include src/plugins/notify/notify.mk
endif
include src/plugins/relay/relay.mk
include src/libeventc/libeventc.mk
include src/eventc/eventc.mk
if ENABLE_TESTS
include tests/tests.mk
endif
include man/man.mk
include data/units/units.mk


#
# Hooks
#

install-data-hook: la-files-install-hook
uninstall-hook: la-files-uninstall-hook

# *.la files cleanup
la-files-install-hook:
	cd $(DESTDIR)$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES)
	cd $(DESTDIR)$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES)

# Remove shared object files at uninstall since
# we remove *.la files at install
la-files-uninstall-hook:
	cd $(DESTDIR)$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES:.la=$(shrext_cmds))
	cd $(DESTDIR)$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES:.la=$(shrext_cmds))
	rmdir $(DESTDIR)$(pluginsdir)
	rmdir $(DESTDIR)$(pkglibdir)
