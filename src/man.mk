EXTRA_DIST += \
	src/config.dtd.in \
	$(man1_MANS:.1=.xml) \
	$(man5_MANS:.5=.xml)

CLEANFILES += \
	src/config.dtd \
	$(man1_MANS) \
	$(man5_MANS)

MAN_GEN_RULE = $(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) \
	-o $(dir $@) \
	$(AM_XSLTPROCFLAGS) \
	http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl \
	$<

$(man1_MANS): %.1: %.xml src/config.dtd
	$(MAN_GEN_RULE)

$(man5_MANS): %.5: %.xml src/config.dtd
	$(MAN_GEN_RULE)

src/config.dtd: src/config.dtd.in src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]sysconfdir[@]:$(sysconfdir):g' \
		-e 's:[@]datadir[@]:$(datadir):g' \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]PACKAGE_VERSION[@]:$(PACKAGE_VERSION):g' \
		-e 's:[@]DEFAULT_BIND_PORT[@]:$(DEFAULT_BIND_PORT):g' \
		-e 's:[@]UNIX_SOCKET[@]:$(UNIX_SOCKET):g' \
		< $< > $@ || rm $@
