EXTRA_DIST = transskipimpl.h

noinst_LTLIBRARIES = libtsl.la 

libtsl_la_SOURCES = transskipimpl.c

libtsl_la_CFLAGS = -O3 -DINTEL -std=c99 $(WARN_FLAGS) \
				  -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`\
				  -I$(top_builddir)/src -I$(top_srcdir)/src
