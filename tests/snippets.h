/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Contributed by: Intel Corporation
 */

/*
 * Useful features:
 *   array_size()   constant expression for C array size,
 *   CRASH_CATCH()  macro for pre-mortal segfault catching,
 *   CLOCK_THAT()   macro to measure block execution time.
 */

#include <gtest/gtest.h>

#ifndef SNIPPETS_H
#define SNIPPETS_H

/*
 * Array size constant expressions
 */

#define array_size(A) sizeof(array_to_sizeof(A))
#define concat_size(A, B) array_size(A)+array_size(B)-1

template <size_t N>
struct N_to_sizeof { typedef char type[N]; };

template <typename T, size_t N>
typename N_to_sizeof<N>::type& array_to_sizeof(T(&)[N]);

/*
 * Crash catching functions to keep GTest alive a bit more
 * in order to print traces before death.
 *
 * Usage:
 *   try_to_catch_crashes() - set segfault catching
 *   reset_death_signals()  - restore default signal handlers
 *   quick_death()          - raise segfault (for testiong purposes)
 */

#include <signal.h>

static struct sigaction old_sa[NSIG];

static void fail_sigaction(__attribute__((unused)) int signal,
                           __attribute__((unused)) siginfo_t *info,
                           __attribute__((unused)) void *reserved)
{
    FAIL();
    exit(0);
}

static void try_to_catch_crashes()
{
    struct sigaction handler;
    memset(&handler, 0, sizeof(struct sigaction));
    handler.sa_sigaction = fail_sigaction;
    handler.sa_flags = SA_RESETHAND;

    #define CATCH_SIG(X) sigaction(X, &handler, &old_sa[X])
    {
        CATCH_SIG(SIGSEGV);
    }
    #undef CATCH_SIG
}

static void reset_death_signals()
{
    #define UNCATCH_SIG(X) sigaction(X, &old_sa[X], &old_sa[X])
    {
        UNCATCH_SIG(SIGSEGV);
    }
    #undef UNCATCH_SIG
}

inline static void quick_death()
{
    char *a = 0;
    a[0] = 0;
}

/*
 * Crash catcher on RAII pattern
 *
 * Usage:
 *   Just add CRASH_CATCH();
 *   in the block and enjoy pre-mortal traces and
 *   greater performance than via death tests.
 */

#define CRASH_CATCH() volatile RaiiCrashCatcher _crash_catcher

struct RaiiCrashCatcher
{
    RaiiCrashCatcher()
    {
        try_to_catch_crashes();
    }
    ~RaiiCrashCatcher()
    {
        reset_death_signals();
    }
};


/*
 * Macro to measure block execution time
 *
 * Usage:
 *   CLOCK_THAT(var_of_double_type)
 *   { ... }
 *   Time in seconds will be stored in specified var.
 */

#define CLOCK_THAT(var) var = 0; \
    for (struct _ClkTime { struct timespec a, b; char flag; _ClkTime() { flag = 1; } } _clk_time; \
         _clk_time.flag && !clock_gettime(CLOCK_REALTIME, &_clk_time.a); \
         _clk_time.flag = 0, ({ \
         clock_gettime(CLOCK_REALTIME, &_clk_time.b); \
         var = (double)(_clk_time.b.tv_sec - _clk_time.a.tv_sec) + \
               (double)(_clk_time.b.tv_nsec - _clk_time.a.tv_nsec) / 1e9; }))

#endif // SNIPPETS_H
