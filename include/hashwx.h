/*
  Copyright (c) 2020-2026 tevador <tevador@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef HASHWX_H
#define HASHWX_H

#include <stdint.h>
#include <stddef.h>

/* Opaque struct representing a HashWX instance */
typedef struct hashwx_ctx hashwx_ctx;

/* Type of hash function */
typedef enum hashwx_type {
    HASHWX_INTERPRETED,
    HASHWX_COMPILED
} hashwx_type;

/* Sentinel value used to indicate unsupported type */
#define HASHWX_NOTSUPP ((hashwx_ctx*)-1)
/* Size of the seed for hashwx_make */
#define HASHWX_SEED_SIZE 32

#if defined(_WIN32) || defined(__CYGWIN__)
#define HASHWX_WIN
#endif

/* Shared/static library definitions */
#ifdef HASHWX_WIN
    #ifdef HASHWX_SHARED
        #define HASHWX_API __declspec(dllexport)
    #elif !defined(HASHWX_STATIC)
        #define HASHWX_API __declspec(dllimport)
    #else
        #define HASHWX_API
    #endif
    #define HASHWX_PRIVATE
#elif defined(__wasm__)
    #define HASHWX_API extern
    #define HASHWX_PRIVATE
#else
    #ifdef HASHWX_SHARED
        #define HASHWX_API __attribute__ ((visibility ("default")))
    #else
        #define HASHWX_API __attribute__ ((visibility ("hidden")))
    #endif
    #define HASHWX_PRIVATE __attribute__ ((visibility ("hidden")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocate a HashWX instance.
 *
 * @param type is the type of instance to be created.
 *
 * @return pointer to a new HashWX instance. Returns NULL on memory allocation 
 *         failure and HASHWX_NOTSUPP if the requested type is not supported.
*/
HASHWX_API hashwx_ctx* hashwx_alloc(hashwx_type type);

/*
 * Create a new HashWX function from a 256-bit seed.
 *
 * @param ctx is pointer to a HashWX instance.
 * @param seed is a pointer to the seed value.
*/
HASHWX_API void hashwx_make(hashwx_ctx* ctx, const uint8_t seed[HASHWX_SEED_SIZE]);

/*
 * Execute the HashWX function.
 *
 * @param ctx is pointer to a HashWX instance. A HashWX function must have
 *        been previously created by calling hashwx_make.
 * @param input is the input to be hashed (64-bit unsigned integer).
 *
 * @return the hash result as a 64-bit unsigned integer. 
 s*/
HASHWX_API uint64_t hashwx_exec(const hashwx_ctx* ctx, uint64_t input);

/*
 * Free a HashWX instance.
 *
 * @param ctx is pointer to a HashWX instance.
*/
HASHWX_API void hashwx_free(hashwx_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif