/*
 * 2011 (c) CSSI, Inc.
 */
#include <linux/irq.h>

/* mcr3000_2g.c */
extern void fpgaf_cascade(struct irq_desc *desc);
extern void fpgaf_init_platform_devices(void);
extern int fpgaf_pic_init(void);
/* mcr3000_2g_clock.c */
extern int __init fpga_clk_init(void);
/* mcr3000_2g_fpga.c */
extern void fpga_cascade(struct irq_desc *desc);
extern void fpga_init_platform_devices(void);
extern int fpga_pic_init(void);
