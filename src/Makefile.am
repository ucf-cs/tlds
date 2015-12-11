rstm_srcdir = $(top_srcdir)/src/rstm/rstm-dev
rstm_builddir = $(top_builddir)/src/rstm/rstm-compile

SUBDIRS = common/fraser translink/skiplist boosting/skiplist

EXTRA_DIST = rstm/list/RSTMList.hpp

bin_PROGRAMS = trans

trans_SOURCES = bench/main.cc \
				common/allocator.cc\
				common/timehelper.cc\
				common/assert.cc\
				translink/list/translist.cc\
				boosting/list/boostinglist.cc\
				boosting/list/lockfreelist.cc\
				boosting/lockkey.cc

trans_CPPFLAGS = $(WARN_FLAGS) -std=c++11\
				-DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`\
				 -I$(top_builddir)/src -I$(top_srcdir)/src\
				 -I$(rstm_builddir)/include -I$(rstm_srcdir)/include -I$(rstm_srcdir)

trans_LDADD = $(BOOST_LDFLAGS) -L$(rstm_builddir)/libstm -lstm64 common/fraser/libfd.la translink/skiplist/libtsl.la boosting/skiplist/libbsl.la
