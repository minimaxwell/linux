/* drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 */

#ifndef _PEF2256_H
#define _PEF2256_H

#define MASTER_MODE 0
#define SLAVE_MODE  1

#define CHANNEL_PHASE_0 0
#define CHANNEL_PHASE_1 1
#define CHANNEL_PHASE_2 2
#define CHANNEL_PHASE_3 3

#define CLOCK_RATE_2M 2
#define CLOCK_RATE_4M 4
#define CLOCK_RATE_8M 8
#define CLOCK_RATE_16M 16

#define DATA_RATE_2M 2
#define DATA_RATE_4M 4
#define DATA_RATE_8M 8
#define DATA_RATE_16M 16

#define RX_TIMEOUT 500

#define TS_0 0x80000000

enum versions {
	VERSION_UNDEF = 0,
	VERSION_1_2 = 0x12,
	VERSION_2_1 = 0x21,
	VERSION_2_2 = 0x22,
};

#define WID_IDENT_1		0x03
#define WID_IDENT_1_2		0x03
#define WID_IDENT_2		0xC0
#define WID_IDENT_2_1		0x00
#define WID_IDENT_2_2		0x40


struct pef2256_dev_priv {
	struct sk_buff *tx_skb;
	u16 tx_len;
	struct device *dev;

	int init_done;

	unsigned char *base_addr;
	int component_id;
	int mode;	/* MASTER or SLAVE */
	int board_type;
	int channel_phase;
	int clock_rate;
	int data_rate;
	char rising_edge_sync_pulse[10];

	u16 rx_len;
	u8 rx_buff[2048];

	u32 Tx_TS;	/* Transmit Time Slots */
	u32 Rx_TS;	/* Receive Time Slots */

	unsigned short encoding;
	unsigned short parity;
	struct net_device *netdev;

	int irq;

	u8 R_ISR0;			/* ISR0 register */
	u8 R_ISR1;			/* ISR1 register */
};


