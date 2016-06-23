EXTRA_DIST += \
	%D%/config.ent.in \
	%D%/common-man.xml \
	$(man1_MANS:.1=.xml) \
	$(man5_MANS:.5=.xml) \
	$(null)

CLEANFILES += \
	%D%/config.ent \
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

MAN_GEN_DEPS = \
	$(NKUTILS_MANFILES) \
	%D%/common-man.xml \
	%D%/config.ent \
	$(null)

if EVENTD_USE_GIT_VERSION
MAN_STAMP = %D%/man-version.stamp

MAN_GEN_DEPS += \
	$(MAN_STAMP) \
	$(null)

$(MAN_STAMP):
	@echo '$(EVENTD_VERSION)' > $@

 ifneq ($(shell cat $(MAN_STAMP) 2>/dev/null),$(EVENTD_VERSION))
.PHONY: $(MAN_STAMP)
 endif
endif

$(man1_MANS): %.1: %.xml $(MAN_GEN_DEPS)
	$(MAN_GEN_RULE)

$(man5_MANS): %.5: %.xml $(MAN_GEN_DEPS)
	$(MAN_GEN_RULE)

%D%/config.ent: %D%/config.ent.in $(MAN_STAMP) $(CONFIG_HEADER) %D%/man.mk
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]EVENTD_VERSION[@]:$(EVENTD_VERSION):g' \
		-e 's:[@]EVP_SERVICE_NAME[@]:$(EVP_SERVICE_NAME):g' \
		-e 's:[@]EVP_TRANSPORT_NAME[@]:$(EVP_TRANSPORT_NAME):g' \
		-e 's:[@]EVP_SERVICE_TYPE[@]:$(EVP_SERVICE_TYPE):g' \
		-e 's:[@]EVP_UNIX_SOCKET[@]:$(EVP_UNIX_SOCKET):g' \
		< $< > $@ || rm $@
