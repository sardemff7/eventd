comma := ,
empty :=
space := $(empty) $(empty)

if EVENTD_USE_GIT_VERSION
# First, check if we have an exact match for a tag
EVENTD_GIT_EXACT_VERSION := $(shell git describe --tags --match 'v*' --exact-match --dirty 2>/dev/null | sed -e 's:^v::')
EVENTD_VERSION := $(if $(EVENTD_GIT_EXACT_VERSION),$(EVENTD_GIT_EXACT_VERSION),$(VERSION) - $(shell git describe --tags --always --dirty) ($(shell git describe --tags --always --all | sed -e 's:heads/::')))
else # ! EVENTD_USE_GIT_VERSION
EVENTD_VERSION = $(VERSION)
endif # ! EVENTD_USE_GIT_VERSION

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--with-systemduserunitdir="$$dc_install_base/$(systemduserunitdir)" \
	--with-systemdsystemunitdir="$$dc_install_base/$(systemdsystemunitdir)" \
	--with-dbussessionservicedir="$$dc_install_base/$(dbussessionservicedir)" \
	--with-girdir="$$dc_install_base/$(girdir)" \
	--with-typelibdir="$$dc_install_base/$(typelibdir)" \
	$(null)

ACLOCAL_AMFLAGS = -I m4 -I src/libnkutils -I src/libgwater ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

bin_PROGRAMS =
noinst_LTLIBRARIES =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
modules_LTLIBRARIES =
plugins_LTLIBRARIES =
pkginclude_HEADERS =

TESTS =
XFAIL_TESTS =
check_PROGRAMS =
check_LTLIBRARIES =
dist_check_DATA =

man1_MANS =
man5_MANS =
pkgconfig_DATA =
noarch_pkgconfig_DATA =
dist_aclocal_DATA =
dist_vapi_DATA =
vapi_DATA =
dist_event_DATA =
systemduserunit_DATA =
systemdsystemunit_DATA =
dbussessionservice_DATA =

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(builddir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

EXTRA_DIST = \
	autogen.sh \
	README.md \
	PROTOCOL \
	COPYING.LGPLv3 \
	$(null)
CLEANFILES =


AM_CPPFLAGS += \
	-I $(srcdir)/src/ \
	-I $(builddir)/src/ \
	-D EVENTD_VERSION='"$(EVENTD_VERSION)"' \
	-D G_LOG_DOMAIN=\"$(word 2,$(subst /,$(space),$(patsubst $(srcdir)/%,%,$<)\))\" \
	$(null)

AM_CFLAGS += \
	-I $(srcdir)/src/ \
	-I $(builddir)/src/ \
	$(null)

AM_XSLTPROCFLAGS += \
	--path "$(srcdir)/src/" \
	--path "$(builddir)/src/" \
	$(null)

PLUGIN_LDFLAGS = \
	-avoid-version \
	-module \
	-shared \
	$(null)

include server/libeventd/libeventd.mk
include server/libeventd-helpers/libeventd-helpers.mk
include server/eventd/eventd.mk
include server/modules/modules.mk
include server/libeventd-plugin/libeventd-plugin.mk
include server/eventdctl/eventdctl.mk
include plugins/fdo-notifications/fdo-notifications.mk
include plugins/exec/exec.mk
include plugins/file/file.mk
include plugins/nd/nd.mk
include plugins/im/im.mk
include plugins/sound/sound.mk
include plugins/tts/tts.mk
include plugins/notify/notify.mk
include plugins/canberra/canberra.mk
include client/libeventc/libeventc.mk
include client/libeventc-light/libeventc-light.mk
include client/eventc/eventc.mk
include server/libeventd-test/libeventd-test.mk
include plugins/test-plugin/test-plugin.mk

include src/man.mk
include src/introspection.mk
include src/units.mk
include src/services.mk

include src/libnkutils/libnkutils.mk

-include local-rules.mk

#
# Hooks
#

install-data-hook: la-files-install-hook
uninstall-libLTLIBRARIES uninstall-hook: la-files-uninstall-hook

ev_remove_la = $(if $(strip $($(1)_LTLIBRARIES)),( cd $(DESTDIR)$($(1)dir) && rm -f $($(1)_LTLIBRARIES) ))
ev_remove_so_from_la = $(if $(strip $($(1)_LTLIBRARIES)),for la in $($(1)_LTLIBRARIES); do \
		$(INSTALL) "$(abs_builddir)/$(LT_OBJDIR)"/$${la}i "$(DESTDIR)$($(1)dir)"/$${la}; \
	done )

# *.la files cleanup
la-files-install-hook:
	$(call ev_remove_la,lib)
	$(call ev_remove_la,pkglib)
	$(call ev_remove_la,plugins)

# Remove shared object files at uninstall since
# we remove *.la files at install
la-files-uninstall-hook:
	$(call ev_remove_so_from_la,lib)
	$(call ev_remove_so_from_la,pkglib)
	$(call ev_remove_so_from_la,plugins)
