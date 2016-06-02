dnl
dnl Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a copy
dnl of this software and associated documentation files (the "Software"), to deal
dnl in the Software without restriction, including without limitation the rights
dnl to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
dnl copies of the Software, and to permit persons to whom the Software is
dnl furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included in
dnl all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
dnl OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
dnl THE SOFTWARE.
dnl

dnl EV_CHECK_LIB(PREFIX, headers, search-libs, function, [action-if-found], [action-if-not-found])
AC_DEFUN([EV_CHECK_LIB], [
    ev_save_LIBS=${LIBS}
    m4_ifnblank([$2], [
        AC_CHECK_HEADERS([$2], [], [], [m4_foreach_w([_ev_header], [$2], [
            [#]ifdef AS_TR_CPP([HAVE_]_ev_header)
            [#]include <_ev_header>
            [#]endif
        ])])
        m4_foreach_w([_ev_header], [$2], [
            AS_IF([test x${]AS_TR_SH([ac_cv_header_]_ev_header)[} != xyes], [$6])
        ])
    ])
    AC_SEARCH_LIBS([$4], [$3], [$5], [$6])
    AS_CASE([${ac_cv_search_][$3][}],
        ['none required'], [$5],
        [no], [],
        [$1][_LIBS=${ac_cv_search_][$4][}]
    )
    AC_SUBST([$1][_LIBS])
    LIBS=${ev_save_LIBS}
])

AC_DEFUN([_EV_CHECK_FEATURE], [
    AC_ARG_ENABLE([$1], AS_HELP_STRING([--enable-][$1], [Enable ][$2]), [], [$3][=auto])
    AS_IF([test x${[$3]} != xno], [
        PKG_CHECK_MODULES([$5], [$6], [$3][=yes], [
            AS_IF([test x${[$3]} = xyes], [
                AC_MSG_ERROR(
[$2 requested, but:
${][$5][_PKG_ERRORS}]
                )
            ], [$3][=no])
        ])
        AC_DEFINE([$4], [1], [$2])
        AM_DOCBOOK_CONDITIONS="${AM_DOCBOOK_CONDITIONS};[$3]"
    ])
    AM_CONDITIONAL([$4], [test x${[$3]} = xyes])
])

AC_DEFUN([EV_CHECK_FEATURE], [_EV_CHECK_FEATURE([$1], [$2], [enable_]AS_TR_SH([$1]), [ENABLE_]AS_TR_CPP([$1]), [$3], [$4])])
