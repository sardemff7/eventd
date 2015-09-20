# Server control utility

AM_CPPFLAGS += \
	-I $(srcdir)/%D%/include \
	$(null)


bin_PROGRAMS += \
	eventdctl \
	$(null)

dist_man1_MANS += \
	%D%/man/eventdctl.1 \
	$(null)


eventdctl_SOURCES = \
	%D%/include/eventdctl.h \
	%D%/src/eventdctl.c \
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
