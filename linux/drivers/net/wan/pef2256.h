/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/cache.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/hdlc.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/etherdevice.h>

#define M_DRV_PEF2256_VERSION	"0.1"
#define M_DRV_PEF2256_AUTHOR	"CHANTELAUZE Jerome - Avril 2013"

#define MASTER_MODE 0
#define SLAVE_MODE  1

#define BOARD_MCR3000_1G	0
#define BOARD_MCR3000_2G	1
#define BOARD_MIAE 		2

#define CHANNEL_PHASE_0 0
#define CHANNEL_PHASE_1 1
#define CHANNEL_PHASE_2 2
#define CHANNEL_PHASE_3 3

#define DATA_RATE_4M 4
#define DATA_RATE_8M 8

/* A preciser */
#define RX_TIMEOUT 500

/* Configuration d'une fréquence pour la trame enregistrement */
/* Nombre de paramètres fréquence et centres */
#define M_DRV_E1_NB_FREQ_ENREG  30      /* nombre de fréquence par trame enregistrement */
#define M_DRV_E1_NB_PARAM_FREQ  6
#define M_DRV_E1_NB_PARAM_CNTR  8

/* structures sur les canaux HDLC */
typedef struct		/* structure sur configuration canal HDLC */
{
	unsigned char	mMode;		/* mode de fonctionnement du canal */
	unsigned char	mIdle;		/* définition octet intertrame */
	unsigned char	mTS;		/* TS émission et réception */
	unsigned char	mRec;		/* mode de réception */
} tDRVE1CfgCanal;

typedef struct		/* structure sur paramètres d'une fréquence */
{
	/* valeur de la fréquence */
	unsigned char	mValeur[M_DRV_E1_NB_PARAM_FREQ];
	/* affectation des centres pour la fréquence */
	unsigned char	mCentre[M_DRV_E1_NB_PARAM_CNTR];
} tDRVE1ParamFreq;

#define M_DRV_E1_ADES_INIT	0xFF	/* initialisation en broadcast (mAdes) */
#define M_DRV_E1_CTL_INIT	0x00	/* initialisation du champ mCtl */
#define M_DRV_E1_CTL_NUM_MASK	0x0E	/* bits 1, 2 et 3 du champ mCtl */
#define M_DRV_E1_CTL_NUM_PLUS	(1 << 1)	/* incrément du numéro de trame */

typedef struct		/* structure sur datas trame enregistrement */
{
	unsigned char	mAdes;		/* adresse destinataire */
	unsigned char	mCtl;		/* type et numéro de trame */
	// données sur l'ensemble des fréquences
	tDRVE1ParamFreq	mFreq[M_DRV_E1_NB_FREQ_ENREG];
} tDRVE1TrameEnreg;

#define M_DRV_E1_MAJ_INIT	0		/* initialisation mise à jour */
#define M_DRV_E1_MAJ_FREQ	(1 << 0)	/* mise à jour valeur fréquence */
#define M_DRV_E1_MAJ_CNTR	(1 << 1)	/* mise à jour affectation centre */
#define M_DRV_E1_MAJ_OK		(M_DRV_E1_MAJ_FREQ + M_DRV_E1_MAJ_CNTR)

typedef struct		/* structure sur modification d'une fréquence */
{
	unsigned char	mIdent;		/* numéro de la fréquence */
	unsigned char	mMaj;		/* suivi mise à jour fréquence */
	tDRVE1ParamFreq	mParam;		/* paramètre de la fréquence */
} tDRVE1MajFreq;

typedef struct		/* structure sur canal HDLC (Enregistrement) */
{
	tDRVE1MajFreq		mMajFq;		/* mise à jour paramètres fréquence */
	tDRVE1MajFreq		mBusyFq;	/* stockage paramètres fréquence en cours de transfert */
	unsigned short		mIxTrft;	/* index transfert émission */
	tDRVE1TrameEnreg	mTrame;		/* trame enregistrement à émettre */
} tDRVE1EmTrEnreg;

/* gestion trame émission HDLC */
typedef union
{
	unsigned char		mOctet[512];	/* longueur maximale */
	tDRVE1EmTrEnreg		mTrEnreg;
} tDRVE1TrameEm;

