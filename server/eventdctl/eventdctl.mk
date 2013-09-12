# Server control utility

AM_CPPFLAGS += \
	-I $(srcdir)/server/eventdctl/include \
	$(null)


bin_PROGRAMS += \
	eventdctl \
	$(null)

man1_MANS += \
	server/eventdctl/man/eventdctl.1 \
	$(null)


eventdctl_SOURCES = \
	server/eventdctl/include/eventdctl.h \
	server/eventdctl/src/eventdctl.c \
	$(null)

eventdctl_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

eventdctl_LDADD = \
	$(GIO_LIBS) \
	$(GLIB_LIBS) \
	$(null)
