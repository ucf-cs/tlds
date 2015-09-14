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
  stamp_use_waiver
  "ON to use Intel's __transaction [[waiver]] extension." ON
  "rstm_enable_itm OR rstm_enable_itm2stm" OFF)