typedef struct		/* structure sur lecture trame enregistrement */
{
	unsigned short		mIxLct;		/* index de lecture trame */
	tDRVE1TrameEnreg	mTrame;		/* trame enregistrement à lire */
} tDRVE1LctTrEnreg;

/* gestion trame réception HDLC */
typedef union
{
	unsigned char		mOctet[512];	/* longueur maximale */
	tDRVE1LctTrEnreg	mTrEnreg;
} tDRVE1TrameRec;

#define M_DRV_E1_TAILLE_FIFO	32	/* taille fifo HDLC pour transfert de datas */



typedef enum
{
	E_DRV_E1_VERSION_UNDEF = 0,
	E_DRV_E1_VERSION_1_2 = 0x12,
	E_DRV_E1_VERSION_2_1 = 0x21,
	E_DRV_E1_VERSION_2_2 = 0x22,
} tDRVE1ListeVersion;


/* déclaration de l'identification du composant */
#define M_DRV_E1_WID_IDENT_1		0x03
#define M_DRV_E1_WID_IDENT_1_2		0x03
#define M_DRV_E1_WID_IDENT_2		0xC0
#define M_DRV_E1_WID_IDENT_2_1		0x00
#define M_DRV_E1_WID_IDENT_2_2		0x40


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


/* déclaration des registres du trameur E1 */
typedef union
{
	u8	mXFIFO[sizeof(u16)];		/* Transmit FIFO */
	u8	mRFIFO[sizeof(u16)];		/* Receive FIFO */
} pef2256_Fifo;

typedef union
{
	unsigned char	mDEC;		/* Disable Error Counter */
	unsigned char	mRSA8;		/* Receive Sa8-Bit Regiter */
} pef2256_60;

typedef union
{
	unsigned char	mXS;		/* Transmit CAS Register */
	unsigned char	mRS;		/* Receive CAS Regiter */
} pef2256_CAS;

typedef union
{
	unsigned char	mSIS2;		/* V1.2 : Signaling Status Register 2 */
	unsigned char	mGCM7;		/* V2.2 : Global Counter Mode 7 */
} pef2256_Dif1;
typedef union

{
	unsigned char	mRSIS2;		/* V1.2 : Receive Signaling Status Register 2 */
	unsigned char	mGCM8;		/* V2.2 : Global Counter Mode 8 */
} pef2256_Dif2;

