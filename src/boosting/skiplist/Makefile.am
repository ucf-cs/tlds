EXTRA_DIST = lockfreeskip.h

noinst_LTLIBRARIES = libbsl.la 

libbsl_la_SOURCES = lockfreeskip.c

libbsl_la_CFLAGS = -O3 -DINTEL -std=c99 $(WARN_FLAGS) \
				  -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`\
				  -I$(top_builddir)/src -I$(top_srcdir)/src
