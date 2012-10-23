# Server control utility

bin_PROGRAMS += \
	eventdctl

man1_MANS += \
	server/eventdctl/man/eventdctl.1


eventdctl_SOURCES = \
	server/eventdctl/src/eventdctl.c

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