typedef struct
{
	pef2256_Fifo	mFIFO;		/* 0x00/0x01	FIFO (Transmit or receive) */
	unsigned char	mCMDR;		/* 0x02		Command Register */
	unsigned char	mMODE;		/* 0x03		Mode Register */
	unsigned char	mRAH1;		/* 0x04		Receive Address High 1 */
	unsigned char	mRAH2;		/* 0x05		Receive Address High 2 */
	unsigned char	mRAL1;		/* 0x06		Receive Address Low 1 */
	unsigned char	mRAL2;		/* 0x07		Receive Address Low 2 */
	unsigned char	mIPC;		/* 0x08		Interrupt Port Configuration */
	unsigned char	mCCR1;		/* 0x09		Common Configuration Register 1 */
	unsigned char	mCCR2;		/* 0x0A		Common Configuration Register 2 */
	unsigned char	mRes1;		/* 0x0B		Free Register 1 */
	unsigned char	mRTR1;		/* 0x0C		Receive Time Slot Register 1 */
	unsigned char	mRTR2;		/* 0x0D		Receive Time Slot Register 2 */
	unsigned char	mRTR3;		/* 0x0E		Receive Time Slot Register 3 */
	unsigned char	mRTR4;		/* 0x0F		Receive Time Slot Register 4 */
	unsigned char	mTTR1;		/* 0x10		Transmit Time Slot Register 1 */
	unsigned char	mTTR2;		/* 0x11		Transmit Time Slot Register 2 */
	unsigned char	mTTR3;		/* 0x12		Transmit Time Slot Register 3 */
	unsigned char	mTTR4;		/* 0x13		Transmit Time Slot Register 4 */
	unsigned char	mIMR0;		/* 0x14		Interrupt Mask Register 0 */
	unsigned char	mIMR1;		/* 0x15		Interrupt Mask Register 1 */
	unsigned char	mIMR2;		/* 0x16		Interrupt Mask Register 2 */
	unsigned char	mIMR3;		/* 0x17		Interrupt Mask Register 3 */
	unsigned char	mIMR4;		/* 0x18		Interrupt Mask Register 4 */
	unsigned char	mIMR5;		/* 0x19		Interrupt Mask Register 5 */
	unsigned char	mRes2;		/* 0x1A		Free Register 2 */
	unsigned char	mIERR;		/* 0x1B		Single Bit Error Insertion Register */
	unsigned char	mFMR0;		/* 0x1C		Framer Mode Register 0 */
	unsigned char	mFMR1;		/* 0x1D		Framer Mode Register 1 */
	unsigned char	mFMR2;		/* 0x1E		Framer Mode Register 2 */
	unsigned char	mLOOP;		/* 0x1F		Channel Loop-Back */
	unsigned char	mXSW;		/* 0x20		Transmit Service Word */
	unsigned char	mXSP;		/* 0x21		Transmit Spare Bits */
	unsigned char	mXC0;		/* 0x22		Transmit Control 0 */
	unsigned char	mXC1;		/* 0x23		Transmit Control 1 */
	unsigned char	mRC0;		/* 0x24		Receive Control 0 */
	unsigned char	mRC1;		/* 0x25		Receive Control 1 */
	unsigned char	mXPM0;		/* 0x26		Transmit Pulse Mask 0 */
	unsigned char	mXPM1;		/* 0x27		Transmit Pulse Mask 1 */
	unsigned char	mXPM2;		/* 0x28		Transmit Pulse Mask 2 */
	unsigned char	mTSWM;		/* 0x29		Transparent Service Word Mask */
	unsigned char	mRes3;		/* 0x2A		Free Register 3 */
	unsigned char	mIDLE;		/* 0x2B		Idle Channel Code */
	unsigned char	mXSA4;		/* 0x2C		Transmit Sa4-Bit Register */
	unsigned char	mXSA5;		/* 0x2D		Transmit Sa5-Bit Register */
	unsigned char	mXSA6;		/* 0x2E		Transmit Sa6-Bit Register */
	unsigned char	mXSA7;		/* 0x2F		Transmit Sa7-Bit Register */
	unsigned char	mXSA8;		/* 0x30		Transmit Sa8-Bit Register */
	unsigned char	mFMR3;		/* 0x31		Framer Mode Register 3 */
	unsigned char	mICB1;		/* 0x32		Idle Channel Register 1 */
	unsigned char	mICB2;		/* 0x33		Idle Channel Register 2 */
	unsigned char	mICB3;		/* 0x34		Idle Channel Register 3 */
	unsigned char	mICB4;		/* 0x35		Idle Channel Register 4 */
	unsigned char	mLIM0;		/* 0x36		Line Interface Mode 0 */
	unsigned char	mLIM1;		/* 0x37		Line Interface Mode 1 */
	unsigned char	mPCD;		/* 0x38		Pulse Count Detection */
	unsigned char	mPCR;		/* 0x39		Pulse Count Recovery */
	unsigned char	mLIM2;		/* 0x3A		Line Interface Mode 2 */
	unsigned char	mLCR1;		/* 0x3B		Loop Code Register 1 */
	unsigned char	mLCR2;		/* 0x3C		Loop Code Register 2 */
	unsigned char	mLCR3;		/* 0x3D		Loop Code Register 3 */
	unsigned char	mSIC1;		/* 0x3E		System Interface Control 1 */
	unsigned char	mSIC2;		/* 0x3F		System Interface Control 2 */
	unsigned char	mSIC3;		/* 0x40		System Interface Control 3 */
	unsigned char	mRes4;		/* 0x41		Free Register 4 */
	unsigned char	mRes5;		/* 0x42		Free Register 5 */
	unsigned char	mRes6;		/* 0x43		Free Register 6 */
	unsigned char	mCMR1;		/* 0x44		Clock Mode Register 1 */
	unsigned char	mCMR2;		/* 0x45		Clock Mode Register 2 */
	unsigned char	mGCR;		/* 0x46		Global Configuration Register */
	unsigned char	mESM;		/* 0x47		Errored Second Mask */
/*	unsigned char	mRes7;	*/	/* 0x48		Free Register 7 en V1.2 */
	unsigned char	mCMR3;		/* 0x48		Clock Mode Register 3 en V2.2 */
	unsigned char	mRBD;		/* 0x49		Receive Buffer Delay */
	unsigned char	mVSTR;		/* 0x4A		Version Status Regiter */
	unsigned char	mRES;		/* 0x4B		Receive Equalizer Status */
	unsigned char	mFRS0;		/* 0x4C		Framer Receive Status 0 */
	unsigned char	mFRS1;		/* 0x4D		Framer Receive Status 1 */
	unsigned char	mRSW;		/* 0x4E		Receive Service Word */
	unsigned char	mRSP;		/* 0x4F		Receive Spare Bits */
	unsigned short	mFEC;		/* 0x50/0x51	Framing Error Counter */
	unsigned short	mCVC;		/* 0x52/0x53	Code Violation Counter */
	unsigned short	mCEC1;		/* 0x54/0x55	CRC Error Counter 1 */
	unsigned short	mEBC;		/* 0x56/0x57	E-Bit Error Counter */
	unsigned short	mCEC2;		/* 0x58/0x59	CRC Error Counter 2 */
	unsigned short	mCEC3;		/* 0x5A/0x5B	CRC Error Counter 3 */
	unsigned char	mRSA4;		/* 0x5C		Receive Sa4-Bit Register */
	unsigned char	mRSA5;		/* 0x5D		Receive Sa5-Bit Register */
	unsigned char	mRSA6;		/* 0x5E		Receive Sa6-Bit Register */
	unsigned char	mRSA7;		/* 0x5F		Receive Sa7-Bit Register */
	pef2256_60	mReg60;		/* 0x60		Common Register */
	unsigned char	mRSA6S;		/* 0x61		Receive Sa6-Bit Status Register */
	unsigned char	mRSP1;		/* 0x62		Receive Signaling Pointer 1 */
	unsigned char	mRSP2;		/* 0x63		Receive Signaling Pointer 2 */
	unsigned char	mSIS;		/* 0x64		Signaling Status Register */
	unsigned char	mRSIS;		/* 0x65		Receive Signaling Status Register */
	unsigned char	mRBCL;		/* 0x66		Receive Byte Control */
	unsigned char	mRBCH;		/* 0x67		Receive Byte Control */
	unsigned char	mISR0;		/* 0x68		Interrupt Status Register 0 */
	unsigned char	mISR1;		/* 0x69		Interrupt Status Register 1 */
	unsigned char	mISR2;		/* 0x6A		Interrupt Status Register 2 */
	unsigned char	mISR3;		/* 0x6B		Interrupt Status Register 3 */
	unsigned char	mISR4;		/* 0x6C		Interrupt Status Register 4 */
	unsigned char	mISR5;		/* 0x6D		Interrupt Status Register 5 */
	unsigned char	mGIS;		/* 0x6E		Global Interrupt Status */
	unsigned char	mRes8;		/* 0x6F		Free Register 8 */
	pef2256_CAS	mCAS1;		/* 0x70		CAS Register 1 */
	pef2256_CAS	mCAS2;		/* 0x71		CAS Register 2 */
	pef2256_CAS	mCAS3;		/* 0x72		CAS Register 3 */
	pef2256_CAS	mCAS4;		/* 0x73		CAS Register 4 */
	pef2256_CAS	mCAS5;		/* 0x74		CAS Register 5 */
	pef2256_CAS	mCAS6;		/* 0x75		CAS Register 6 */
	pef2256_CAS	mCAS7;		/* 0x76		CAS Register 7 */
	pef2256_CAS	mCAS8;		/* 0x77		CAS Register 8 */
	pef2256_CAS	mCAS9;		/* 0x78		CAS Register 9 */
	pef2256_CAS	mCAS10;		/* 0x79		CAS Register 10 */
	pef2256_CAS	mCAS11;		/* 0x7A		CAS Register 11 */
	pef2256_CAS	mCAS12;		/* 0x7B		CAS Register 12 */
	pef2256_CAS	mCAS13;		/* 0x7C		CAS Register 13 */
	pef2256_CAS	mCAS14;		/* 0x7D		CAS Register 14 */
	pef2256_CAS	mCAS15;		/* 0x7E		CAS Register 15 */
	pef2256_CAS	mCAS16;		/* 0x7F		CAS Register 16 */
	unsigned char	mPC1;		/* 0x80		Port Configuration 1 */
	unsigned char	mPC2;		/* 0x81		Port Configuration 2 */
	unsigned char	mPC3;		/* 0x82		Port Configuration 3 */
	unsigned char	mPC4;		/* 0x83		Port Configuration 4 */
	unsigned char	mPC5;		/* 0x84		Port Configuration 5 */
	unsigned char	mGPC1;		/* 0x85		Global Port Configuration 1 */
	unsigned char	mPC6;		/* 0x86		Port Configuration 6 */
	unsigned char	mCMDR2;		/* 0x87		Command Register 2 */
	unsigned char	mCMDR3;		/* 0x88		Command Register 3 */
	unsigned char	mCMDR4;		/* 0x89		Command Register 4 */
	unsigned char	mRes9;		/* 0x8A		Free Register 9 */
	unsigned char	mCCR3;		/* 0x8B		Common Control Register 3 */
	unsigned char	mCCR4;		/* 0x8C		Common Control Register 4 */
	unsigned char	mCCR5;		/* 0x8D		Common Control Register 5 */
	unsigned char	mMODE2;		/* 0x8E		Mode Register 2 */
	unsigned char	mMODE3;		/* 0x8F		Mode Register 3 */
	unsigned char	mRBC2;		/* 0x90		Receive Byte Count Register 2 */
	unsigned char	mRBC3;		/* 0x91		Receive Byte Count Register 3 */
	unsigned char	mGCM1;		/* 0x92		Global Counter Mode 1 */
	unsigned char	mGCM2;		/* 0x93		Global Counter Mode 2 */
	unsigned char	mGCM3;		/* 0x94		Global Counter Mode 3 */
	unsigned char	mGCM4;		/* 0x95		Global Counter Mode 4 */
	unsigned char	mGCM5;		/* 0x96		Global Counter Mode 5 */
	unsigned char	mGCM6;		/* 0x97		Global Counter Mode 6 */
	pef2256_Dif1	mDif1;		/* 0x98		SIS2 en V1.2, GCM7 en V2.2 */
	pef2256_Dif2	mDif2;		/* 0x99		RSIS2 en V1.2, GCM8 en V2.2 */
	unsigned char	mSIS3;		/* 0x9A		Signaling Status Register 3 */
	unsigned char	mRSIS3;		/* 0x9B		Receive Signaling Status Register 3 */
	pef2256_Fifo	mFIFO2;		/* 0x9C/0x9D	FIFO 2 (Transmit or receive) */
	pef2256_Fifo	mFIFO3;		/* 0x9E/0x9F	FIFO 3 (Transmit or receive) */
	unsigned char	mTSEO;		/* 0xA0		Time Slot Even/Odd select */
	unsigned char	mTSBS1;		/* 0xA1		Time Slot Bit select 1 */
	unsigned char	mTSBS2;		/* 0xA2		Time Slot Bit select 2 */
	unsigned char	mTSBS3;		/* 0xA3		Time Slot Bit select 3 */
	unsigned char	mTSS2;		/* 0xA4		Time Slot select 2 */
	unsigned char	mTSS3;		/* 0xA5		Time Slot select 3 */
	unsigned char	mRes10;		/* 0xA6		Free Register 10 */
	unsigned char	mRes11;		/* 0xA7		Free Register 11 */
	unsigned char	mTPC0;		/* 0xA8		Test Pattern Control Register 0 */
	unsigned char	mSIS2;		/* 0xA9		Signaling Status Register 2 en V2.2 */
	unsigned char	mRSIS2;		/* 0xAA		Receive Signaling Status Register 2 en V2.2 */
	unsigned char	mMFPI;		/* 0xAB		Multi Function Port Input Status */
	unsigned char	mRes12;		/* 0xAC		Free Register 12 */
	unsigned char	mRes13;		/* 0xAD		Free Register 13 */
	unsigned char	mRes14;		/* 0xAE		Free Register 14 */
	unsigned char	mGLC1;		/* 0xAF		Global Line Control Register 1 */
	unsigned char	mRes[0xEB-0xAF];	/* 0xB0/0xEB	Free Registers */
	unsigned char	mWID;		/* 0xEC		Identification Register */
} pef2256_regs;


