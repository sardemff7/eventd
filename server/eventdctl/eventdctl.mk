# Server control utility

AM_CPPFLAGS += \
	-I $(srcdir)/server/eventdctl/include


bin_PROGRAMS += \
	eventdctl

man1_MANS += \
	server/eventdctl/man/eventdctl.1


eventdctl_SOURCES = \
	server/eventdctl/include/eventdctl.h \
	server/eventdctl/src/eventdctl.c

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
