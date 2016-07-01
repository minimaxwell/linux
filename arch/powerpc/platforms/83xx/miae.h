/*
 * 2012 (c) CSSI, Inc.
 */
#include <linux/irq.h>

/* miae.c */
extern void fpgam_cascade(unsigned int irq, struct irq_desc *desc);
extern void fpgam_init_platform_devices(void);
extern int fpgam_pic_init(void);
