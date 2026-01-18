/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static inline void read_option(const char* option, int argc, char** argv, bool* out) {
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], option) == 0) {
            *out = true;
            return;
        }
    }
    *out = false;
}

static inline void read_int_option(const char* option, int argc, char** argv, int* out, int default_val) {
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], option) == 0 && (*out = atoi(argv[i + 1])) > 0) {
            return;
        }
    }
    *out = default_val;
}

#endif
