rstm_srcdir = /home/deli/dev/parallel/rstm-dev
rstm_builddir = /home/deli/dev/parallel/rstm-build

SUBDIRS =

EXTRA_DIST = rstm/list/RSTMList.hpp

bin_PROGRAMS = trans

trans_SOURCES = bench/main.cc \
			   common/timehelper.cc\
			   common/assert.cc\
			   translink/list/translist.cc

trans_CPPFLAGS = $(WARN_FLAGS) -std=c++11\
				-I$(top_builddir)/src -I$(top_srcdir)/src\
				-I$(rstm_builddir)/include -I$(rstm_srcdir)/include -I$(rstm_srcdir)

trans_LDADD = $(BOOST_LDFLAGS) -L$(rstm_builddir)/libstm -lstm64
