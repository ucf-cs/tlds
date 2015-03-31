/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  Step 1:
 *    Include the configuration code for the harness, and the API code.
 */

#include <iostream>
#include <api/api.hpp>
#include "bmconfig.hpp"

/**
 *  We provide the option to build the entire benchmark in a single
 *  source. The bmconfig.hpp include defines all of the important functions
 *  that are implemented in this file, and bmharness.cpp defines the
 *  execution infrastructure.
 */
#ifdef SINGLE_SOURCE_BUILD
#include "bmharness.cpp"
#endif

/**
 *  Step 2:
 *    Declare the data type that will be stress tested via this benchmark.
 *    Also provide any functions that will be needed to manipulate the data
 *    type.  Take care to avoid unnecessary indirection.
 */
struct TypeTestObject
{
    char               m_cfield;
    unsigned char      m_ucfield;
    int                m_ifield;
    unsigned int       m_uifield;
    long               m_lfield;
    unsigned long      m_ulfield;
    long long          m_llfield;
    unsigned long long m_ullfield;
    float              m_ffield;
    double             m_dfield;

    TypeTestObject()
        : m_cfield('a'),
          m_ucfield(255),
          m_ifield(-200000000L),
          m_uifield(4000000000UL),
          m_lfield(-2000000000L),
          m_ulfield(4000000000UL),
          m_llfield(-400000000000000000LL),
          m_ullfield(400000000000000000ULL),
          m_ffield(1.05f),
          m_dfield(1.07)
    { }
};

/**
 *  Step 3:
 *    Declare an instance of the data type, and provide init, test, and verify
 *    functions
 */
TypeTestObject* tto;

/**
 *  This is just to make sure that all primitive data types are read,
 *  written, and read-after-written correctly.  You'll need to visually
 *  inspect the output to make sure everything is correct, unless you
 *  feel like running this in a debugger and watching that the correct
 *  templated versions of stm read and stm write are called, and that
 *  the values are being stored and retrieved correctly from the log
 *  types.
 */
static TM_CALLABLE
void DataTypeTest(TM_ARG_ALONE)
{
#if !defined(STM_API_CXXTM)
    TM_WAIVER {
        std::cout << "----------------------------\n";
    }
#endif

    // test char and uchar
    char c  = TM_READ(tto->m_cfield);;
    unsigned char uc = TM_READ(tto->m_ucfield);
    char c2 = c + 1;
    unsigned char uc2= uc + 1;
    TM_WRITE(tto->m_cfield, c2);
    TM_WRITE(tto->m_ucfield, uc2);
    c2 = TM_READ(tto->m_cfield);
    uc2 = TM_READ(tto->m_ucfield);
#if !defined(STM_API_CXXTM)
    TM_WAIVER {
        std::cout << "(c,uc) from ("
             << (int)c << "," << (int)uc << ") to ("
             << (int)c2 << "," << (int)uc2 << ")\n";
    }
#endif

    // test int, unsigned int, long, unsigned long
    int i = TM_READ(tto->m_ifield);
    unsigned int ui = TM_READ(tto->m_uifield);
    long l = TM_READ(tto->m_lfield);
    unsigned long ul = TM_READ(tto->m_ulfield);
    int i2 = i + 1;
    unsigned int ui2 = ui + 1;
    long l2 = l + 1;
    unsigned long ul2 = ul + 1;
    TM_WRITE(tto->m_ifield, i2);
    TM_WRITE(tto->m_uifield, ui2);
    TM_WRITE(tto->m_lfield, l2);
    TM_WRITE(tto->m_ulfield, ul2);
    i2 = TM_READ(tto->m_ifield);
    ui2 = TM_READ(tto->m_uifield);
    l2 = TM_READ(tto->m_lfield);
    ul2 = TM_READ(tto->m_ulfield);
#if !defined(STM_API_CXXTM)
    TM_WAIVER {
        std::cout << "(i,ui,l,ul) from ("
             << i << "," << ui << "," << l << "," << ul
             << ") to ("
             << i2 << "," << ui2 << "," << l2 << "," << ul2
             << ")\n";
    }
#endif
    // test long long and unsigned long long
    long long ll = TM_READ(tto->m_llfield);
    unsigned long long ull = TM_READ(tto->m_ullfield);
    long long ll2 = ll+ 1;
    unsigned long long ull2 = ull+ 1;
    TM_WRITE(tto->m_llfield, ll2);
    TM_WRITE(tto->m_ullfield, ull2);
    ll2 = TM_READ(tto->m_llfield);
    ull2 = TM_READ(tto->m_ullfield);
#if !defined(STM_API_CXXTM)
    TM_WAIVER {
        std::cout << "(ll,ull) from ("
             << ll << "," << ull << ") to ("
             << ll2 << "," << ull2 << ")\n";
    }
#endif
    // test float and double
    float f = TM_READ(tto->m_ffield);
    double d = TM_READ(tto->m_dfield);
    float f2 = f + 1;
    double d2 = d + 1;
    TM_WRITE(tto->m_ffield, f2);
    TM_WRITE(tto->m_dfield, d2);
    f2 = TM_READ(tto->m_ffield);
    d2 = TM_READ(tto->m_dfield);
#if !defined(STM_API_CXXTM)
    TM_WAIVER {
        std::cout << "(f,d) from ("
             << f << "," << d << ") to ("
             << f2 << "," << d2 << ")\n";
    }
#endif
}

/*** Initialize the counter */
void bench_init()
{
    tto = new TypeTestObject();
}

/*** Run a bunch of random transactions */
void bench_test(uintptr_t, uint32_t*)
{
    TM_BEGIN(atomic) {
        DataTypeTest(TM_PARAM_ALONE);
    } TM_END;
}

/*** Ensure the final state of the benchmark satisfies all invariants */
bool bench_verify() { return true; }

/**
 *  Step 4:
 *    Include the code that has the main() function, and the code for creating
 *    threads and calling the three above-named functions.  Don't forget to
 *    provide an arg reparser.
 */

/*** Deal with special names that map to different M values */
void bench_reparse()
{
    if      (CFG.bmname == "")          CFG.bmname   = "TypeTest";
    if      (CFG.threads != 1)          CFG.threads = 1;
}
