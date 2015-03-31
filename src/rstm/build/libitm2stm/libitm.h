/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

// -----------------------------------------------------------------------------
// Our definitions of the Intel TM ABI as described in
// docs/Intel+TM+ABI+1_0_1.pdf, available from
// software.intel.com/en-us/articles/intel-c-stm-compiler-prototype-edition/.
// -----------------------------------------------------------------------------
#ifndef STM_ITM2STM_LIBITM_H
#define STM_ITM2STM_LIBITM_H

#include <itm/itm.h> // 5.1 is public

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>     // uint32_t
#include <stdbool.h>    // bool for tryCommitTransaction
#ifndef __AVX__
#    include "alt-license/avxintrin_emu.h"
#else
#    include <immintrin.h>  // sse-specific type __m256, __m128, __m64
#endif

// -----------------------------------------------------------------------------
// 4  Types and macro list -----------------------------------------------------
// -----------------------------------------------------------------------------
#define _ITM_VERSION            "0.9 (October 1, 2008)"
#define _ITM_VERSION_NO         90
#define _ITM_NoTransactionId    0

#define _ITM_NORETURN NORETURN

/* Values used as arguments to abort. */
typedef enum {
    userAbort           = 1,
    userRetry           = 2,
    TMConflict          = 4,
    exceptionBlockAbort = 8
} _ITM_abortReason;

/* Arguments to changeTransactionMode */
typedef enum {
    modeSerialIrrevocable
} _ITM_transactionState;

/* Results from inTransaction */
typedef enum {
    outsideTransaction = 0,    /* So "if (inTransaction(td))" works */
    inRetryableTransaction,
    inIrrevocableTransaction
} _ITM_howExecuting;

/* Values to describe properties of code, passed in to startTransaction */
typedef enum {
    pr_instrumentedCode     = 0x0001,
    pr_uninstrumentedCode   = 0x0002,
    pr_multiwayCode         = pr_instrumentedCode | pr_uninstrumentedCode,
    pr_hasNoXMMUpdate       = 0x0004,
    pr_hasNoAbort           = 0x0008,
    pr_hasNoRetry           = 0x0010,
    pr_hasNoIrrevocable     = 0x0020,
    pr_doesGoIrrevocable    = 0x0040,
    pr_hasNoSimpleReads     = 0x0080,
    pr_aWBarriersOmitted    = 0x0100,
    pr_RaRBarriersOmitted   = 0x0200,
    pr_undoLogCode          = 0x0400,
    pr_preferUninstrumented = 0x0800,
    pr_exceptionBlock       = 0x1000,
    pr_hasElse              = 0x2000
} _ITM_codeProperties;

/* Result from startTransaction that describes what actions to take.  */
typedef enum {
    a_runInstrumentedCode       = 0x01,
    a_runUninstrumentedCode     = 0x02,
    a_saveLiveVariables         = 0x04,
    a_restoreLiveVariables      = 0x08,
    a_abortTransaction          = 0x10
} _ITM_actions;

typedef struct {
    uint32_t reserved_1;
    uint32_t flags;
    uint32_t reserved_2;
    uint32_t reserved_3;
    const char* psource;
} _ITM_srcLocation;

typedef struct _ITM_transaction _ITM_transaction;
typedef uint32_t _ITM_transactionId_t;
typedef void (*_ITM_userUndoFunction)(void*);
typedef void (*_ITM_userCommitFunction)(void*);

// -----------------------------------------------------------------------------
// 5.2  Version checking -------------------------------------------------------
// -----------------------------------------------------------------------------
int _ITM_FASTCALL _ITM_versionCompatible(int);
const char* _ITM_FASTCALL _ITM_libraryVersion(void);

// -----------------------------------------------------------------------------
// 5.3  Error reporting --------------------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_NORETURN _ITM_error(const _ITM_srcLocation*, int errorCode);

// -----------------------------------------------------------------------------
// 5.4  inTransaction call -----------------------------------------------------
// -----------------------------------------------------------------------------
_ITM_howExecuting _ITM_FASTCALL _ITM_inTransaction(_ITM_transaction*);

// -----------------------------------------------------------------------------
// 5.5  State manipulation functions -------------------------------------------
// -----------------------------------------------------------------------------
_ITM_transaction* _ITM_FASTCALL _ITM_getTransaction(void);
_ITM_transactionId_t _ITM_FASTCALL _ITM_getTransactionId(_ITM_transaction*);

// -----------------------------------------------------------------------------
// 5.6  Source locations -------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 5.7  Starting a transaction -------------------------------------------------
// -----------------------------------------------------------------------------
uint32_t _ITM_FASTCALL _ITM_beginTransaction(_ITM_transaction*, uint32_t, _ITM_srcLocation*);

