/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#include "virtual_memory.h"

#ifndef __wasm__

#if defined(HASHWX_WIN)
#include <windows.h>
#else
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#define PAGE_READONLY PROT_READ
#define PAGE_READWRITE (PROT_READ | PROT_WRITE)
#define PAGE_EXECUTE_READ (PROT_READ | PROT_EXEC)
#define PAGE_EXECUTE_READWRITE (PROT_READ | PROT_WRITE | PROT_EXEC)
#if defined(__NetBSD__)
#define RESERVED_FLAGS PROT_MPROTECT(PROT_EXEC)
#else
#define RESERVED_FLAGS 0
#endif
#endif

void* hashwx_vm_alloc(size_t bytes) {
    void* mem;
#ifdef HASHWX_WIN
    mem = VirtualAlloc(NULL, bytes, MEM_COMMIT, PAGE_READWRITE);
#else
    mem = mmap(NULL, bytes, PAGE_READWRITE | RESERVED_FLAGS, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
#endif
    return mem;
}

static inline int page_protect(void* ptr, size_t bytes, int rules) {
#ifdef HASHWX_WIN
    DWORD oldp;
    if (!VirtualProtect(ptr, bytes, (DWORD)rules, &oldp)) {
        return 0;
    }
#else
    if (-1 == mprotect(ptr, bytes, rules)) {
        return 0;
    }
#endif
    return 1;
}

void hashwx_vm_rw(void* ptr, size_t bytes) {
    page_protect(ptr, bytes, PAGE_READWRITE);
}

void hashwx_vm_rx(void* ptr, size_t bytes) {
    page_protect(ptr, bytes, PAGE_EXECUTE_READ);
}

void hashwx_vm_free(void* ptr, size_t bytes) {
#ifdef HASHWX_WIN
    (void)bytes;
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, bytes);
#endif
}

#endif /* __wasm__ */
