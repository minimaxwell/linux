/*
 * 2010 (c) CSSI, Inc.
 */

#ifdef __KERNEL__
#ifndef __ASM_MCR3000_H__
#define __ASM_MCR3000_H__

/* Bits of interest in the BCSRs.
 */
#define BCSR1_ETHEN		((uint)0x20000000)
#define BCSR1_IRDAEN		((uint)0x10000000)
#define BCSR1_RS232EN_1		((uint)0x01000000)
#define BCSR1_PCCEN		((uint)0x00800000)
#define BCSR1_PCCVCC0		((uint)0x00400000)
#define BCSR1_PCCVPP0		((uint)0x00200000)
#define BCSR1_PCCVPP1		((uint)0x00100000)
#define BCSR1_PCCVPP_MASK	(BCSR1_PCCVPP0 | BCSR1_PCCVPP1)
#define BCSR1_RS232EN_2		((uint)0x00040000)
#define BCSR1_PCCVCC1		((uint)0x00010000)
#define BCSR1_PCCVCC_MASK	(BCSR1_PCCVCC0 | BCSR1_PCCVCC1)

#define BCSR4_ETH10_RST		((uint)0x80000000)	/* 10Base-T PHY reset*/
#define BCSR4_USB_LO_SPD	((uint)0x04000000)
#define BCSR4_USB_VCC		((uint)0x02000000)
#define BCSR4_USB_FULL_SPD	((uint)0x00040000)
#define BCSR4_USB_EN		((uint)0x00020000)

#define BCSR5_MII2_EN		0x40
#define BCSR5_MII2_RST		0x20
#define BCSR5_T1_RST		0x10
#define BCSR5_ATM155_RST	0x08
#define BCSR5_ATM25_RST		0x04
#define BCSR5_MII1_EN		0x02
#define BCSR5_MII1_RST		0x01

/* Memory map is configured by the PROM startup.
 * We just map a few things we need.  The CSR is actually 4 byte-wide
 * registers that can be accessed as 8-, 16-, or 32-bit values.
 */
//#define IMAP_ADDR		((uint)(0xff000000))
//#define IMAP_SIZE		((uint)(64 * 1024))
//#define DPRAM_ADDR              ((uint)(0x08000000))
//#define DPRAM_SIZE              ((uint)(0x0C000000) - DPRAM_ADDR) 
//#define PERIPH_ADDR             ((uint)(0x10000000))
//#define PERIPH_SIZE             ((uint)(0x14000000) - PERIPH_ADDR)
//#define FPGA_ADDR               ((uint)(0x14000000))
//#define FPGA_SIZE               ((uint)(0x18000000) - FPGA_ADDR)
//#define CPLD_ADDR               ((uint)(0x10000800))
//#define CPLD_SIZE               ((uint)(0x10000A00) - CPLD_ADDR)
//#define DSP_ADDR                ((uint)(0x1C000000))
//#define DSP_SIZE                ((uint)(0x20000000) - DSP_ADDR)
//#define MCR_CPLD_RST		((uint)(CPLD_ADDR + 0 ))
//#define MCR_CPLD_CMD		((uint)(CPLD_ADDR + 2 ))
//#define MCR_CPLD_EVE		((uint)(CPLD_ADDR + 4 ))
//#define MCR_CPLD_CDG		((uint)(CPLD_ADDR + 6 ))

#endif /* __ASM_MCR3000_H__ */
#endif /* __KERNEL__ */
