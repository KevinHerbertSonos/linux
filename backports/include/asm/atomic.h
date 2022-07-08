#ifndef __BACKPORT_ASM_ATOMIC_H
#define __BACKPORT_ASM_ATOMIC_H
#include_next <asm/atomic.h>

#ifndef smp_mb__after_atomic
#define smp_mb__after_atomic smp_mb__after_clear_bit
#endif

#endif /* __BACKPORT_ASM_ATOMIC_H */
