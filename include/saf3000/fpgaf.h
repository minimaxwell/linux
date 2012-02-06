#ifndef __FPGAF_H__
#define __FPGAF_H__

struct fpgaf {
	u16 ident;
	u16 version;
	u16 res1[6];
	u16 reset;
	u16 res2[7];
	u16 it_mask;
	u16 it_pend;
	u16 it_ack;
	u16 it_ctr;
	u16 res3[4];
	u16 alrm_in;
	u16 alrm_out;
	u16 res4[6];
	u16 fonc_gen;
	u16 addr;
	u16 res5[6];
	u16 pll_ctr;
	u16 pll_status;
	u16 pll_src;
	u16 res6[5];
	u16 net_ref;
	u16 etat_ref;
	u16 res7[6];
	u16 syn_h110;
	u16 res8[7];
	u16 test;
};

#endif /* __FPGAF_H__ */
