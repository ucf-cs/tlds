#
#  Copyright (C) 2011
#  University of Rochester Department of Computer Science
#    and
#  Lehigh University Department of Computer Science and Engineering
# 
# License: Modified BSD
#          Please see the file LICENSE.RSTM for licensing information

##
# Translate the RSTM configuration settings stored in the cache into the
# expected configuration settings. This step is necessary since there are
# many user-defined options for configuring RSTM

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|i.86)")
  set(STM_CPU_X86 1)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "sparc")
  set(STM_CPU_SPARC 1)
else ()
  message(SEND_ERROR "Unexpected system processor ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (CMAKE_COMPILER_IS_GNUCXX)
  set(STM_CC_GCC 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "llvm")
  set(STM_CC_LLVM 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "sun")
  set(STM_CC_SUN 1)
else ()
  message(SEND_ERROR "Unexcpected C++ compiler ${CMAKE_CXX_COMPILER}")
endif ()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(STM_OS_LINUX 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
  set(STM_OS_SOLARIS 1)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(STM_OS_MACOS 1)
else ()
  message(SEND_ERROR "Unexpected system ${CMAKE_SYSTEM_NAME}")
endif ()

if (CMAKE_BUILD_TYPE MATCHES "(Release|RelWithDebInfo)")
  set(STM_O3 1)
else ()
  set(STM_O0 1)
endif ()

# Configure histogram
if (libstm_enable_abort_histogram)
  set(STM_COUNTCONSEC_YES 1)
endif ()

# Configure ProfileTMtrigger
if (libstm_adaptation_points MATCHES "all")
  set(STM_PROFILETMTRIGGER_ALL 1)
elseif (libstm_adaptation_points MATCHES "none")
  set(STM_PROFILETMTRIGGER_NONE 1)
else ()
  set(STM_PROFILETMTRIGGER_PATHOLOGY 1)
endif ()

# Configure TLS
if (libstm_use_pthread_tls)
  set(STM_TLS_PTHREAD 1)
else ()
  set(STM_TLS_GCC 1)
endif ()

# Configure logging
if (libstm_enable_byte_logging)
  set(STM_WS_BYTELOG 1)
else ()
  set(STM_WS_WORDLOG 1)
endif ()

if (libstm_enable_norec_false_conflicts)
  set(STM_USE_WORD_LOGGING_VALUELIST 1)
endif ()

# Configure stack protection.
if (libstm_enable_stack_protection)
  set(STM_PROTECT_STACK 1)
endif ()

# Configure cancel and throw.
if (libstm_enable_cancel_and_throw)
  set(STM_ABORT_ON_THROW 1)
endif ()

# Configure sse
if (libstm_use_sse)
  set(STM_USE_SSE 1)
endif ()

configure_file (config.h.cmake config.h)
