# eventdctl
if ENABLE_EVENTDCTL
bin_PROGRAMS += \
	eventdctl

eventdctl_SOURCES = \
	src/eventdctl/eventdctl.c

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
endif