/* Framer E1 registers offsets */
#define XFIFO	0x00	/* 0x00/0x01	Tx FIFO */
#define RFIFO	0x00	/* 0x00/0x01	Rx FIFO */
#define	CMDR	0x02	/* 0x02	Command Register */
#define	MODE	0x03	/* 0x03	Mode Register */
#define	RAH1	0x04	/* 0x04	Receive Address High 1 */
#define	RAH2	0x05	/* 0x05	Receive Address High 2 */
#define	RAL1	0x06	/* 0x06	Receive Address Low 1 */
#define	RAL2	0x07	/* 0x07	Receive Address Low 2 */
#define	IPC	0x08	/* 0x08	Interrupt Port Configuration */
#define	CCR1	0x09	/* 0x09	Common Configuration Register 1 */
#define	CCR2	0x0A	/* 0x0A	Common Configuration Register 2 */
#define	Res1	0x0B	/* 0x0B	Free Register 1 */
#define	RTR1	0x0C	/* 0x0C	Receive Time Slot Register 1 */
#define	RTR2	0x0D	/* 0x0D	Receive Time Slot Register 2 */
#define	RTR3	0x0E	/* 0x0E	Receive Time Slot Register 3 */
#define	RTR4	0x0F	/* 0x0F	Receive Time Slot Register 4 */
#define	TTR1	0x10	/* 0x10	Transmit Time Slot Register 1 */
#define	TTR2	0x11	/* 0x11	Transmit Time Slot Register 2 */
#define	TTR3	0x12	/* 0x12	Transmit Time Slot Register 3 */
#define	TTR4	0x13	/* 0x13	Transmit Time Slot Register 4 */
#define	IMR0	0x14	/* 0x14	Interrupt Mask Register 0 */
#define	IMR1	0x15	/* 0x15	Interrupt Mask Register 1 */
#define	IMR2	0x16	/* 0x16	Interrupt Mask Register 2 */
#define	IMR3	0x17	/* 0x17	Interrupt Mask Register 3 */
#define	IMR4	0x18	/* 0x18	Interrupt Mask Register 4 */
#define	IMR5	0x19	/* 0x19	Interrupt Mask Register 5 */
#define	Res2	0x1A	/* 0x1A	Free Register 2 */
#define	IERR	0x1B	/* 0x1B	Single Bit Error Insertion Register */
#define	FMR0	0x1C	/* 0x1C	Framer Mode Register 0 */
#define	FMR1	0x1D	/* 0x1D	Framer Mode Register 1 */
#define	FMR2	0x1E	/* 0x1E	Framer Mode Register 2 */
#define	LOOP	0x1F	/* 0x1F	Channel Loop-Back */
#define	XSW	0x20	/* 0x20	Transmit Service Word */
#define	XSP	0x21	/* 0x21	Transmit Spare Bits */
#define	XC0	0x22	/* 0x22	Transmit Control 0 */
#define	XC1	0x23	/* 0x23	Transmit Control 1 */
#define	RC0	0x24	/* 0x24	Receive Control 0 */
#define	RC1	0x25	/* 0x25	Receive Control 1 */
#define	XPM0	0x26	/* 0x26	Transmit Pulse Mask 0 */
#define	XPM1	0x27	/* 0x27	Transmit Pulse Mask 1 */
#define	XPM2	0x28	/* 0x28	Transmit Pulse Mask 2 */
#define	TSWM	0x29	/* 0x29	Transparent Service Word Mask */
#define	Res3	0x2A	/* 0x2A	Free Register 3 */
#define	IDLE	0x2B	/* 0x2B	Idle Channel Code */
#define	XSA4	0x2C	/* 0x2C	Transmit Sa4-Bit Register */
#define	XSA5	0x2D	/* 0x2D	Transmit Sa5-Bit Register */
#define	XSA6	0x2E	/* 0x2E	Transmit Sa6-Bit Register */
#define	XSA7	0x2F	/* 0x2F	Transmit Sa7-Bit Register */
#define	XSA8	0x30	/* 0x30	Transmit Sa8-Bit Register */
#define	FMR3	0x31	/* 0x31	Framer Mode Register 3 */
#define	ICB1	0x32	/* 0x32	Idle Channel Register 1 */
#define	ICB2	0x33	/* 0x33	Idle Channel Register 2 */
#define	ICB3	0x34	/* 0x34	Idle Channel Register 3 */
#define	ICB4	0x35	/* 0x35	Idle Channel Register 4 */
#define	LIM0	0x36	/* 0x36	Line Interface Mode 0 */
#define	LIM1	0x37	/* 0x37	Line Interface Mode 1 */
#define	PCD	0x38	/* 0x38	Pulse Count Detection */
#define	PCR	0x39	/* 0x39	Pulse Count Recovery */
#define	LIM2	0x3A	/* 0x3A	Line Interface Mode 2 */
#define	LCR1	0x3B	/* 0x3B	Loop Code Register 1 */
#define	LCR2	0x3C	/* 0x3C	Loop Code Register 2 */
#define	LCR3	0x3D	/* 0x3D	Loop Code Register 3 */
#define	SIC1	0x3E	/* 0x3E	System Interface Control 1 */
#define	SIC2	0x3F	/* 0x3F	System Interface Control 2 */
#define	SIC3	0x40	/* 0x40	System Interface Control 3 */
#define	Res4	0x41	/* 0x41	Free Register 4 */
#define	Res5	0x42	/* 0x42	Free Register 5 */
#define	Res6	0x43	/* 0x43	Free Register 6 */
#define	CMR1	0x44	/* 0x44	Clock Mode Register 1 */
#define	CMR2	0x45	/* 0x45	Clock Mode Register 2 */
#define	GCR	0x46	/* 0x46	Global Configuration Register */
#define	ESM	0x47	/* 0x47	Errored Second Mask */
#define	CMR3	0x48	/* 0x48	Clock Mode Register 3 en V2.2 */
#define	RBD	0x49	/* 0x49	Receive Buffer Delay */
#define	VSTR	0x4A	/* 0x4A	Version Status Regiter */
#define	RES	0x4B	/* 0x4B	Receive Equalizer Status */
#define	FRS0	0x4C	/* 0x4C	Framer Receive Status 0 */
#define	FRS1	0x4D	/* 0x4D	Framer Receive Status 1 */
#define	RSW	0x4E	/* 0x4E	Receive Service Word */
#define	RSP	0x4F	/* 0x4F	Receive Spare Bits */
#define	FEC	0x50	/* 0x50/0x51 Framing Error Counter */
#define	CVC	0x52	/* 0x52/0x53 Code Violation Counter */
#define	CEC1	0x54	/* 0x54/0x55 CRC Error Counter 1 */
#define	EBC	0x56	/* 0x56/0x57 E-Bit Error Counter */
#define	CEC2	0x58	/* 0x58/0x59 CRC Error Counter 2 */
#define	CEC3	0x5A	/* 0x5A/0x5B CRC Error Counter 3 */
#define	RSA4	0x5C	/* 0x5C	Receive Sa4-Bit Register */
#define	RSA5	0x5D	/* 0x5D	Receive Sa5-Bit Register */
#define	RSA6	0x5E	/* 0x5E	Receive Sa6-Bit Register */
#define	RSA7	0x5F	/* 0x5F	Receive Sa7-Bit Register */
#define DEC	0x60	/* 0x60 Common Register - Disable Error Counter */
#define RSA8	0x60	/* 0x60 Common Register - Receive Sa8-Bit Regiter */
#define	RSA6S	0x61	/* 0x61	Receive Sa6-Bit Status Register */
#define	RSP1	0x62	/* 0x62	Receive Signaling Pointer 1 */
#define	RSP2	0x63	/* 0x63	Receive Signaling Pointer 2 */
#define	SIS	0x64	/* 0x64	Signaling Status Register */
#define	RSIS	0x65	/* 0x65	Receive Signaling Status Register */
#define	RBCL	0x66	/* 0x66	Receive Byte Control */
#define	RBCH	0x67	/* 0x67	Receive Byte Control */
#define	ISR0	0x68	/* 0x68	Interrupt Status Register 0 */
#define	ISR1	0x69	/* 0x69	Interrupt Status Register 1 */
#define	ISR2	0x6A	/* 0x6A	Interrupt Status Register 2 */
#define	ISR3	0x6B	/* 0x6B	Interrupt Status Register 3 */
#define	ISR4	0x6C	/* 0x6C	Interrupt Status Register 4 */
#define	ISR5	0x6D	/* 0x6D	Interrupt Status Register 5 */
#define	GIS	0x6E	/* 0x6E	Global Interrupt Status */
#define	Res8	0x6F	/* 0x6F	Free Register 8 */
#define	CAS1	0x70	/* 0x70	CAS Register 1 */
#define	CAS2	0x71	/* 0x71	CAS Register 2 */
#define	CAS3	0x72	/* 0x72	CAS Register 3 */
#define	CAS4	0x73	/* 0x73	CAS Register 4 */
#define	CAS5	0x74	/* 0x74	CAS Register 5 */
#define	CAS6	0x75	/* 0x75	CAS Register 6 */
#define	CAS7	0x76	/* 0x76	CAS Register 7 */
#define	CAS8	0x77	/* 0x77	CAS Register 8 */
#define	CAS9	0x78	/* 0x78	CAS Register 9 */
#define	CAS10	0x79	/* 0x79	CAS Register 10 */
#define	CAS11	0x7A	/* 0x7A	CAS Register 11 */
#define	CAS12	0x7B	/* 0x7B	CAS Register 12 */
#define	CAS13	0x7C	/* 0x7C	CAS Register 13 */
#define	CAS14	0x7D	/* 0x7D	CAS Register 14 */
#define	CAS15	0x7E	/* 0x7E	CAS Register 15 */
#define	CAS16	0x7F	/* 0x7F	CAS Register 16 */
#define	PC1	0x80	/* 0x80	Port Configuration 1 */
#define	PC2	0x81	/* 0x81	Port Configuration 2 */
#define	PC3	0x82	/* 0x82	Port Configuration 3 */
#define	PC4	0x83	/* 0x83	Port Configuration 4 */
#define	PC5	0x84	/* 0x84	Port Configuration 5 */
#define	GPC1	0x85	/* 0x85	Global Port Configuration 1 */
#define	PC6	0x86	/* 0x86	Port Configuration 6 */
#define	CMDR2	0x87	/* 0x87	Command Register 2 */
#define	CMDR3	0x88	/* 0x88	Command Register 3 */
#define	CMDR4	0x89	/* 0x89	Command Register 4 */
#define	Res9	0x8A	/* 0x8A	Free Register 9 */
#define	CCR3	0x8B	/* 0x8B	Common Control Register 3 */
#define	CCR4	0x8C	/* 0x8C	Common Control Register 4 */
#define	CCR5	0x8D	/* 0x8D	Common Control Register 5 */
#define	MODE2	0x8E	/* 0x8E	Mode Register 2 */
#define	MODE3	0x8F	/* 0x8F	Mode Register 3 */
#define	RBC2	0x90	/* 0x90	Receive Byte Count Register 2 */
#define	RBC3	0x91	/* 0x91	Receive Byte Count Register 3 */
#define	GCM1	0x92	/* 0x92	Global Counter Mode 1 */
#define	GCM2	0x93	/* 0x93	Global Counter Mode 2 */
#define	GCM3	0x94	/* 0x94	Global Counter Mode 3 */
#define	GCM4	0x95	/* 0x95	Global Counter Mode 4 */
#define	GCM5	0x96	/* 0x96	Global Counter Mode 5 */
#define	GCM6	0x97	/* 0x97	Global Counter Mode 6 */
#define SIS2_1	0x98	/* 0x98 V1.2 : Signaling Status Register 2 */
#define GCM7	0x98	/* 0x98 V2.2 : Global Counter Mode 7 */
#define RSIS2_1	0x99	/* 0x99 V1.2 : Rx Signaling Status Register 2 */
#define GCM8	0x99	/* 0x99 V2.2 : Global Counter Mode 8 */
#define	SIS3	0x9A	/* 0x9A	Signaling Status Register 3 */
#define	RSIS3	0x9B	/* 0x9B	Receive Signaling Status Register 3 */
#define XFIFO2	0x9C	/* 0x9C/0x9D	Tx FIFO 2 */
#define RFIFO2	0x9C	/* 0x9C/0x9D	Rx FIFO 2 */
#define XFIFO3	0x9E	/* 0x9E/0x9F	Tx FIFO 3 */
#define RFIFO3	0x9E	/* 0x9E/0x9F	Rx FIFO 3 */
#define	TSEO	0xA0	/* 0xA0	Time Slot Even/Odd select */
#define	TSBS1	0xA1	/* 0xA1	Time Slot Bit select 1 */
#define	TSBS2	0xA2	/* 0xA2	Time Slot Bit select 2 */
#define	TSBS3	0xA3	/* 0xA3	Time Slot Bit select 3 */
#define	TSS2	0xA4	/* 0xA4	Time Slot select 2 */
#define	TSS3	0xA5	/* 0xA5	Time Slot select 3 */
#define	Res10	0xA6	/* 0xA6	Free Register 10 */
#define	Res11	0xA7	/* 0xA7	Free Register 11 */
#define	TPC0	0xA8	/* 0xA8	Test Pattern Control Register 0 */
#define	SIS2	0xA9	/* 0xA9	Signaling Status Register 2 (V2.2) */
#define	RSIS2	0xAA	/* 0xAA	Rx Signaling Status Register 2 (V2.2) */
#define	MFPI	0xAB	/* 0xAB	Multi Function Port Input Status */
#define	Res12	0xAC	/* 0xAC	Free Register 12 */
#define	Res13	0xAD	/* 0xAD	Free Register 13 */
#define	Res14	0xAE	/* 0xAE	Free Register 14 */
#define	GLC1	0xAF	/* 0xAF	Global Line Control Register 1 */
#define Res15	0xB0	/* 0xB0/0xEB Free Registers */
#define WID	0xEC	/* 0xEC	Identification Register */

#endif /* _PEF2256_H */
