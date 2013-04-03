comma := ,
empty :=
space := $(empty) $(empty)

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemduserunitdir="$$dc_install_base/$(systemduserunitdir)" \
	--with-dbussessionservicedir="$$dc_install_base/$(dbussessionservicedir)"

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

vapidir = $(datadir)/vala/vapi

bin_PROGRAMS =
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
noarch_pkgconfig_DATA =
dist_vapi_DATA =
vapi_DATA =
dist_event_DATA =
systemduserunit_DATA =
dbussessionservice_DATA =
dbuscapabilities_DATA =

EXTRA_DIST =
CLEANFILES =


AM_CPPFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	-I $(srcdir)/src/ \
	-I $(builddir)/src/

AM_XSLTPROCFLAGS += \
	--path "$(srcdir)/src/" \
	--path "$(builddir)/src/"


include server/libeventd-event/libeventd-event.mk
include server/libeventd-evp/libeventd-evp.mk
include server/libeventd/libeventd.mk
include server/eventd/eventd.mk
include server/libeventd-plugin/libeventd-plugin.mk
include server/eventdctl/eventdctl.mk
include plugins/evp/evp.mk
include plugins/dbus/dbus.mk
include plugins/relay/relay.mk
include plugins/exec/exec.mk
include plugins/nd/nd.mk
include plugins/im/im.mk
include plugins/sndfile/sndfile.mk
include plugins/espeak/espeak.mk
include plugins/notify/notify.mk
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
