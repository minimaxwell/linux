/*
 * 2011 (c) CSSI, Inc.
 */
#include <linux/irq.h>

/* mcr3000_2g.c */
extern void fpgaf_cascade(unsigned int irq, struct irq_desc *desc);
extern void fpgaf_init_platform_devices(void);
extern int fpgaf_pic_init(void);
/* mcr3000_2g_clock.c */
extern int __init fpga_clk_init(void);
