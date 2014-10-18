#ifndef __CPM1_DSP_H
#define __CPM1_DSP_H

#include <asm/cpm1.h>

#define PROFF_DSP1	((uint)0x02C0)
#define PROFF_DSP2	((uint)0x03C0)

/*
 * DSP pram
 */
typedef struct dsp_param {
	uint	dsp_fdbase;	/* Function descriptor chain base address */
	uint	dsp_fdptr;	/* Current FD pointer */
	uint	dsp_dstate;	/* DSP state */
	uint	res;		/* Reserved */
	ushort	dsp_dstatus;	/* Current FD status */
	ushort	dsp_i;		/* Current FD nb of iteration */
	ushort	dsp_tap;		/* Current FD nb of taps */
	ushort	dsp_cbase;	/* Current FD coefficient buffer base */
	ushort	dsp_x;		/* Current FD sample buffer size -1 */
	ushort	dsp_xptr;	/* Current FD pointer to input buffer */
	ushort	dsp_y;		/* Current FD output buffer size -1 */
	ushort	dsp_yptr;	/* Current FD pointer to output buffer */
	ushort	dsp_m;		/* Current FD input buffer size -1 */
	ushort	dsp_mptr;	/* Current FD input buffer data pointer */
	ushort	dsp_n;		/* Current FD output buffer size -1 */
	ushort	dsp_nptr;	/* Current FD output buffer data pointer */
	ushort	dsp_k;		/* Current FD coefficient buffer size -1 */
	ushort	dsp_kptr;	/* Current FD coefficient buffer data pointer */
} dspp_t;


/* Opcodes
*/
#define CPM_CR_INIT_DSP		((ushort)0x000d)
#define CPM_CR_START_DSP	((ushort)0x000c)

/* DSP Function descriptors */
typedef struct cpm_dsp_buf_desc {
	ushort	fbd_sc;		/* Status and Control */
	ushort	fbd_param[7];	/* Parameters 1 to 7 */
} fbd_t;

/* Buffer descriptor control/status used by serial
 */

#define FD_SC_S			(0x8000)	/* Stop processing */
#define FD_SC_W			(0x2000)	/* Wrap to the beginning of the chain */
#define FD_SC_I			(0x1000)	/* Interrupt the core */
#define FD_SC_X			(0x0800)	/* Complex number option */
#define FD_SC_IALL		(0x0400)	/* Auto inc X for all iterations */
#define FD_SC_INDEX_NONE	(0x0000)	/* Auto increment index none */
#define FD_SC_INDEX_ONE		(0x0100)	/* Auto increment index by one */
#define FD_SC_INDEX_TWO		(0x0200)	/* Auto increment index by two */
#define FD_SC_INDEX_THREE	(0x0300)	/* Auto increment index by three */
#define FD_SC_PC		(0x0080)	/* Preset coefficients pointer */

/*
 * DSP Library Functions
 */
 
#define FD_SC_OP_FIR1		(0x0001)		/* FIR1 - Real C, Real X and Real Y */
#define FD_SC_OP_FIR2		(0x0002)		/* FIR2 - Real C, Complex X and Complex Y */
#define FD_SC_OP_FIR3		(0x0003)		/* FIR3 - Complex C, Complex X and Real/Complex Y */
#define FD_SC_OP_FIR5		(0x0005)		/* FIR5 - Complex C, Complex X and Complex Y */
#define FD_SC_OP_FIR6		(0x0006)		/* FIR6 - Complex C, Real X and Complex Y */
#define FD_SC_OP_IIR		(0x0007)		/* IIR - Real C, Real X and Real Y */
#define FD_SC_OP_MOD		(0x0008)		/* MOD - Real Sin, Real Cos, Complex X and Real/Complex Y */
#define FD_SC_OP_DEMOD		(0x0009)		/* DEMOD - Real Sin, Real Cos, Real X and Complex Y */
#define FD_SC_OP_LMS1		(0x000a)		/* LMS1 - Complex Coefficients, Complex Samples and Real/Complex Scalar */
#define FD_SC_OP_LMS2		(0x000b)		/* LMS2 - Complex Coefficients, Complex Samples and Real/Complex Scalar */
#define FD_SC_OP_WADD		(0x000c)		/* WADD - Real X and Real Y */

#define SDMA_DSP1		(0x01)
#define SDMA_DSP2		(0x02)

#endif
