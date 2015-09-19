# Linux framebuffer backend

if ENABLE_LINUX_FB
ndbackends_LTLIBRARIES += \
	linux.la \
	$(null)
endif


linux_la_SOURCES = \
	%D%/linux.c \
	$(null)

linux_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

linux_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

linux_la_LIBADD = \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS) \
	$(null)
