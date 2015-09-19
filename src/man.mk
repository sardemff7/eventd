EXTRA_DIST += \
	src/config.ent.in \
	src/common-man.xml \
	$(man1_MANS:.1=.xml) \
	$(man5_MANS:.5=.xml) \
	$(null)

CLEANFILES += \
	src/config.ent \
	$(man1_MANS) \
	$(man5_MANS) \
	$(null)

EV_V_MAN = $(ev__v_man_@AM_V@)
ev__v_man_ = $(ev__v_man_@AM_DEFAULT_V@)
ev__v_man_0 = @echo "  XSLT    " $@;

MAN_GEN_RULE = $(EV_V_MAN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) \
	-o $(dir $@) \
	$(AM_XSLTPROCFLAGS) \
	--stringparam profile.condition '${AM_DOCBOOK_CONDITIONS}' \
	$(XSLTPROCFLAGS) \
	http://docbook.sourceforge.net/release/xsl/current/manpages/profile-docbook.xsl \
	$<

$(man1_MANS): %.1: %.xml $(NKUTILS_MANFILES) src/common-man.xml src/config.ent
	$(MAN_GEN_RULE)

$(man5_MANS): %.5: %.xml $(NKUTILS_MANFILES) src/common-man.xml src/config.ent
	$(MAN_GEN_RULE)

src/config.ent: src/config.ent.in $(CONFIG_HEADER) src/man.mk
	$(EV_V_MAN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]sysconfdir[@]:$(sysconfdir):g' \
		-e 's:[@]libdir[@]:$(libdir):g' \
		-e 's:[@]datadir[@]:$(datadir):g' \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]PACKAGE_VERSION[@]:$(PACKAGE_VERSION):g' \
		-e 's:[@]EVP_SERVICE_NAME[@]:$(EVP_SERVICE_NAME):g' \
		-e 's:[@]EVP_TRANSPORT_NAME[@]:$(EVP_TRANSPORT_NAME):g' \
		-e 's:[@]EVP_SERVICE_TYPE[@]:$(EVP_SERVICE_TYPE):g' \
		-e 's:[@]EVP_UNIX_SOCKET[@]:$(EVP_UNIX_SOCKET):g' \
		< $< > $@ || rm $@
