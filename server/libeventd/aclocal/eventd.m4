AC_DEFUN([EVENTD_I18N], [
AC_REQUIRE([AC_PROG_MKDIR_P])dnl
AC_REQUIRE([IT_PROG_INTLTOOL])dnl
EVENTD_EVENT_RULE='%.event: %.event.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(INTLTOOL_V_MERGE) $(MKDIR_P) $(dir [$]@) && LC_ALL=C $(INTLTOOL_MERGE) $(INTLTOOL_V_MERGE_OPTIONS) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
EVENTD_ACTION_RULE='%.action: %.action.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(INTLTOOL_V_MERGE) $(MKDIR_P) $(dir [$]@) && LC_ALL=C $(INTLTOOL_MERGE) $(INTLTOOL_V_MERGE_OPTIONS) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([EVENTD_EVENT_RULE])
AC_SUBST([EVENTD_ACTION_RULE])
AM_SUBST_NOTMAKE([EVENTD_EVENT_RULE])
])
