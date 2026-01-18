/* Copyright (c) 2020-2026 tevador <tevador@gmail.com> */
/* See LICENSE for licensing information */

#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "hashwx.h"

HASHWX_PRIVATE void* hashwx_vm_alloc(size_t size);
HASHWX_PRIVATE void hashwx_vm_rw(void* ptr, size_t size);
HASHWX_PRIVATE void hashwx_vm_rx(void* ptr, size_t size);
HASHWX_PRIVATE void hashwx_vm_free(void* ptr, size_t size);

#endif
