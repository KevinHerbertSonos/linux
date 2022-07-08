/*
    Structures, defines and functions specific to the Freescale MPC8247 (MPC8272)
    See https://www.nxp.com/docs/en/reference-manual/MPC8272RM.pdf

    Prior to kernel version 2.6.x these were part of the PPC include files in the Linux
    kernel code.  Since they are no longer part of the kernel, we need to define them ourselves.
    Note that the 2.6.x kernel supports MPC8247 (MPC8260), but no longer provides the means to
    directly access the parallel I/O ports/pins that the AudioDev driver needs to control LEDs
    and such.
*/

#ifndef __MPC8247
#define __MPC8247

#include <asm/cpm2.h>
#include <asm/immap_cpm2.h>

#define IMAP_ADDR               ((uint)0xf0000000)
#define BCSR_ADDR               ((uint)0xf8000000)
#define BCSR_SIZE               ((uint)(32 * 1024))
#define BCSR0_LED0              ((uint)0x02000000)
#define BCSR0_LED1              ((uint)0x01000000)
#define BCSR1_FETHIEN           ((uint)0x08000000)
#define BCSR1_FETH_RST          ((uint)0x04000000)
#define BCSR1_RS232_EN1         ((uint)0x02000000)
#define BCSR1_RS232_EN2         ((uint)0x01000000)


#define SA_INTERRUPT    0x20000000 /* dummy -- ignored */


#define	SIU_INT_IRQ1		    ((uint)0x13)
#define	SIU_INT_IRQ2		    ((uint)SIU_INT_IRQ1+1)
#define	SIU_INT_IRQ3		    ((uint)SIU_INT_IRQ2+1)
#define	SIU_INT_IRQ4		    ((uint)SIU_INT_IRQ3+1)
#define	SIU_INT_IRQ5		    ((uint)SIU_INT_IRQ4+1)
#define	SIU_INT_IRQ6		    ((uint)SIU_INT_IRQ5+1)
#define	SIU_INT_IRQ7		    ((uint)SIU_INT_IRQ6+1)

#define SIU_INT_PC15            ((uint)0x32)
#define SIU_INT_PC14            ((uint)SIU_INT_PC15+1)
#define SIU_INT_PC13            ((uint)SIU_INT_PC14+1)
#define SIU_INT_PC12            ((uint)SIU_INT_PC13+1)
#define SIU_INT_PC11            ((uint)SIU_INT_PC12+1)
#define SIU_INT_PC10            ((uint)SIU_INT_PC11+1)
#define SIU_INT_PC9             ((uint)SIU_INT_PC10+1)
#define SIU_INT_PC8             ((uint)SIU_INT_PC9+1)
#define SIU_INT_PC7             ((uint)SIU_INT_PC8+1)
#define SIU_INT_PC6             ((uint)SIU_INT_PC7+1)
#define SIU_INT_PC5             ((uint)SIU_INT_PC6+1)
#define SIU_INT_PC4             ((uint)SIU_INT_PC5+1)

#define SIU_INT_SCC1            ((uint)0x28)
#define SIU_INT_SCC2            ((uint)SIU_INT_SCC1+1)
#define SIU_INT_SCC3            ((uint)SIU_INT_SCC2+1)
#define SIU_INT_SCC4            ((uint)SIU_INT_SCC3+1)

#define SIU_INT_I2C             ((uint)0x01)

#define SIRE_SWTR ((ushort)0x4000)
#define SIRE_SSEL1 ((ushort)0x2000)
#define SIRE_SSEL2 ((ushort)0x1000)
#define SIRE_SSEL3 ((ushort)0x0800)
#define SIRE_SSEL4 ((ushort)0x0400)
#define SIRE_CSEL_NONE ((ushort)0x0000)
#define SIRE_CSEL_SCC1 ((ushort)0x0020)
#define SIRE_CSEL_SCC3 ((ushort)0x0060)
#define SIRE_CSEL_SCC4 ((ushort)0x0080)
#define SIRE_CSEL_SMC1 ((ushort)0x00a0)
#define SIRE_CSEL_SMC2 ((ushort)0x00c0)
#define SIRE_CSEL_DGNT ((ushort)0x00e0)
#define SIRE_CSEL_FCC1 ((ushort)0x0120)
#define SIRE_CSEL_FCC2 ((ushort)0x0140)
#define SIRE_CNT1 ((ushort)0x0000)
#define SIRE_CNT2 ((ushort)0x0004)
#define SIRE_CNT3 ((ushort)0x0008)
#define SIRE_CNT4 ((ushort)0x000c)
#define SIRE_CNT5 ((ushort)0x0010)
#define SIRE_CNT6 ((ushort)0x0014)
#define SIRE_CNT7 ((ushort)0x0018)
#define SIRE_CNT8 ((ushort)0x001c)
#define SIRE_BYT ((ushort)0x0002)
#define SIRE_LST ((ushort)0x0001)

#define SI_GMR_STZB     ((u_char)0x20)
#define SI_GMR_STZA     ((u_char)0x10)
#define SI_GMR_ENB      ((u_char)0x02)
#define SI_GMR_ENA      ((u_char)0x01)

