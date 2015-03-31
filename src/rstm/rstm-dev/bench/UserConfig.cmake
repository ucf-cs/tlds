#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

include (CMakeDependentOption)

cmake_dependent_option(
  bench_enable_multi_source
  "ON to enable the multiple source build---useful for CXX-tm debugging." OFF
  "rstm_enable_bench" OFF)
mark_as_advanced(bench_enable_multi_source)

cmake_dependent_option(
  bench_enable_single_source
  "ON to enable the single source build." ON
  "rstm_enable_bench" OFF)
mark_as_advanced(bench_enable_single_source)
