/*
 * 2011 (c) CSSI, Inc.
 */
#include <linux/irq.h>

extern void fpgaf_cascade(unsigned int irq, struct irq_desc *desc);
extern void fpgaf_init_platform_devices(void);
extern int fpgaf_pic_init(void);

