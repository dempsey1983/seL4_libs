/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SYNC_SEM_BARE_H_
#define _SYNC_SEM_BARE_H_

/* An unmanaged semaphore; i.e. the caller stores the state related to the
 * semaphore itself. This can be useful in scenarios such as CAmkES, where
 * immediate concurrency means we have a race on initialising a managed
 * semaphore.
 */

#include <assert.h>
#include <limits.h>
#include <sel4/sel4.h>
#include <stddef.h>
#include <sync/atomic.h>

static inline int sync_sem_bare_wait(seL4_CPtr ep, volatile int *value) {
#ifdef SEL4_DEBUG_KERNEL
    /* Check the cap actually is an EP. */
    assert(seL4_DebugCapIdentify(ep) == 4);
#endif
    assert(value != NULL);
    int oldval;
    int result = sync_atomic_decrement_safe(value, &oldval, __ATOMIC_ACQUIRE);
    if (result != 0) {
        /* Failed decrement; too many outstanding lock holders. */
        return -1;
    }
    if (oldval <= 0) {
        seL4_Wait(ep, NULL);
        /* Even though we performed an acquire barrier during the atomic
         * decrement we did not actually have the lock yet, so we have
         * to do another one now */
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
    }
    return 0;
}

static inline int sync_sem_bare_trywait(seL4_CPtr ep, volatile int *value) {
    int val = *value;
    while (val > 0) {
        if (__atomic_compare_exchange_n(value, &val, val - 1, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            /* We got it! */
            return 0;
        }
        /* We didn't get it. */
        val = *value;
    }
    /* The semaphore is empty. */
    return -1;
}

static inline int sync_sem_bare_post(seL4_CPtr ep, volatile int *value) {
#ifdef SEL4_DEBUG_KERNEL
    /* Check the cap actually is an EP. */
    assert(seL4_DebugCapIdentify(ep) == 4);
#endif
    assert(value != NULL);
    /* We can do an "unsafe" increment here because we know the lock cannot be
     * full due to ourselves having been able to acquire it.
     */
    assert(*value < INT_MAX);
    int v = sync_atomic_increment(value, __ATOMIC_RELEASE);
    if (v <= 0) {
        seL4_Signal(ep);
    }
    return 0;
}

#endif
