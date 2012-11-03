#ifndef __FPGAF_M__
#define __FPGAF_M__

struct fpgam {
	u16 ident;
	u16 version;
	u16 test;
	u16 res1[5];
	u16 reset;
	u16 res2[7];
	u16 it_mask1;
	u16 it_mask2;
	u16 it_pend1;
	u16 it_pend2;
	u16 it_ack1;
	u16 it_ack2;
	u16 it_ctrl;
	u16 res3[1];
	u16 tor_in;
	u16 tor_out;
	u16 res4[6];
	u16 fct_gen;
	u16 gest_far;
	u16 gest_fav;
	u16 res5[5];
	u16 pll_status;
	u16 pll_src;
	u16 etat_ref;
};

#endif /* __FPGAF_M__ */
