mandir = @mandir@/ru
man_MANS = mc.1

pkgdata_DATA = mc.hlp.ru

EXTRA_DIST = xnc.hlp
CLEANFILES = $(pkgdata_DATA)

mc.hlp.ru: mc.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp
	- $(top_builddir)/src/man2hlp_ mc.1 $(srcdir)/xnc.hlp mc.hlp.ru