#define SI_MR_SAD0      ((ushort)0x0000)
#define SI_MR_SAD32     ((ushort)0x1000)
#define SI_MR_SAD64     ((ushort)0x2000)
#define SI_MR_SAD96     ((ushort)0x3000)
#define SI_MR_SAD128    ((ushort)0x4000)
#define SI_MR_SAD160    ((ushort)0x5000)
#define SI_MR_SAD192    ((ushort)0x6000)
#define SI_MR_SAD224    ((ushort)0x7000)
#define SI_MR_SDM_NORM  ((ushort)0x0000)
#define SI_MR_SDM_ECHO  ((ushort)0x0400)
#define SI_MR_SDM_LOOP  ((ushort)0x0800)
#define SI_MR_SDM_LCTL  ((ushort)0x0c00)
#define SI_MR_RFSD0     ((ushort)0x0000)
#define SI_MR_RFSD1     ((ushort)0x0100)
#define SI_MR_RFSD2     ((ushort)0x0200)
#define SI_MR_RFSD3     ((ushort)0x0300)
#define SI_MR_DSC       ((ushort)0x0080)
#define SI_MR_CRT       ((ushort)0x0040)
#define SI_MR_SL        ((ushort)0x0020)
#define SI_MR_CE        ((ushort)0x0010)
#define SI_MR_FE        ((ushort)0x0008)
#define SI_MR_GM        ((ushort)0x0004)
#define SI_MR_TFSD0     ((ushort)0x0000)
#define SI_MR_TFSD1     ((ushort)0x0001)
#define SI_MR_TFSD2     ((ushort)0x0002)
#define SI_MR_TFSD3     ((ushort)0x0003)

#define SI_RSR_SSADA0 ((ushort)0x0000)
#define SI_RSR_SSADA32 ((ushort)0x1000)
#define SI_RSR_SSADA64 ((ushort)0x2000)
#define SI_RSR_SSADA96 ((ushort)0x3000)
#define SI_RSR_SSADA128 ((ushort)0x4000)
#define SI_RSR_SSADA160 ((ushort)0x5000)
#define SI_RSR_SSADA192 ((ushort)0x6000)
#define SI_RSR_SSADA224 ((ushort)0x7000)

#define SI_RSR_SSADB0 ((ushort)0x0000)
#define SI_RSR_SSADB32 ((ushort)0x0100)
#define SI_RSR_SSADB64 ((ushort)0x0200)
#define SI_RSR_SSADB96 ((ushort)0x0300)
#define SI_RSR_SSADB128 ((ushort)0x0400)
#define SI_RSR_SSADB160 ((ushort)0x0500)
#define SI_RSR_SSADB192 ((ushort)0x0600)
#define SI_RSR_SSADB224 ((ushort)0x0700)

#define SI_CMDR_CSRRA ((u_char)0x80)
#define SI_CMDR_CSRTA ((u_char)0x40)
#define SI_CMDR_CSRRB ((u_char)0x20)
#define SI_CMDR_CSRTB ((u_char)0x10)

#define SI_STR_CRORA ((u_char)0x80)
#define SI_STR_CROTA ((u_char)0x40)
#define SI_STR_CRORB ((u_char)0x20)
#define SI_STR_CROTB ((u_char)0x10)

#define I2C_I2MOD_REVD ((u_char)0x20)
#define I2C_I2MOD_GCD ((u_char)0x10)
#define I2C_I2MOD_FLT ((u_char)0x08)
#define I2C_I2MOD_PDIV_32 ((u_char)0x00)
#define I2C_I2MOD_PDIV_16 ((u_char)0x02)
#define I2C_I2MOD_PDIV_8 ((u_char)0x04)
#define I2C_I2MOD_PDIV_4 ((u_char)0x06)
#define I2C_I2MOD_EN ((u_char)0x01)

#define I2C_I2CER_TXE ((u_char)0x10)
#define I2C_I2CER_BSY ((u_char)0x04)
#define I2C_I2CER_TXB ((u_char)0x02)
#define I2C_I2CER_RXB ((u_char)0x01)

#define BD_SC_S         ((ushort)0x0400)	/* Generate SOF (I2C) */

#define PORT_NONE   (-1)
#define PORT_A      0
#define PORT_B      1
#define PORT_C      2
#define PORT_D      3

#define PIN_NONE    (-1)

// Option flags to pass to m8260_port_set to indicate which operations
// should be performed.
// Open Drain
#define PORT_SET_ODR	        0x00000001
#define PORT_CLEAR_ODR	        0x00000002
// Data
#define PORT_SET_DAT	        0x00000004
#define PORT_CLEAR_DAT	        0x00000008
// Direction
#define PORT_SET_DIR	        0x00000010
#define PORT_CLEAR_DIR	        0x00000020
//Pin Assignment
#define PORT_SET_PAR	        0x00000040
#define PORT_CLEAR_PAR	        0x00000080
// Special Operations
#define PORT_SET_SOR	        0x00000100
#define PORT_CLEAR_SOR	        0x00000200

#define CPM_DP_NOSPACE		((uint)0x7fffffff)

#endif