// -----------------------------------------------------------------------------
// 5.8  Aborting a transaction -------------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_NORETURN _ITM_abortTransaction(_ITM_transaction*, _ITM_abortReason, const _ITM_srcLocation*);
void _ITM_FASTCALL _ITM_rollbackTransaction(_ITM_transaction*, const _ITM_srcLocation*);

// -----------------------------------------------------------------------------
// 5.9  Committing a transaction -----------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_commitTransaction(_ITM_transaction*, const _ITM_srcLocation*);
bool _ITM_FASTCALL _ITM_tryCommitTransaction(_ITM_transaction*, const _ITM_srcLocation*);
void _ITM_FASTCALL _ITM_commitTransactionToId(_ITM_transaction*, const _ITM_transactionId_t, const _ITM_srcLocation*);

// -----------------------------------------------------------------------------
// 5.10  Exception handling support --------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_registerThrownObject(_ITM_transaction*, const void*, size_t);

// -----------------------------------------------------------------------------
// 5.11  Transition to serial irrevocable mode ---------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_changeTransactionMode(_ITM_transaction*, _ITM_transactionState, const _ITM_srcLocation*);

// -----------------------------------------------------------------------------
// 5.12  Data transfer functions -----------------------------------------------
// -----------------------------------------------------------------------------
uint8_t _ITM_FASTCALL _ITM_RU1(_ITM_transaction*, const uint8_t*);
uint8_t _ITM_FASTCALL _ITM_RaRU1(_ITM_transaction*, const uint8_t*);
uint8_t _ITM_FASTCALL _ITM_RaWU1(_ITM_transaction*, const uint8_t*);
uint8_t _ITM_FASTCALL _ITM_RfWU1(_ITM_transaction*, const uint8_t*);

void _ITM_FASTCALL _ITM_WU1(_ITM_transaction*, uint8_t*, uint8_t);
void _ITM_FASTCALL _ITM_WaRU1(_ITM_transaction*, uint8_t*, uint8_t);
void _ITM_FASTCALL _ITM_WaWU1(_ITM_transaction*, uint8_t*, uint8_t);

uint16_t _ITM_FASTCALL _ITM_RU2(_ITM_transaction*, const uint16_t*);
uint16_t _ITM_FASTCALL _ITM_RaRU2(_ITM_transaction*, const uint16_t*);
uint16_t _ITM_FASTCALL _ITM_RaWU2(_ITM_transaction*, const uint16_t*);
uint16_t _ITM_FASTCALL _ITM_RfWU2 (_ITM_transaction*, const uint16_t*);

void _ITM_FASTCALL _ITM_WU2(_ITM_transaction*, uint16_t*, uint16_t);
void _ITM_FASTCALL _ITM_WaRU2(_ITM_transaction*, uint16_t*, uint16_t);
void _ITM_FASTCALL _ITM_WaWU2(_ITM_transaction*, uint16_t*, uint16_t);

uint32_t _ITM_FASTCALL _ITM_RU4(_ITM_transaction*, const uint32_t*);
uint32_t _ITM_FASTCALL _ITM_RaRU4(_ITM_transaction*, const uint32_t*);
uint32_t _ITM_FASTCALL _ITM_RaWU4(_ITM_transaction*, const uint32_t*);
uint32_t _ITM_FASTCALL _ITM_RfWU4(_ITM_transaction*, const uint32_t*);

void _ITM_FASTCALL _ITM_WU4(_ITM_transaction*, uint32_t*, uint32_t);
void _ITM_FASTCALL _ITM_WaRU4(_ITM_transaction*, uint32_t*, uint32_t);
void _ITM_FASTCALL _ITM_WaWU4(_ITM_transaction*, uint32_t*, uint32_t);

uint64_t _ITM_FASTCALL _ITM_RU8(_ITM_transaction*, const uint64_t*);
uint64_t _ITM_FASTCALL _ITM_RaRU8(_ITM_transaction*, const uint64_t*);
uint64_t _ITM_FASTCALL _ITM_RaWU8(_ITM_transaction*, const uint64_t*);
uint64_t _ITM_FASTCALL _ITM_RfWU8(_ITM_transaction*, const uint64_t*);

void _ITM_FASTCALL _ITM_WU8(_ITM_transaction*, uint64_t*, uint64_t);
void _ITM_FASTCALL _ITM_WaRU8(_ITM_transaction*, uint64_t*, uint64_t);
void _ITM_FASTCALL _ITM_WaWU8(_ITM_transaction*, uint64_t*, uint64_t);

float _ITM_FASTCALL _ITM_RF(_ITM_transaction*, const float*);
float _ITM_FASTCALL _ITM_RaRF(_ITM_transaction*, const float*);
float _ITM_FASTCALL _ITM_RaWF(_ITM_transaction*, const float*);
float _ITM_FASTCALL _ITM_RfWF (_ITM_transaction*, const float*);

