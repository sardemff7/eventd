# eventdctl
bin_PROGRAMS += \
	eventdctl

man1_MANS += \
	man/eventdctl.1

eventdctl_SOURCES = \
	src/eventdctl/eventdctl.c

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
