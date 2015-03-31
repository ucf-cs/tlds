SUBDIRS =

EXTRA_DIST =

bin_PROGRAMS = trans

trans_SOURCES = bench/main.cc \
			   common/timehelper.cc\
			   common/assert.cc\
			   translink/list/translist.cc


trans_CPPFLAGS = $(WARN_FLAGS) -std=c++11\
				-I$(top_builddir)/src -I$(top_srcdir)/src

trans_LDADD = $(BOOST_LDFLAGS)
