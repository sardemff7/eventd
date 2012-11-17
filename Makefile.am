comma := ,
empty :=
space := $(empty) $(empty)

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemduserunitdir="$$dc_install_base/$(systemduserunitdir)" \
	--with-dbussessionservicedir="$$dc_install_base/$(dbussessionservicedir)"

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

pkgconfigdir = $(libdir)/pkgconfig
vapidir = $(datadir)/vala/vapi
pluginsdir = $(pkglibdir)/plugins

bin_PROGRAMS =
libexec_PROGRAMS =
noinst_LTLIBRARIES =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
pkginclude_HEADERS =

TESTS =
XFAIL_TESTS =
check_PROGRAMS =
check_LTLIBRARIES =

man1_MANS =
man5_MANS =
pkgconfig_DATA =
dist_vapi_DATA =
vapi_DATA =
dist_pkgdata_DATA =
systemduserunit_DATA =
dbussessionservice_DATA =

EXTRA_DIST =
CLEANFILES =

XSLTPROC_CONDITIONS = \
	installation

AM_CFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	-I $(srcdir)/src/ \
	-I $(builddir)/src/ \
	--include config.h

AM_VALAFLAGS += \
	--vapidir $(srcdir)/src/ \
	--vapidir $(builddir)/src/ \
	--pkg config

AM_XSLTPROCFLAGS += \
	--path "$(srcdir)/src/" \
	--path "$(builddir)/src/" \
	--stringparam profile.condition '$(subst $(space),;,$(XSLTPROC_CONDITIONS))'

EXTRA_DIST += \
	src/config.vapi

include server/libeventd-event/libeventd-event.mk
include server/libeventd-evp/libeventd-evp.mk
include server/libeventd/libeventd.mk
include server/eventd/eventd.mk
include server/eventdctl/eventdctl.mk
include plugins/evp/evp.mk
include plugins/relay/relay.mk
include plugins/exec/exec.mk
if ENABLE_DBUS
include plugins/dbus/dbus.mk
endif
if ENABLE_SNDFILE
include plugins/sndfile/sndfile.mk
endif
if ENABLE_ESPEAK
include plugins/espeak/espeak.mk
endif
if ENABLE_NOTIFICATION_DAEMON
include plugins/nd/nd.mk
endif
if ENABLE_NOTIFY
include plugins/notify/notify.mk
endif
include client/libeventc/libeventc.mk
include client/eventc/eventc.mk
include server/libeventd-test/libeventd-test.mk
include plugins/test-plugin/test-plugin.mk

include src/man.mk
include src/units.mk
include src/services.mk


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
