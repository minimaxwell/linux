/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */


#define M_DRV_PEF2256_VERSION	"0.1"
#define M_DRV_PEF2256_AUTHOR	"CHANTELAUZE Jerome - April 2013"

#define MASTER_MODE 0
#define SLAVE_MODE  1

#define CHANNEL_PHASE_0 0
#define CHANNEL_PHASE_1 1
#define CHANNEL_PHASE_2 2
#define CHANNEL_PHASE_3 3

#define DATA_RATE_4M 4
#define DATA_RATE_8M 8

#define RX_TIMEOUT 500

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

	void *base_addr;
	int component_id;
	int mode;	/* MASTER or SLAVE */
	int board_type;
	int channel_phase;
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

	u8 ISR0;			/* ISR0 register */
	u8 ISR1;			/* ISR1 register */
};


/* Framer E1 registers */
union pef2256_Fifo {
	u8	XFIFO[sizeof(u16)];		/* Transmit FIFO */
	u8	RFIFO[sizeof(u16)];		/* Receive FIFO */
};

union pef2256_60 {
	unsigned char	DEC;	/* Disable Error Counter */
	unsigned char	RSA8;	/* Receive Sa8-Bit Regiter */
};

union pef2256_CAS {
	unsigned char	XS;	/* Transmit CAS Register */
	unsigned char	RS;	/* Receive CAS Regiter */
};

union pef2256_Dif1 {
	unsigned char	SIS2;	/* V1.2 : Signaling Status Register 2 */
	unsigned char	GCM7;	/* V2.2 : Global Counter Mode 7 */
};

union pef2256_Dif2 {
	unsigned char	RSIS2;	/* V1.2 : Rx Signaling Status Register 2 */
	unsigned char	GCM8;	/* V2.2 : Global Counter Mode 8 */
};

