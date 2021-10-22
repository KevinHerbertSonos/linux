/*
 * Copyright (C) Paul Mackerras 1997.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * NOTE: this code runs in 32 bit mode and is packaged as ELF32.
 */

#include "ppc_asm.h"

	.text
	/* a procedure descriptor used when booting this as a COFF file */
	.globl	_zimage_start_opd
_zimage_start_opd:
	.long	_zimage_start, 0, 0, 0

	.weak	_zimage_start
	.globl	_zimage_start
_zimage_start:
	.globl	_zimage_start_lib
_zimage_start_lib:
	/* Work out the offset between the address we were linked at
	   and the address where we're running. */
	bl	1f
1:	mflr	r0
	lis	r9,1b@ha
	addi	r9,r9,1b@l
	subf.	r0,r9,r0
	beq	3f		/* if running at same address as linked */

	/* The .got2 section contains a list of addresses, so add
	   the address offset onto each entry. */
	lis	r9,__got2_start@ha
	addi	r9,r9,__got2_start@l
	lis	r8,__got2_end@ha
	addi	r8,r8,__got2_end@l
	subf.	r8,r9,r8
	beq	3f
	srwi.	r8,r8,2
	mtctr	r8
	add	r9,r0,r9
2:	lwz	r8,0(r9)
	add	r8,r8,r0
	stw	r8,0(r9)
	addi	r9,r9,4
	bdnz	2b

	/* Do a cache flush for our text, in case the loader didn't */
3:	lis	r9,_start@ha
	addi	r9,r9,_start@l
	add	r9,r0,r9
	lis	r8,_etext@ha
	addi	r8,r8,_etext@l
	add	r8,r0,r8
4:	dcbf	r0,r9
	icbi	r0,r9
	addi	r9,r9,0x20
	cmplw	cr0,r9,r8
	blt	4b
	sync
	isync

	/* Clear the BSS */
	lis	r9,__bss_start@ha
	addi	r9,r9,__bss_start@l
	add	r9,r0,r9
	lis	r8,_end@ha
	addi	r8,r8,_end@l
	add	r8,r0,r8
	li	r10,0
5:	stw	r10,0(r9)
	addi	r9,r9,4
	cmplw	cr0,r9,r8
	blt	5b
#if 0
	.balignl	0x20, 0x60000000
	bl	c1
	.balignl	0x20, 0x60000000
	bl	c2
	.balignl	0x20, 0x60000000
	bl	c3
	.balignl	0x20, 0x60000000
	bl	c4
	.balignl	0x20, 0x60000000
	bl	c5
	.balignl	0x20, 0x60000000
	bl	c6
	.balignl	0x20, 0x60000000
	bl	c7
	.balignl	0x20, 0x60000000
	bl	c8
	.balignl	0x20, 0x60000000
	bl	c9
	.balignl	0x20, 0x60000000
	bl	c10
	.balignl	0x20, 0x60000000
	bl	c11
	.balignl	0x20, 0x60000000
	bl	c12
	.balignl	0x20, 0x60000000
	bl	c13
	.balignl	0x20, 0x60000000
	bl	c14
	.balignl	0x20, 0x60000000
	bl	c15
	.balignl	0x20, 0x60000000
	bl	c16
	.balignl	0x20, 0x60000000
	bl	c17
	.balignl	0x20, 0x60000000
	bl	c18
	.balignl	0x20, 0x60000000
	bl	c19
	.balignl	0x20, 0x60000000
	bl	c20
	.balignl	0x20, 0x60000000
	bl	c21
	.balignl	0x20, 0x60000000
	bl	c22
	.balignl	0x20, 0x60000000
	bl	c23
	.balignl	0x20, 0x60000000
	bl	c24
	.balignl	0x20, 0x60000000
	bl	c25
	.balignl	0x20, 0x60000000
	bl	c26
	.balignl	0x20, 0x60000000
	bl	c27
	.balignl	0x20, 0x60000000
	bl	c28
	.balignl	0x20, 0x60000000
	bl	c29
	.balignl	0x20, 0x60000000
	bl	c30
	.balignl	0x20, 0x60000000
	bl	c31
	.balignl	0x20, 0x60000000
	bl	c32
#endif	

	/* Possibly set up a custom stack */
.weak	_platform_stack_top
	lis	r8,_platform_stack_top@ha
	addi	r8,r8,_platform_stack_top@l
	cmpwi	r8,0
	beq	6f
	add	r8,r0,r8
	lwz	r1,0(r8)
	add	r1,r0,r1
	li	r0,0
	stwu	r0,-16(r1)	/* establish a stack frame */
6:

	/* Call platform_init() */
	bl	platform_init

	/* Call start */
	b	start

#if 0
	.balignl	0x20
c1:	hwsync
	blr

	.balignl	0x20
c2:	hwsync
	blr

	.balignl	0x20
c3:	hwsync
	blr

	.balignl	0x20
c4:	hwsync
	blr

				
	.balignl	0x20
c5:	hwsync
	blr

	.balignl	0x20
c6:	hwsync
	blr

	.balignl	0x20
c7:	hwsync
	blr

	.balignl	0x20
c8:	hwsync
	blr

				
	.balignl	0x20
c9:	hwsync
	blr

	.balignl	0x20
c10:	hwsync
	blr

				
	.balignl	0x20
c11:	hwsync
	blr

	.balignl	0x20
c12:	hwsync
	blr

	.balignl	0x20
c13:	hwsync
	blr

	.balignl	0x20
c14:	hwsync
	blr

				
	.balignl	0x20
c15:	hwsync
	blr

	.balignl	0x20
c16:	hwsync
	blr

	.balignl	0x20
c17:	hwsync
	blr

	.balignl	0x20
c18:	hwsync
	blr

				
	.balignl	0x20
c19:	hwsync
	blr

	.balignl	0x20
c20:	hwsync
	blr

	.balignl	0x20
c21:	hwsync
	blr

	.balignl	0x20
c22:	hwsync
	blr

	.balignl	0x20
c23:	hwsync
	blr

	.balignl	0x20
c24:	hwsync
	blr

				
	.balignl	0x20
c25:	hwsync
	blr

	.balignl	0x20
c26:	hwsync
	blr

	.balignl	0x20
c27:	hwsync
	blr

	.balignl	0x20
c28:	hwsync
	blr

				
	.balignl	0x20
c29:	hwsync
	blr

	.balignl	0x20
c30:	hwsync
	blr

	.balignl	0x20
c31:	hwsync
	blr

	.balignl	0x20
c32:	hwsync
	blr

#endif
	
