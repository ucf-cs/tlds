rstm_srcdir = $(top_srcdir)/src/rstm/rstm-dev
rstm_builddir = $(top_builddir)/src/rstm/rstm-compile

SUBDIRS = common/fraser boosting/skiplist

EXTRA_DIST = rstm/list/RSTMList.hpp

bin_PROGRAMS = trans

trans_SOURCES = bench/main.cc \
				common/allocator.cc\
				common/timehelper.cc\
				common/assert.cc\
				ostm/skiplist/stmskip.cc\
				translink/list/translist.cc\
				translink/skiplist/transskip.cc\
				boosting/list/boostinglist.cc\
				boosting/list/lockfreelist.cc\
				boosting/skiplist/boostingskip.cc\
				boosting/lockkey.cc\
				translink/map/transmap.cc

trans_CPPFLAGS = $(WARN_FLAGS) -std=c++11 -pthread\
				-DINTEL -DCACHE_LINE_SIZE=`getconf LEVEL1_DCACHE_LINESIZE`\
				 -I$(top_builddir)/src -I$(top_srcdir)/src\
				 -I$(rstm_builddir)/include -I$(rstm_srcdir)/include -I$(rstm_srcdir)

trans_LDADD = $(BOOST_LDFLAGS) -L$(rstm_builddir)/libstm -lstm64 common/fraser/libfd.la boosting/skiplist/libbsl.la