struct pef2256_regs {
	union pef2256_Fifo	FIFO;	/* 0x00/0x01	FIFO (Tx or rx) */
	unsigned char	CMDR;	/* 0x02	Command Register */
	unsigned char	MODE;	/* 0x03	Mode Register */
	unsigned char	RAH1;	/* 0x04	Receive Address High 1 */
	unsigned char	RAH2;	/* 0x05	Receive Address High 2 */
	unsigned char	RAL1;	/* 0x06	Receive Address Low 1 */
	unsigned char	RAL2;	/* 0x07	Receive Address Low 2 */
	unsigned char	IPC;	/* 0x08	Interrupt Port Configuration */
	unsigned char	CCR1;	/* 0x09	Common Configuration Register 1 */
	unsigned char	CCR2;	/* 0x0A	Common Configuration Register 2 */
	unsigned char	Res1;	/* 0x0B	Free Register 1 */
	unsigned char	RTR1;	/* 0x0C	Receive Time Slot Register 1 */
	unsigned char	RTR2;	/* 0x0D	Receive Time Slot Register 2 */
	unsigned char	RTR3;	/* 0x0E	Receive Time Slot Register 3 */
	unsigned char	RTR4;	/* 0x0F	Receive Time Slot Register 4 */
	unsigned char	TTR1;	/* 0x10	Transmit Time Slot Register 1 */
	unsigned char	TTR2;	/* 0x11	Transmit Time Slot Register 2 */
	unsigned char	TTR3;	/* 0x12	Transmit Time Slot Register 3 */
	unsigned char	TTR4;	/* 0x13	Transmit Time Slot Register 4 */
	unsigned char	IMR0;	/* 0x14	Interrupt Mask Register 0 */
	unsigned char	IMR1;	/* 0x15	Interrupt Mask Register 1 */
	unsigned char	IMR2;	/* 0x16	Interrupt Mask Register 2 */
	unsigned char	IMR3;	/* 0x17	Interrupt Mask Register 3 */
	unsigned char	IMR4;	/* 0x18	Interrupt Mask Register 4 */
	unsigned char	IMR5;	/* 0x19	Interrupt Mask Register 5 */
	unsigned char	Res2;	/* 0x1A	Free Register 2 */
	unsigned char	IERR;	/* 0x1B	Single Bit Error Insertion Register */
	unsigned char	FMR0;	/* 0x1C	Framer Mode Register 0 */
	unsigned char	FMR1;	/* 0x1D	Framer Mode Register 1 */
	unsigned char	FMR2;	/* 0x1E	Framer Mode Register 2 */
	unsigned char	LOOP;	/* 0x1F	Channel Loop-Back */
	unsigned char	XSW;	/* 0x20	Transmit Service Word */
	unsigned char	XSP;	/* 0x21	Transmit Spare Bits */
	unsigned char	XC0;	/* 0x22	Transmit Control 0 */
	unsigned char	XC1;	/* 0x23	Transmit Control 1 */
	unsigned char	RC0;	/* 0x24	Receive Control 0 */
	unsigned char	RC1;	/* 0x25	Receive Control 1 */
	unsigned char	XPM0;	/* 0x26	Transmit Pulse Mask 0 */
	unsigned char	XPM1;	/* 0x27	Transmit Pulse Mask 1 */
	unsigned char	XPM2;	/* 0x28	Transmit Pulse Mask 2 */
	unsigned char	TSWM;	/* 0x29	Transparent Service Word Mask */
	unsigned char	Res3;	/* 0x2A	Free Register 3 */
	unsigned char	IDLE;	/* 0x2B	Idle Channel Code */
	unsigned char	XSA4;	/* 0x2C	Transmit Sa4-Bit Register */
	unsigned char	XSA5;	/* 0x2D	Transmit Sa5-Bit Register */
	unsigned char	XSA6;	/* 0x2E	Transmit Sa6-Bit Register */
	unsigned char	XSA7;	/* 0x2F	Transmit Sa7-Bit Register */
	unsigned char	XSA8;	/* 0x30	Transmit Sa8-Bit Register */
	unsigned char	FMR3;	/* 0x31	Framer Mode Register 3 */
	unsigned char	ICB1;	/* 0x32	Idle Channel Register 1 */
	unsigned char	ICB2;	/* 0x33	Idle Channel Register 2 */
	unsigned char	ICB3;	/* 0x34	Idle Channel Register 3 */
	unsigned char	ICB4;	/* 0x35	Idle Channel Register 4 */
	unsigned char	LIM0;	/* 0x36	Line Interface Mode 0 */
	unsigned char	LIM1;	/* 0x37	Line Interface Mode 1 */
	unsigned char	PCD;	/* 0x38	Pulse Count Detection */
	unsigned char	PCR;	/* 0x39	Pulse Count Recovery */
	unsigned char	LIM2;	/* 0x3A	Line Interface Mode 2 */
	unsigned char	LCR1;	/* 0x3B	Loop Code Register 1 */
	unsigned char	LCR2;	/* 0x3C	Loop Code Register 2 */
	unsigned char	LCR3;	/* 0x3D	Loop Code Register 3 */
	unsigned char	SIC1;	/* 0x3E	System Interface Control 1 */
	unsigned char	SIC2;	/* 0x3F	System Interface Control 2 */
	unsigned char	SIC3;	/* 0x40	System Interface Control 3 */
	unsigned char	Res4;	/* 0x41	Free Register 4 */
	unsigned char	Res5;	/* 0x42	Free Register 5 */
	unsigned char	Res6;	/* 0x43	Free Register 6 */
	unsigned char	CMR1;	/* 0x44	Clock Mode Register 1 */
	unsigned char	CMR2;	/* 0x45	Clock Mode Register 2 */
	unsigned char	GCR;	/* 0x46	Global Configuration Register */
	unsigned char	ESM;	/* 0x47	Errored Second Mask */
	unsigned char	CMR3;	/* 0x48	Clock Mode Register 3 en V2.2 */
	unsigned char	RBD;	/* 0x49	Receive Buffer Delay */
	unsigned char	VSTR;	/* 0x4A	Version Status Regiter */
	unsigned char	RES;	/* 0x4B	Receive Equalizer Status */
	unsigned char	FRS0;	/* 0x4C	Framer Receive Status 0 */
	unsigned char	FRS1;	/* 0x4D	Framer Receive Status 1 */
	unsigned char	RSW;	/* 0x4E	Receive Service Word */
	unsigned char	RSP;	/* 0x4F	Receive Spare Bits */
	unsigned short	FEC;	/* 0x50/0x51 Framing Error Counter */
	unsigned short	CVC;	/* 0x52/0x53 Code Violation Counter */
	unsigned short	CEC1;	/* 0x54/0x55 CRC Error Counter 1 */
	unsigned short	EBC;	/* 0x56/0x57 E-Bit Error Counter */
	unsigned short	CEC2;	/* 0x58/0x59 CRC Error Counter 2 */
	unsigned short	CEC3;	/* 0x5A/0x5B CRC Error Counter 3 */
	unsigned char	RSA4;	/* 0x5C	Receive Sa4-Bit Register */
	unsigned char	RSA5;	/* 0x5D	Receive Sa5-Bit Register */
	unsigned char	RSA6;	/* 0x5E	Receive Sa6-Bit Register */
	unsigned char	RSA7;	/* 0x5F	Receive Sa7-Bit Register */
	union pef2256_60	Reg60;	/* 0x60	Common Register */
	unsigned char	RSA6S;	/* 0x61	Receive Sa6-Bit Status Register */
	unsigned char	RSP1;	/* 0x62	Receive Signaling Pointer 1 */
	unsigned char	RSP2;	/* 0x63	Receive Signaling Pointer 2 */
	unsigned char	SIS;	/* 0x64	Signaling Status Register */
	unsigned char	RSIS;	/* 0x65	Receive Signaling Status Register */
	unsigned char	RBCL;	/* 0x66	Receive Byte Control */
	unsigned char	RBCH;	/* 0x67	Receive Byte Control */
	unsigned char	ISR0;	/* 0x68	Interrupt Status Register 0 */
	unsigned char	ISR1;	/* 0x69	Interrupt Status Register 1 */
	unsigned char	ISR2;	/* 0x6A	Interrupt Status Register 2 */
	unsigned char	ISR3;	/* 0x6B	Interrupt Status Register 3 */
	unsigned char	ISR4;	/* 0x6C	Interrupt Status Register 4 */
	unsigned char	ISR5;	/* 0x6D	Interrupt Status Register 5 */
	unsigned char	GIS;	/* 0x6E	Global Interrupt Status */
	unsigned char	Res8;	/* 0x6F	Free Register 8 */
	union pef2256_CAS	CAS1;	/* 0x70	CAS Register 1 */
	union pef2256_CAS	CAS2;	/* 0x71	CAS Register 2 */
	union pef2256_CAS	CAS3;	/* 0x72	CAS Register 3 */
	union pef2256_CAS	CAS4;	/* 0x73	CAS Register 4 */
	union pef2256_CAS	CAS5;	/* 0x74	CAS Register 5 */
	union pef2256_CAS	CAS6;	/* 0x75	CAS Register 6 */
	union pef2256_CAS	CAS7;	/* 0x76	CAS Register 7 */
	union pef2256_CAS	CAS8;	/* 0x77	CAS Register 8 */
	union pef2256_CAS	CAS9;	/* 0x78	CAS Register 9 */
	union pef2256_CAS	CAS10;	/* 0x79	CAS Register 10 */
	union pef2256_CAS	CAS11;	/* 0x7A	CAS Register 11 */
	union pef2256_CAS	CAS12;	/* 0x7B	CAS Register 12 */
	union pef2256_CAS	CAS13;	/* 0x7C	CAS Register 13 */
	union pef2256_CAS	CAS14;	/* 0x7D	CAS Register 14 */
	union pef2256_CAS	CAS15;	/* 0x7E	CAS Register 15 */
	union pef2256_CAS	CAS16;	/* 0x7F	CAS Register 16 */
	unsigned char	PC1;	/* 0x80	Port Configuration 1 */
	unsigned char	PC2;	/* 0x81	Port Configuration 2 */
	unsigned char	PC3;	/* 0x82	Port Configuration 3 */
	unsigned char	PC4;	/* 0x83	Port Configuration 4 */
	unsigned char	PC5;	/* 0x84	Port Configuration 5 */
	unsigned char	GPC1;	/* 0x85	Global Port Configuration 1 */
	unsigned char	PC6;	/* 0x86	Port Configuration 6 */
	unsigned char	CMDR2;	/* 0x87	Command Register 2 */
	unsigned char	CMDR3;	/* 0x88	Command Register 3 */
	unsigned char	CMDR4;	/* 0x89	Command Register 4 */
	unsigned char	Res9;	/* 0x8A	Free Register 9 */
	unsigned char	CCR3;	/* 0x8B	Common Control Register 3 */
	unsigned char	CCR4;	/* 0x8C	Common Control Register 4 */
	unsigned char	CCR5;	/* 0x8D	Common Control Register 5 */
	unsigned char	MODE2;	/* 0x8E	Mode Register 2 */
	unsigned char	MODE3;	/* 0x8F	Mode Register 3 */
	unsigned char	RBC2;	/* 0x90	Receive Byte Count Register 2 */
	unsigned char	RBC3;	/* 0x91	Receive Byte Count Register 3 */
	unsigned char	GCM1;	/* 0x92	Global Counter Mode 1 */
	unsigned char	GCM2;	/* 0x93	Global Counter Mode 2 */
	unsigned char	GCM3;	/* 0x94	Global Counter Mode 3 */
	unsigned char	GCM4;	/* 0x95	Global Counter Mode 4 */
	unsigned char	GCM5;	/* 0x96	Global Counter Mode 5 */
	unsigned char	GCM6;	/* 0x97	Global Counter Mode 6 */
	union pef2256_Dif1	Dif1;	/* 0x98	SIS2 en V1.2, GCM7 en V2.2 */
	union pef2256_Dif2	Dif2;	/* 0x99	RSIS2 en V1.2, GCM8 en V2.2 */
	unsigned char	SIS3;	/* 0x9A	Signaling Status Register 3 */
	unsigned char	RSIS3;	/* 0x9B	Receive Signaling Status Register 3 */
	union pef2256_Fifo	FIFO2;	/* 0x9C/0x9D FIFO 2 (Tx or rx) */
	union pef2256_Fifo	FIFO3;	/* 0x9E/0x9F FIFO 3 (Tx or rx) */
	unsigned char	TSEO;	/* 0xA0	Time Slot Even/Odd select */
	unsigned char	TSBS1;	/* 0xA1	Time Slot Bit select 1 */
	unsigned char	TSBS2;	/* 0xA2	Time Slot Bit select 2 */
	unsigned char	TSBS3;	/* 0xA3	Time Slot Bit select 3 */
	unsigned char	TSS2;	/* 0xA4	Time Slot select 2 */
	unsigned char	TSS3;	/* 0xA5	Time Slot select 3 */
	unsigned char	Res10;	/* 0xA6	Free Register 10 */
	unsigned char	Res11;	/* 0xA7	Free Register 11 */
	unsigned char	TPC0;	/* 0xA8	Test Pattern Control Register 0 */
	unsigned char	SIS2;	/* 0xA9	Signaling Status Register 2 (V2.2) */
	unsigned char	RSIS2;	/* 0xAA	Rx Signaling Status Register 2 (V2.2) */
	unsigned char	MFPI;	/* 0xAB	Multi Function Port Input Status */
	unsigned char	Res12;	/* 0xAC	Free Register 12 */
	unsigned char	Res13;	/* 0xAD	Free Register 13 */
	unsigned char	Res14;	/* 0xAE	Free Register 14 */
	unsigned char	GLC1;	/* 0xAF	Global Line Control Register 1 */
	unsigned char	Res[0xEB-0xAF];	/* 0xB0/0xEB Free Registers */
	unsigned char	WID;	/* 0xEC	Identification Register */
};