void _ITM_FASTCALL _ITM_WF(_ITM_transaction*, float*, float);
void _ITM_FASTCALL _ITM_WaRF(_ITM_transaction*, float*, float);
void _ITM_FASTCALL _ITM_WaWF(_ITM_transaction*, float*, float);

double _ITM_FASTCALL _ITM_RD(_ITM_transaction*, const double*);
double _ITM_FASTCALL _ITM_RaRD(_ITM_transaction*, const double*);
double _ITM_FASTCALL _ITM_RaWD (_ITM_transaction*, const double*);
double _ITM_FASTCALL _ITM_RfWD (_ITM_transaction*, const double*);

void _ITM_FASTCALL _ITM_WD(_ITM_transaction*, double*, double);
void _ITM_FASTCALL _ITM_WaRD(_ITM_transaction*, double*, double);
void _ITM_FASTCALL _ITM_WaWD(_ITM_transaction*, double*, double);

long double _ITM_FASTCALL _ITM_RE(_ITM_transaction*, const long double*);
long double _ITM_FASTCALL _ITM_RaRE(_ITM_transaction*, const long double*);
long double _ITM_FASTCALL _ITM_RaWE(_ITM_transaction*, const long double*);
long double _ITM_FASTCALL _ITM_RfWE(_ITM_transaction*, const long double*);

void _ITM_FASTCALL _ITM_WE(_ITM_transaction*, long double*, long double);
void _ITM_FASTCALL _ITM_WaRE(_ITM_transaction*, long double*, long double);
void _ITM_FASTCALL _ITM_WaWE(_ITM_transaction*, long double*, long double);

__m64 _ITM_FASTCALL _ITM_RM64(_ITM_transaction*, const __m64*);
__m64 _ITM_FASTCALL _ITM_RaRM64(_ITM_transaction*, const __m64*);
__m64 _ITM_FASTCALL _ITM_RaWM64(_ITM_transaction*, const __m64*);
__m64 _ITM_FASTCALL _ITM_RfWM64(_ITM_transaction*, const __m64*);

void _ITM_FASTCALL _ITM_WM64(_ITM_transaction*, __m64*, __m64);
void _ITM_FASTCALL _ITM_WaRM64(_ITM_transaction*, __m64*, __m64);
void _ITM_FASTCALL _ITM_WaWM64(_ITM_transaction*, __m64*, __m64);

__m128 _ITM_FASTCALL _ITM_RM128(_ITM_transaction*, const __m128*);
__m128 _ITM_FASTCALL _ITM_RaRM128(_ITM_transaction*, const __m128*);
__m128 _ITM_FASTCALL _ITM_RaWM128(_ITM_transaction*, const __m128*);
__m128 _ITM_FASTCALL _ITM_RfWM128(_ITM_transaction*, const __m128*);

void _ITM_FASTCALL _ITM_WM128(_ITM_transaction*, __m128*, __m128);
void _ITM_FASTCALL _ITM_WaRM128(_ITM_transaction*, __m128*, __m128);
void _ITM_FASTCALL _ITM_WaWM128(_ITM_transaction*, __m128*, __m128);

__m256 _ITM_FASTCALL _ITM_RM256(_ITM_transaction*, const __m256*);
__m256 _ITM_FASTCALL _ITM_RaRM256(_ITM_transaction*, const __m256*);
__m256 _ITM_FASTCALL _ITM_RaWM256(_ITM_transaction*, const __m256*);
__m256 _ITM_FASTCALL _ITM_RfWM256(_ITM_transaction*, const __m256*);

void _ITM_FASTCALL _ITM_WM256(_ITM_transaction*, __m256*, __m256);
void _ITM_FASTCALL _ITM_WaRM256(_ITM_transaction*, __m256*, __m256);
void _ITM_FASTCALL _ITM_WaWM256(_ITM_transaction*, __m256*, __m256);

_Complex float _ITM_FASTCALL _ITM_RCF(_ITM_transaction*, const _Complex float*);
_Complex float _ITM_FASTCALL _ITM_RaRCF(_ITM_transaction*, const _Complex float*);
_Complex float _ITM_FASTCALL _ITM_RaWCF(_ITM_transaction*, const _Complex float*);
_Complex float _ITM_FASTCALL _ITM_RfWCF(_ITM_transaction*, const _Complex float*);

void _ITM_FASTCALL _ITM_WCF(_ITM_transaction*, _Complex float*, _Complex float);
void _ITM_FASTCALL _ITM_WaRCF(_ITM_transaction*, _Complex float*, _Complex float);
void _ITM_FASTCALL _ITM_WaWCF(_ITM_transaction*, _Complex float*, _Complex float);

