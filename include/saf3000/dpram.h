#ifndef __DPRAM_H__
#define __DPRAM_H__

typedef void (*(thandler))(short*);
extern void enreg_handler(int, thandler ipFonction);

extern void enreg_it_msg(void);
extern int write_fifo_cmde(short *cmde);

/*
 * idendificateurs des messages émis par le DSP au MPC
 */
#define	NO_MSG				0
#define MSG_START_ACK			1	/* Msg 1 */
#define MSG_DSP_ERR			2	/* Msg 2 */
#define MSG_LOST_FRAME			3	/* Msg 3 */
#define MSG_NIVEAU_FREQ_TEST		4	/* Msg 4 */
#define MSG_DETECT_FREQ_TELECMD		5	/* Msg 5 */
#define MSG_NIVEAU_FREQ_TELECMD		6	/* Msg 6 */
#define MSG_CHGT_CAG			7	/* Msg 7 */
#define MSG_RECV_DTMF			8	/* Msg 8 */
#define MSG_CHGT_ETAT_SQ_DSP		9	/* Msg 9 */
#define MSG_CHGT_BSS			10	/* Msg 10 */
#define MSG_ACK_DTMF			11	/* Msg 11 */
#define	MSG_ACK_600BDS			12	/* Msg 12 */
#define	MSG_GET_ETAT_FREQ_TELECMD	13	/* Msg 13 */
#define MSG_DUMP_MEM			14	/* Msg 14 = contenu d'une adresse mémoire */
#define	MSG_VOIE_REEMISE		15	/* Msg 15 = voie réémise (ident et rang) */
#define MSG_VISU_SIG_RDO		16	/* Msg 16 = visu TS Sig Rdo (par rang) */
#define MSG_VISU_SIG_CPL		17	/* Msg 17 = visu TS Sig Couplage (sortie Mod. CPL) */
#define MSG_VISU_CDE_CPL		18	/* Msg 18 = visu TS Sig Couplage (entrée Mod. VOIE) */
#define MSG_PB_CTL_ECOUTE		19	/* Msg 19 = problème sur détection réécoute (Mod. VOIE) */
#define MSG_NOTES_BSS			20	/* Msg 20 = notes des 3 voies du groupement BSS */
#define MSG_APPEL_BSS			21	/* Msg 21 = appels des 3 voies du groupement BSS */
#define MSG_REC_ATS			22	/* Msg 22 = détection d'un code ATS (Mod. VOIE) */
#define MSG_ACK_ATS			23	/* Msg 23 = acquis émission code ATS (Mod. VOIE) */

/* messages relatifs à GPGW */
#define MSG_ATS				24	/* Msg 24 = message module ATS */
#define MSG_DTMF			25	/* Msg 25 = message module DTMF */
#define MSG_TON				26	/* Msg 26 = message module TON */
#define MSG_VOIP_EM			27	/* Msg 27 = message module VOIP EM */
#define MSG_VOIP_REC			28	/* Msg 28 = message module VOIP REC */
#define MSG_VOIP_ENREG			29	/* Msg 29 = message module VOIP ENREG */
#define MSG_QSIG_EM			30	/* Msg 30 = message module QSIG EM */
#define MSG_QSIG_REC			31	/* Msg 31 = message module QSIG REC */

#define MSG_MANAGE			32	/* Msg 32 = message module MANAGE */
#define MSG_CAS				33	/* Msg 33 = message module CAS */
#define MSG_BSS				34	/* Msg 34 = message module BSS/GRS */
	
#define MSG_LAST_MSG			35

#endif /* __DPRAM_H__ */
