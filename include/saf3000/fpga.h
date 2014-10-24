#ifndef __FPGA_H__
#define __FPGA_H__

/* structure sur les registres du PFGA RADIO */
struct fpga {
	u16	mRDMFG1;	/* 0x00  Mode de fctmnt general */
	u16	mRDMFG2;	/* 0x01  Mode de fctmnt general */
	u16	mRSINT10;	/* 0x02  Statut it 10 */
	u16	mRSINT11;	/* 0x03  Statut it 11 */
	u16	mRSINT12;	/* 0x04  Statut it 12 */
	u16	mRSINT13;	/* 0x05  Statut it 13 */
	u16	mRSINT14;	/* 0x06  Statut it 14 */
	u16	mRSINT15;	/* 0x07  Statut it 15 */
	u16	mRSINT16;	/* 0x08  Statut it 16 */
	u16	mRSINT17;	/* 0x09  Statut it 17 */
	u16	mRSINT18;	/* 0x0A  Statut it 18 */
	u16	mRSINT19;	/* 0x0B  Statut it 19 */
	u16	mRSINT20;	/* 0x0C  Statut it 20 */
	u16	mRSINT21;	/* 0x0D  Statut it 21 */
	u16	mRSINT22;	/* 0x0E  Statut it 22 */
	u16	mRSINT23;	/* 0x0F  Statut it 23 */
	u16	mRSINT24;	/* 0x10  Statut it 24 */
	u16	mRSINT25;	/* 0x11  Statut it 25 */
	u16	mRESV_1;	/* 0x12  reserve ou espion1 */
	u16	mRESV_2;	/* 0x13  reserve ou espion2 */
	u16	mRPME1_1;	/* 0x14  Maitre / Eleve 0 a 2 Mod 1 */
	u16	mRPME1_2;	/* 0x15  Maitre / Eleve 3 a 5 Mod 1 */
	u16	mRPME1_3;	/* 0x16  Maitre / Eleve 6 a 8 Mod 1 */
	u16	mRPME1_4;	/* 0x17  Maitre / Eleve 9 a 11 Mod 1 */
	u16	mRPME1_5;	/* 0x18  Maitre / Eleve 12 a 14 Mod 1 */
	u16	mRPME1_6;	/* 0x19  Maitre / Eleve 15 a 17 Mod 1 */
	u16	mRPME1_7;	/* 0x1A  Maitre / Eleve 18 a 20 Mod 1 */
	u16	mRPME1_8;	/* 0x1B  Maitre / Eleve 21 a 23 Mod 1 */
	u16	mRPME1_9;	/* 0x1C  Maitre / Eleve 24 a 26 Mod 1 */
	u16	mRPME1_10;	/* 0x1D  Maitre / Eleve 27 a 29 Mod 1 */
	u16	mRPME1_11;	/* 0x1E  Maitre / Eleve 30 a 31 Mod 1 */
	u16	mRPME2_1;	/* 0x1F  Maitre / Eleve 0 a 2 Mod 2 */
	u16	mRPME2_2;	/* 0x20  Maitre / Eleve 3 a 5 Mod 2 */
	u16	mRPME2_3;	/* 0x21  Maitre / Eleve 6 a 8 Mod 2 */
	u16	mRPME2_4;	/* 0x22  Maitre / Eleve 9 a 11 Mod 2 */
	u16	mRPME2_5;	/* 0x23  Maitre / Eleve 12 a 14 Mod 2 */
	u16	mRPME2_6;	/* 0x24  Maitre / Eleve 15 a 17 Mod 2 */
	u16	mRPME2_7;	/* 0x25  Maitre / Eleve 18 a 20 Mod 2 */
	u16	mRPME2_8;	/* 0x26  Maitre / Eleve 21 a 23 Mod 2 */
	u16	mRPME2_9;	/* 0x27  Maitre / Eleve 24 a 26 Mod 2 */
	u16	mRPME2_10;	/* 0x28  Maitre / Eleve 27 a 29 Mod 2 */
	u16	mRPME2_11;	/* 0x29  Maitre / Eleve 30 a 31 Mod 2 */
	u16	mRPME3_1;	/* 0x2A  Maitre / Eleve 0 a 2 Mod 3 */
	u16	mRPME3_2;	/* 0x2B  Maitre / Eleve 3 a 5 Mod 3 */
	u16	mRPME3_3;	/* 0x2C  Maitre / Eleve 6 a 8 Mod 3 */
	u16	mRPME3_4;	/* 0x2D  Maitre / Eleve 9 a 11 Mod 3 */
	u16	mRPME3_5;	/* 0x2E  Maitre / Eleve 12 a 14 Mod 3 */
	u16	mRPME3_6;	/* 0x2F  Maitre / Eleve 15 a 17 Mod 3 */
	u16	mRPME3_7;	/* 0x30  Maitre / Eleve 18 a 20 Mod 3 */
	u16	mRPME3_8;	/* 0x31  Maitre / Eleve 21 a 23 Mod 3 */
	u16	mRPME3_9;	/* 0x32  Maitre / Eleve 24 a 26 Mod 3 */
	u16	mRPME3_10;	/* 0x33  Maitre / Eleve 27 a 29 Mod 3 */
	u16	mRPME3_11;	/* 0x34  Maitre / Eleve 30 a 31 Mod 3 */
	u16	mRPME4_1;	/* 0x35  Maitre / Eleve 0 a 2 Mod 4 */
	u16	mRPME4_2;	/* 0x36  Maitre / Eleve 3 a 5 Mod 4 */
	u16	mRPME4_3;	/* 0x37  Maitre / Eleve 6 a 8 Mod 4 */
	u16	mRPME4_4;	/* 0x38  Maitre / Eleve 9 a 11 Mod 4 */
	u16	mRPME4_5;	/* 0x39  Maitre / Eleve 12 a 14 Mod 4 */
	u16	mRPME4_6;	/* 0x3A  Maitre / Eleve 15 a 17 Mod 4 */
	u16	mRPME4_7;	/* 0x3B  Maitre / Eleve 18 a 20 Mod 4 */
	u16	mRPME4_8;	/* 0x3C  Maitre / Eleve 21 a 23 Mod 4 */
	u16	mRPME4_9;	/* 0x3D  Maitre / Eleve 24 a 26 Mod 4 */
	u16	mRPME4_10;	/* 0x3E  Maitre / Eleve 27 a 29 Mod 4 */
	u16	mRPME4_11;	/* 0x3F  Maitre / Eleve 30 a 31 Mod 4 */
	u16	mRESV_3;	/* 0x40  reserve */
	u16	mRESV_4;	/* 0x41  reserve */
	u16	mRESV_5;	/* 0x42  reserve */
	u16	mRESV_6;	/* 0x43  reserve */
	u16	mRESV_7;	/* 0x44  reserve */
	u16	mRESV_8;	/* 0x45  reserve */
	u16	mRCMR0;		/* 0x46  Ctrl des modules radio */
	u16	mRCMR1;		/* 0x47  Ctrl du mod radio 1 */
	u16	mRCMR2;		/* 0x48  Ctrl du mod radio 2 */
	u16	mRCMR3;		/* 0x49  Ctrl du mod radio 3 */
	u16	mRCMR4;		/* 0x4A  Ctrl du mod radio 4 */
	u16	mRASMP;		/* 0x4B  Alternat modules po */
	u16	mRCLED;		/* 0x4C  Configuration des leds */
	u16	mRELED;		/* 0x4D  Etat des leds */
	u16	mRIDENT;	/* 0x4E  Identification binaire FPGA */
	u16	mRMINT1;	/* 0x4F  Masque des it 1 */
	u16	mRMINT2;	/* 0x50  Masque des it 2 */
	u16	mRESV_9;	/* 0x51  reserve */
	u16	mRESV_10;	/* 0x52  reserve */
	u16	mRMINT5;	/* 0x53  Masque des it sqh 1-8 PO1-2 */
	u16	mRMINT6;	/* 0x54  Masque des it sqh 1-8 PO3-4 */
	u16	mRMINT7;	/* 0x55  Masque des it alt 1-8 PO1-2 */
	u16	mRMINT8;	/* 0x56  Masque des it alt 1-8 PO3-4 */
	u16	mRMINT9;	/* 0x57  Masque des it 9 */
	u16	mRINT1;		/* 0x58  Statut it 1 */
	u16	mRINT2;		/* 0x59  Statut it 2 */
	u16	mRINT10;	/* 0x5A  Status it 10 */
	u16	mRESV_11;	/* 0x5B  reserve */
	u16	mRINT5;		/* 0x5C  Statut it sqh 1-8 PO1-2 */
	u16	mRINT6;		/* 0x5D  Statut it sqh 1-8 PO3-4 */
	u16	mRINT7;		/* 0x5E  Statut it alt 1-8 PO1-2 */
	u16	mRINT8;		/* 0x5F  Statut it alt 1-8 PO3-4 */
	u16	mRINT9;		/* 0x60  Statut it 9 */
	u16	mRSSINT;	/* 0x61  Synthese status RINT1-9 */
	u16	mRSINT1_1;	/* 0x62  Statut */
	u16	mRSINT1_2;	/* 0x63  Statut */
	u16	mRSINT1_3;	/* 0x64  Statut */
	u16	mRSINT2;	/* 0x65  Statut */
	u16	mRSINT3;	/* 0x66  Statut */
	u16	mRSINT4;	/* 0x67  Statut */
	u16	mRSINT5;	/* 0x68  Statut sqh 1-8 PO1-2 */
	u16	mRSINT6;	/* 0x69  Statut sqh 1-8 PO3-4 */
	u16	mRSINT7;	/* 0x6A  Statut alt 1-8 PO1-2 */
	u16	mRSINT8;	/* 0x6B  Statut alt 1-8 PO3-4 */
	u16	mRSINT9;	/* 0x6C  Status SCC et MCS */
	u16	mRESV_12;	/* 0x6D  reserve */
	u16	mRESV_13;	/* 0x6E  reserve */
	u16	mRESV_14;	/* 0x6F  reserve */
	u16	mRESV_15;	/* 0x70  reserve */
	u16	mRSERIE;	/* 0x71  Routage liaisons séries */
	u16	mRSERIE1;	/* 0x72  Routage UART interne */
	u16	mRMODE_LS;	/* 0x73  Type interface LS (V11/V28) */
	u16	mRBRG5;		/* 0x74  Horloge BRG 5 */
	u16	mRESV_16;	/* 0x75  reserve */
	u16	mRESV_17;	/* 0x76  reserve */
	u16	mRESV_18;	/* 0x77  reserve */
	u16	mRESV_19;	/* 0x78  reserve */
	u16	mRESV_20;	/* 0x79  reserve */
	u16	mRESV_21;	/* 0x7A  reserve */
	u16	mRESV_22;	/* 0x7B  reserve */
	u16	mRESV_23;	/* 0x7C  reserve */
	u16	mRESV_24;	/* 0x7D  reserve */
	u16	mRESV_25;	/* 0x7E  reserve */
	u16	mRESV_26;	/* 0x7F  reserve */
	u16	mRESV_27;	/* 0x80  reserve */
	u16	mRESV_28;	/* 0x81  reserve */
	u16	mRESV_29;	/* 0x82  reserve */
	u16	mRESV_30;	/* 0x83  reserve */
	u16	mRCDE_MR1;	/* 0x84  Commandes couplage voie 1 */
	u16	mRETAT_MR1;	/* 0x85  Etats couplage voie 1 */
	u16	mRESV_31;	/* 0x86  reserve */
	u16	mRCDE_MR2;	/* 0x87  Commandes couplage voie 2 */
	u16	mRETAT_MR2;	/* 0x88  Etats couplage voie 2 */
	u16	mRESV_32;	/* 0x89  reserve */
	u16	mRCDE_MR3;	/* 0x8A  Commandes couplage voie 3 */
	u16	mRETAT_MR3;	/* 0x8B  Etats couplage voie 3 */
	u16	mRESV_33;	/* 0x8C  reserve */
	u16	mRCDE_MR4;	/* 0x8D  Commandes couplage voie 4 */
	u16	mRETAT_MR4;	/* 0x8E  Etats couplage voie 4 */
	u16	mRESV_34;	/* 0x8F  reserve */
	u16	mRMINT5_1;	/* 0x90  Masque it sqh 9-16 PO1-2 */
	u16	mRMINT5_2;	/* 0x91  Masque it sqh 17-24 PO1-2 */
	u16	mRMINT5_3;	/* 0x92  Masque it sqh 25-32 PO1-2 */
	u16	mRMINT6_1;	/* 0x93  Masque it sqh 9-16 PO3-4 */
	u16	mRMINT6_2;	/* 0x94  Masque it sqh 17-24 PO3-4 */
	u16	mRMINT6_3;	/* 0x95  Masque it sqh 25-32 PO3-4 */
	u16	mRMINT7_1;	/* 0x96  Masque it alt 9-16 PO1-2 */
	u16	mRMINT7_2;	/* 0x97  Masque it alt 17-24 PO1-2 */
	u16	mRMINT7_3;	/* 0x98  Masque it alt 25-32 PO1-2 */
	u16	mRMINT8_1;	/* 0x99  Masque it alt 9-16 PO3-4 */
	u16	mRMINT8_2;	/* 0x9A  Masque it alt 17-24 PO3-4 */
	u16	mRMINT8_3;	/* 0x9B  Masque it alt 25-32 PO3-4 */
	u16	mRESV_35;	/* 0x9C  reserve */
	u16	mRESV_36;	/* 0x9D  reserve */
	u16	mRESV_37;	/* 0x9E  reserve */
	u16	mRESV_38;	/* 0x9F  reserve */
	u16	mRINT5_1;	/* 0xA0  Statut it sqh 9-16 PO1-2 */
	u16	mRINT5_2;	/* 0xA1  Statut it sqh 17-24 PO1-2 */
	u16	mRINT5_3;	/* 0xA2  Statut it sqh 25-32 PO1-2 */
	u16	mRINT6_1;	/* 0xA3  Statut it sqh 9-16 PO3-4 */
	u16	mRINT6_2;	/* 0xA4  Statut it sqh 17-24 PO3-4 */
	u16	mRINT6_3;	/* 0xA5  Statut it sqh 25-32 PO3-4 */
	u16	mRINT7_1;	/* 0xA6  Statut it alt 9-16 PO1-2 */
	u16	mRINT7_2;	/* 0xA7  Statut it alt 17-24 PO1-2 */
	u16	mRINT7_3;	/* 0xA8  Statut it alt 25-32 PO1-2 */
	u16	mRINT8_1;	/* 0xA9  Statut it alt 9-16 PO3-4 */
	u16	mRINT8_2;	/* 0xAA  Statut it alt 17-24 PO3-4 */
	u16	mRINT8_3;	/* 0xAB  Statut it alt 25-32 PO3-4 */
	u16	mRSSINT_1;	/* 0xAC  Synthese status RINT5_1-8_3 */
	u16	mRESV_39;	/* 0xAD  reserve */
	u16	mRESV_40;	/* 0xAE  reserve */
	u16	mRESV_41;	/* 0xAF  reserve */
	u16	mRSINT5_1;	/* 0xB0  Statut sqh 9-16 PO1-2 */
	u16	mRSINT5_2;	/* 0xB1  Statut sqh 17-24 PO1-2 */
	u16	mRSINT5_3;	/* 0xB2  Statut sqh 25-32 PO1-2 */
	u16	mRSINT6_1;	/* 0xB3  Statut sqh 9-16 PO3-4 */
	u16	mRSINT6_2;	/* 0xB4  Statut sqh 17-24 PO3-4 */
	u16	mRSINT6_3;	/* 0xB5  Statut sqh 25-32 PO3-4 */
	u16	mRSINT7_1;	/* 0xB6  Statut alt 9-16 PO1-2 */
	u16	mRSINT7_2;	/* 0xB7  Statut alt 17-24 PO1-2 */
	u16	mRSINT7_3;	/* 0xB8  Statut alt 25-32 PO1-2 */
	u16	mRSINT8_1;	/* 0xB9  Statut alt 9-16 PO3-4 */
	u16	mRSINT8_2;	/* 0xBA  Statut alt 17-24 PO3-4 */
	u16	mRSINT8_3;	/* 0xBB  Statut alt 25-32 PO3-4 */
	u16	mRESV_42;	/* 0xBC  reserve */
	u16	mRESV_43;	/* 0xBD  reserve */
	u16	mRESV_44;	/* 0xBE  reserve */
	u16	mRESV_45;	/* 0xBF  reserve */
	u16	mRETAT_MP12;	/* 0xC0  Etats couplage postes 1-2 */
	u16	mRCDE_MP12;	/* 0xC1  Cmdes couplage postes 1-2 */
	u16	mRESV_46;	/* 0xC2  reserve */
	u16	mRETAT_MP34;	/* 0xC3  Etats couplage postes 3-4 */
	u16	mRCDE_MP34;	/* 0xC4  Cmdes couplage postes 3-4 */
	u16	mRESV_47;	/* 0xC5  reserve */
	u16	mRMAA_MP1_1;	/* 0xC6  Masque AA 1-16 couplage PO1 */
	u16	mRMAA_MP1_2;	/* 0xC7  Masque AA 17-32 couplage PO1 */
	u16	mRMAA_MP2_1;	/* 0xC8  Masque AA 1-16 couplage PO2 */
	u16	mRMAA_MP2_2;	/* 0xC9  Masque AA 17-32 couplage PO2 */
	u16	mRMAA_MP3_1;	/* 0xCA  Masque AA 1-16 couplage PO3 */
	u16	mRMAA_MP3_2;	/* 0xCB  Masque AA 17-32 couplage PO3 */
	u16	mRMAA_MP4_1;	/* 0xCC  Masque AA 1-16 couplage PO4 */
	u16	mRMAA_MP4_2;	/* 0xCD  Masque AA 17-32 couplage PO4 */
	u16	mRAA_MP1_1;	/* 0xCE  Etat AA 1-16 couplage PO1 */
	u16	mRAA_MP1_2;	/* 0xCF  Etat AA 17-32 couplage PO1 */
	u16	mRAA_MP2_1;	/* 0xD0  Etat AA 1-16 couplage PO2 */
	u16	mRAA_MP2_2;	/* 0xD1  Etat AA 17-32 couplage PO2 */
	u16	mRAA_MP3_1;	/* 0xD2  Etat AA 1-16 couplage PO3 */
	u16	mRAA_MP3_2;	/* 0xD3  Etat AA 17-32 couplage PO3 */
	u16	mRAA_MP4_1;	/* 0xD4  Etat AA 1-16 couplage PO4 */
	u16	mRAA_MP4_2;	/* 0xD5  Etat AA 17-32 couplage PO4 */
	u16	mRTIM_MP1_1;	/* 0xD6  Latence 1-3 couplage PO1 */
	u16	mRTIM_MP1_2;	/* 0xD7  Latence 4-6 couplage PO1 */
	u16	mRTIM_MP1_3;	/* 0xD8  Latence 7-9 couplage PO1 */
	u16	mRTIM_MP1_4;	/* 0xD9  Latence 10-12 couplage PO1 */
	u16	mRTIM_MP1_5;	/* 0xDA  Latence 13-15 couplage PO1 */
	u16	mRTIM_MP1_6;	/* 0xDB  Latence 16-18 couplage PO1 */
	u16	mRTIM_MP1_7;	/* 0xDC  Latence 19-21 couplage PO1 */
	u16	mRTIM_MP1_8;	/* 0xDD  Latence 22-24 couplage PO1 */
	u16	mRTIM_MP1_9;	/* 0xDE  Latence 25-27 couplage PO1 */
	u16	mRTIM_MP1_10;	/* 0xDF  Latence 28-30 couplage PO1 */
	u16	mRTIM_MP1_11;	/* 0xE0  Latence 31-32 couplage PO1 */
	u16	mRTIM_MP2_1;	/* 0xE1  Latence 1-3 couplage PO2 */
	u16	mRTIM_MP2_2;	/* 0xE2  Latence 4-6 couplage PO2 */
	u16	mRTIM_MP2_3;	/* 0xE3  Latence 7-9 couplage PO2 */
	u16	mRTIM_MP2_4;	/* 0xE4  Latence 10-12 couplage PO2 */
	u16	mRTIM_MP2_5;	/* 0xE5  Latence 13-15 couplage PO2 */
	u16	mRTIM_MP2_6;	/* 0xE6  Latence 16-18 couplage PO2 */
	u16	mRTIM_MP2_7;	/* 0xE7  Latence 19-21 couplage PO2 */
	u16	mRTIM_MP2_8;	/* 0xE8  Latence 22-24 couplage PO2 */
	u16	mRTIM_MP2_9;	/* 0xE9  Latence 25-27 couplage PO2 */
	u16	mRTIM_MP2_10;	/* 0xEA  Latence 28-30 couplage PO2 */
	u16	mRTIM_MP2_11;	/* 0xEB  Latence 31-32 couplage PO2 */
	u16	mRTIM_MP3_1;	/* 0xEC  Latence 1-3 couplage PO3 */
	u16	mRTIM_MP3_2;	/* 0xED  Latence 4-6 couplage PO3 */
	u16	mRTIM_MP3_3;	/* 0xEE  Latence 7-9 couplage PO3 */
	u16	mRTIM_MP3_4;	/* 0xEF  Latence 10-12 couplage PO3 */
	u16	mRTIM_MP3_5;	/* 0xF0  Latence 13-15 couplage PO3 */
	u16	mRTIM_MP3_6;	/* 0xF1  Latence 16-18 couplage PO3 */
	u16	mRTIM_MP3_7;	/* 0xF2  Latence 19-21 couplage PO3 */
	u16	mRTIM_MP3_8;	/* 0xF3  Latence 22-24 couplage PO3 */
	u16	mRTIM_MP3_9;	/* 0xF4  Latence 25-27 couplage PO3 */
	u16	mRTIM_MP3_10;	/* 0xF5  Latence 28-30 couplage PO3 */
	u16	mRTIM_MP3_11;	/* 0xF6  Latence 31-32 couplage PO3 */
	u16	mRESV_48;	/* 0xF7  reserve */
	u16	mRTIM_LEDJ1;	/* 0xF8  Delais on/off led jaune 1-2 */
	u16	mRTIM_LEDJ2;	/* 0xF9  Delais on/off led jaune 3-4 */
	u16	mRTIM_LEDJ3;	/* 0xFA  Delais on/off led jaune 5-6 */
	u16	mRTIM_LEDJ4;	/* 0xFB  Delais on/off led jaune 7-8 */
	u16	mRTIM_LEDV1;	/* 0xFC  Delais on/off led verte 1-2 */
	u16	mRTIM_LEDV2;	/* 0xFD  Delais on/off led verte 3-4 */
	u16	mRTIM_LEDV3;	/* 0xFE  Delais on/off led verte 5-6 */
	u16	mRTIM_LEDV4;	/* 0xFF  Delais on/off led verte 7-8 */
	u16	mRESV_49;	/* 0x100 reserve */
	u16	mRCFG_ETOR;	/* 0x101 Configuration filtrage ETORs */
	u16	mRLISSAGE1;	/* 0x102 Temps de lissage ETORs 1-8 */
	u16	mRLISSAGE2;	/* 0x103 Temps de lissage ETORs 9-16 */
	u16	mRFILTRAGE;	/* 0x104 Temps de filtrage des ETORs */
	u16	mRESV_50;	/* 0x105 reserve */
	u16	mRESV_51;	/* 0x106 reserve */
	u16	mRESV_52;	/* 0x107 reserve */
	u16	mRAPP_BSS;	/* 0x108 Appels des voies BSS */
	u16	mRESV_53;	/* 0x109 reserve */
	u16	mRESV_54;	/* 0x10A reserve */
	u16	mRESV_55;	/* 0x10B reserve */
	u16	mRESV_56;	/* 0x10C reserve */
	u16	mRESV_57;	/* 0x10D reserve */
	u16	mRALT_MP1_1;	/* 0x10E Etat Alt 1-16 couplage PO1 */
	u16	mRALT_MP1_2;	/* 0x10F Etat Alt 17-32 couplage PO1 */
	u16	mRALT_MP2_1;	/* 0x110 Etat Alt 1-16 couplage PO2 */
	u16	mRALT_MP2_2;	/* 0x111 Etat Alt 17-32 couplage PO2 */
	u16	mRALT_MP3_1;	/* 0x112 Etat Alt 1-16 couplage PO3 */
	u16	mRALT_MP3_2;	/* 0x113 Etat Alt 17-32 couplage PO3 */
	u16	mRALT_MP4_1;	/* 0x114 Etat Alt 1-16 couplage PO4 */
	u16	mRALT_MP4_2;	/* 0x115 Etat Alt 17-32 couplage PO4 */
	u16	mRESV_58;	/* 0x116 reserve */
	u16	mRESV_59;	/* 0x117 reserve */
	u16	mRTIM_MP4_1;	/* 0x118 Latence 1-3 couplage PO4 */
	u16	mRTIM_MP4_2;	/* 0x119 Latence 4-6 couplage PO4 */
	u16	mRTIM_MP4_3;	/* 0x11A Latence 7-9 couplage PO4 */
	u16	mRTIM_MP4_4;	/* 0x11B Latence 10-12 couplage PO4 */
	u16	mRTIM_MP4_5;	/* 0x11C Latence 13-15 couplage PO4 */
	u16	mRTIM_MP4_6;	/* 0x11D Latence 16-18 couplage PO4 */
	u16	mRTIM_MP4_7;	/* 0x11E Latence 19-21 couplage PO4 */
	u16	mRTIM_MP4_8;	/* 0x11F Latence 22-24 couplage PO4 */
	u16	mRTIM_MP4_9;	/* 0x120 Latence 25-27 couplage PO4 */
	u16	mRTIM_MP4_10;	/* 0x121 Latence 28-30 couplage PO4 */
	u16	mRTIM_MP4_11;	/* 0x122 Latence 31-32 couplage PO4 */
	u16	mRESV_60;	/* 0x123 reserve */
	u16	mRINT1_ACK;	/* 0x124 Acquittement ITs RINT1 */
	u16	mRINT2_ACK;	/* 0x125 Acquittement ITs RINT2 */
	u16	mRESV_61;	/* 0x126 reserve Ack ITs RINT3 */
	u16	mRESV_62;	/* 0x127 reserve Ack ITs RINT4 */
	u16	mRINT5_ACK;	/* 0x128 Acquittement ITs RINT5 */
	u16	mRINT6_ACK;	/* 0x129 Acquittement ITs RINT6 */
	u16	mRINT7_ACK;	/* 0x12A Acquittement ITs RINT7 */
	u16	mRINT8_ACK;	/* 0x12B Acquittement ITs RINT8 */
	u16	mRINT9_ACK;	/* 0x12C Acquittement ITs RINT9 */
	u16	mRESV_63;	/* 0x12D reserve Ack ITs RINT10 */
	u16	mRESV_64;	/* 0x12E reserve Ack ITs RINT11 */
	u16	mRESV_65;	/* 0x12F reserve Ack ITs RINT12 */
	u16	mRESV_66;	/* 0x130 reserve Ack ITs RINT13 */
	u16	mRESV_67;	/* 0x131 reserve Ack ITs RINT14 */
	u16	mRESV_68;	/* 0x132 reserve Ack ITs RINT15 */
	u16	mRESV_69;	/* 0x133 reserve Ack ITs RINT16 */
	u16	mRINT5_1_ACK;	/* 0x134 Acquittement ITs RINT17 */
	u16	mRINT6_1_ACK;	/* 0x135 Acquittement ITs RINT18 */
	u16	mRINT7_1_ACK;	/* 0x136 Acquittement ITs RINT19 */
	u16	mRINT8_1_ACK;	/* 0x137 Acquittement ITs RINT20 */
	u16	mRINT5_2_ACK;	/* 0x138 Acquittement ITs RINT21 */
	u16	mRINT6_2_ACK;	/* 0x139 Acquittement ITs RINT22 */
	u16	mRINT7_2_ACK;	/* 0x13A Acquittement ITs RINT23 */
	u16	mRINT8_2_ACK;	/* 0x13B Acquittement ITs RINT24 */
	u16	mRINT5_3_ACK;	/* 0x13C Acquittement ITs RINT25 */
	u16	mRINT6_3_ACK;	/* 0x13D Acquittement ITs RINT26 */
	u16	mRINT7_3_ACK;	/* 0x13E Acquittement ITs RINT27 */
	u16	mRINT8_3_ACK;	/* 0x13F Acquittement ITs RINT28 */
	u16	mRESV_70;	/* 0x140 reserve Ack ITs RINT29 */
	u16	mRESV_71;	/* 0x141 reserve Ack ITs RINT30 */
	u16	mRESV_72;	/* 0x142 reserve Ack ITs RINT31 */
	u16	mRESV_73;	/* 0x143 reserve Ack ITs RINT32 */
	u16	mRUART_CTL;	/* 0x144 Reg controle UART */
	u16	mRUART_RX;	/* 0x145 Reg reception UART */
	u16	mRUART_TX;	/* 0x146 Reg emission UART */
};