_Complex double _ITM_FASTCALL _ITM_RCD(_ITM_transaction*, const _Complex double*);
_Complex double _ITM_FASTCALL _ITM_RaRCD(_ITM_transaction*, const _Complex double*);
_Complex double _ITM_FASTCALL _ITM_RaWCD(_ITM_transaction*, const _Complex double*);
_Complex double _ITM_FASTCALL _ITM_RfWCD(_ITM_transaction*, const _Complex double*);

void _ITM_FASTCALL _ITM_WCD(_ITM_transaction*, _Complex double*, _Complex double);
void _ITM_FASTCALL _ITM_WaRCD(_ITM_transaction*, _Complex double*, _Complex double);
void _ITM_FASTCALL _ITM_WaWCD(_ITM_transaction*, _Complex double*, _Complex double);

_Complex long double _ITM_FASTCALL _ITM_RCE(_ITM_transaction*, const _Complex long double*);
_Complex long double _ITM_FASTCALL _ITM_RaRCE(_ITM_transaction*, const _Complex long double*);
_Complex long double _ITM_FASTCALL _ITM_RaWCE(_ITM_transaction*, const _Complex long double*);
_Complex long double _ITM_FASTCALL _ITM_RfWCE(_ITM_transaction*, const _Complex long double*);

void _ITM_FASTCALL _ITM_WCE(_ITM_transaction*, _Complex long double*, _Complex long double);
void _ITM_FASTCALL _ITM_WaRCE(_ITM_transaction*, _Complex long double*, _Complex long double);
void _ITM_FASTCALL _ITM_WaWCE(_ITM_transaction*, _Complex long double*, _Complex long double);

// -----------------------------------------------------------------------------
// 5.13  Transactional memory copies -------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_memcpyRnWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRnWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRnWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaRWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaRWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaRWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaRWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaWWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaWWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaWWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memcpyRtaWWtaW(_ITM_transaction*, void*, const void*, size_t);

// -----------------------------------------------------------------------------
// 5.14  Transactional versions of memmove -------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_memmoveRnWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRnWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRnWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaRWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaRWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaRWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaRWtaW(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaWWn(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaWWt(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaWWtaR(_ITM_transaction*, void*, const void*, size_t);
void _ITM_FASTCALL _ITM_memmoveRtaWWtaW(_ITM_transaction*, void*, const void*, size_t);

// -----------------------------------------------------------------------------
// 5.15  Transactional versions of memset --------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_memsetW(_ITM_transaction*, void*, int, size_t);
void _ITM_FASTCALL _ITM_memsetWaR(_ITM_transaction*, void*, int, size_t);
void _ITM_FASTCALL _ITM_memsetWaW(_ITM_transaction*, void*, int, size_t);

// -----------------------------------------------------------------------------
// 5.16  Logging functions -----------------------------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_LU1(_ITM_transaction*, const uint8_t*);
void _ITM_FASTCALL _ITM_LU2(_ITM_transaction*, const uint16_t*);
void _ITM_FASTCALL _ITM_LU4(_ITM_transaction*, const uint32_t*);
void _ITM_FASTCALL _ITM_LU8(_ITM_transaction*, const uint64_t*);
void _ITM_FASTCALL _ITM_LF(_ITM_transaction*, const float*);
void _ITM_FASTCALL _ITM_LD(_ITM_transaction*, const double*);
void _ITM_FASTCALL _ITM_LE(_ITM_transaction*, const long double*);
void _ITM_FASTCALL _ITM_LM64(_ITM_transaction*, const __m64*);
void _ITM_FASTCALL _ITM_LM128(_ITM_transaction*, const __m128*);
void _ITM_FASTCALL _ITM_LM256(_ITM_transaction*, const __m256*);
void _ITM_FASTCALL _ITM_LCF(_ITM_transaction*, const _Complex float*);
void _ITM_FASTCALL _ITM_LCD(_ITM_transaction*, const _Complex double*);
void _ITM_FASTCALL _ITM_LCE(_ITM_transaction*, const _Complex long double*);
void _ITM_FASTCALL _ITM_LB(_ITM_transaction*, const void*, size_t);

// -----------------------------------------------------------------------------
// 5.17 User registered commit and undo actions --------------------------------
// -----------------------------------------------------------------------------
void _ITM_FASTCALL _ITM_addUserCommitAction(_ITM_transaction*, _ITM_userCommitFunction, _ITM_transactionId_t, void*);
void _ITM_FASTCALL _ITM_addUserUndoAction(_ITM_transaction*, _ITM_userUndoFunction, void*);
void _ITM_FASTCALL _ITM_dropReferences(_ITM_transaction*, void*, size_t);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // STM_ITM2STM_LIBITM_H
