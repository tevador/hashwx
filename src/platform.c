/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "platform.h"
#include <hashwx.h>

#if defined(HASHWX_WIN)
#include <windows.h>
#else
#include <sys/time.h>
#endif

double platform_wall_clock(void) {
#ifdef HASHWX_WIN
    static double freq = 0;
    if (freq == 0) {
        LARGE_INTEGER freq_long;
        if (!QueryPerformanceFrequency(&freq_long)) {
            return 0;
        }
        freq = freq_long.QuadPart;
    }
    LARGE_INTEGER time;
    if (!QueryPerformanceCounter(&time)) {
        return 0;
    }
    return time.QuadPart / freq;
#else
    struct timeval time;
    if (gettimeofday(&time, NULL) != 0) {
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * 1.0e-6;
#endif
}