/* Masque de bits pour les registres d'it */
#define	RINT1_ETOR_MSK		0x0FFF
#define	RINT1_POCPL_MSK		0xF000
#define	RINT2_COUVERTURE_MSK	0x000F
#define	RINT2_ETORSQH_MSK	0x01E0
#define	RINT2_STORALT_MSK	0x1E00
#define	RINT2_DSP_MSK		0x4000
#define RINT5_POSQ1_MSK		0x00FF
#define RINT5_POSQ2_MSK		0xFF00
#define RINT6_POSQ3_MSK		0x00FF
#define RINT6_POSQ4_MSK		0xFF00
#define RINT7_POALT1_MSK	0x00FF
#define RINT7_POALT2_MSK	0xFF00
#define RINT8_POALT3_MSK	0x00FF
#define RINT8_POALT4_MSK	0xFF00
#define	RINT9_MCS_MSK		0x0F00
#define	RINT9_ETATALT_MSK	0xF000
#define	RINT9_UART_MSK		0x001F

/* Definition des bits d'it pour rssint et rssint_1 */
#define IDENT_BIT_RINT1		(1 << 0)
#define IDENT_BIT_RINT2		(1 << 1)
#define IDENT_BIT_RINT3		(1 << 2)
#define IDENT_BIT_RINT4		(1 << 3)
#define IDENT_BIT_RINT5		(1 << 4)
#define IDENT_BIT_RINT6		(1 << 5)
#define IDENT_BIT_RINT7		(1 << 6)
#define IDENT_BIT_RINT8		(1 << 7)
#define IDENT_BIT_RINT9		(1 << 8)
#define IDENT_BIT_RINT10	(1 << 9)
#define IDENT_BIT_RINT11	(1 << 10)
#define IDENT_BIT_RINT12	(1 << 11)
#define IDENT_BIT_RINT13	(1 << 12)
#define IDENT_BIT_RINT14	(1 << 13)
#define IDENT_BIT_RINT15	(1 << 14)
#define IDENT_BIT_RINT16	(1 << 15)
#define IDENT_BIT_RINT17	(1 << 16)
#define IDENT_BIT_RINT18	(1 << 17)
#define IDENT_BIT_RINT19	(1 << 18)
#define IDENT_BIT_RINT20	(1 << 19)
#define IDENT_BIT_RINT21	(1 << 20)
#define IDENT_BIT_RINT22	(1 << 21)
#define IDENT_BIT_RINT23	(1 << 22)
#define IDENT_BIT_RINT24	(1 << 23)
#define IDENT_BIT_RINT25	(1 << 24)
#define IDENT_BIT_RINT26	(1 << 25)
#define IDENT_BIT_RINT27	(1 << 26)
#define IDENT_BIT_RINT28	(1 << 27)
#define IDENT_BIT_RINT29	(1 << 28)
#define IDENT_BIT_RINT30	(1 << 29)
#define IDENT_BIT_RINT31	(1 << 30)
#define IDENT_BIT_RINT32	(1 << 31)

#endif /* __FPGA_H__ */
