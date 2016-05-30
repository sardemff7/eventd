# Service discovery modules

AM_CFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


if ENABLE_DNS_SD
modules_LTLIBRARIES += \
	dns-sd.la \
	$(null)
endif

if ENABLE_SSDP
modules_LTLIBRARIES += \
	ssdp.la \
	$(null)
endif


dns_sd_la_SOURCES = \
	%D%/include/eventd-sd-module.h \
	%D%/src/dns-sd.c \
	$(null)

dns_sd_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

dns_sd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

dns_sd_la_LIBADD = \
	$(AVAHI_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)


ssdp_la_SOURCES = \
	%D%/include/eventd-sd-module.h \
	%D%/src/ssdp.c \
	$(null)

ssdp_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GSSDP_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

ssdp_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

ssdp_la_LIBADD = \
	$(GSSDP_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(NKUTILS_LIBS) \
	$(GLIB_LIBS) \
	$(null)
