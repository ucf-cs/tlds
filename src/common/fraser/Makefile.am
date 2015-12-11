EXTRA_DIST = gc.h intel_defns.h pertable_defns.h ptst.h random.h

noinst_LTLIBRARIES = libfd.la

libfd_la_SOURCES = gc.c\
				   ptst.c

libfd_la_CFLAGS = -O3 -DINTEL -std=c99 $(WARN_FLAGS) \
				  -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`
				  -I$(top_builddir)/src/common/fraser -I$(top_srcdir)/src/common/fraser
