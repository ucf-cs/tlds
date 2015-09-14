#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

# Configures the itm2stm library build.
include (AppendFlags)

# Add some additional compiler flags.
append_cxx_flags(-fno-rtti -fno-exceptions -fno-strict-aliasing
  -Wno-invalid-offsetof -Wno-unused-function)

if (CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_NAME MATCHES "Darwin")
  append_cxx_flags(-Wno-unknown-pragmas)
endif ()

set(cppsources
  BlockOperations.cpp
  Extras.cpp
  Scope.cpp
  Transaction.cpp
  libitm-5.1,5.cpp
  libitm-5.2.cpp
  libitm-5.3.cpp
  libitm-5.4.cpp
  libitm-5.7.cpp
  libitm-5.8.cpp
  libitm-5.9.cpp
  libitm-5.10.cpp
  libitm-5.11.cpp
  libitm-5.12.cpp
  libitm-5.13,14.cpp
  libitm-5.15.cpp
  libitm-5.16.cpp
  libitm-5.17.cpp)

set(asmsources
  arch/x86/_ITM_beginTransaction.S
  arch/x86/checkpoint_restore.S
  arch/x86_64/_ITM_beginTransaction.S
  arch/x86_64/checkpoint_restore.S)

# Hack because preprocessed asm files aren't supported in cmake-2.8.4, so we
# pretend they're C++ files. They also need the additional include directory.
set_source_files_properties(
  ${asmsources} PROPERTIES
  LANGUAGE CXX
  COMPILE_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR}/arch)

if (rstm_build_32-bit)
  set(arch_path ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86)
  add_library(itm2stm32
    ${cppsources}
    ${arch_path}/_ITM_beginTransaction.S
    ${arch_path}/checkpoint_restore.S)
  append_property(TARGET itm2stm32 COMPILE_FLAGS -m32 -I${arch_path})
  target_link_libraries(itm2stm32 stm32)
endif ()

if (rstm_build_64-bit)
  set(arch_path ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86_64)
  add_library(itm2stm64
    ${cppsources}
    ${arch_path}/_ITM_beginTransaction.S
    ${arch_path}/checkpoint_restore.S)
  append_property(TARGET itm2stm64 COMPILE_FLAGS -m64 -I${arch_path})
  target_link_libraries(itm2stm64 stm64)
endif ()
