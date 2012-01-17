/* 
 * arch/powerpc/platforms/8xx/miae.c
 *
 * Copyright 2012 CSSI
 *
 */

#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>

#include <ldb/ldb_gpio.h>

#include <sysdev/simple_gpio.h>

#include <saf3000/fpgam.h>

#include "mpc8xx.h"
#include "miae.h"

/*
 * Controlleur d'IRQ du FPGA MIAE 
 */
static struct fpgam *fpgam_regs;
static struct irq_host *fpgam_pic_host;

static void fpgam_mask_irq(unsigned int irq)
{
	unsigned int vec = (unsigned int)irq_map[irq].hwirq;

	if (vec < 16)
		clrbits16(&fpgam_regs->it_mask1, 1 << (15-vec));
	else {
		vec -= 16;
		clrbits16(&fpgam_regs->it_mask2, 1 << (15-vec));
	}
}

static void fpgam_unmask_irq(unsigned int irq)
{
	unsigned int vec = (unsigned int)irq_map[irq].hwirq;

	if (vec < 16)
		setbits16(&fpgam_regs->it_mask1, 1 << (15-vec));
	else {
		vec -= 16;
		setbits16(&fpgam_regs->it_mask2, 1 << (15-vec));
	}
}

static void fpgam_end_irq(unsigned int irq)
{
	unsigned int vec = (unsigned int)irq_map[irq].hwirq;

	if (vec < 16)
		clrbits16(&fpgam_regs->it_ack1, 1 << (15-vec));
	else {
		vec -= 16;
		clrbits16(&fpgam_regs->it_ack2, 1 << (15-vec));
	}
}

static struct irq_chip fpgam_pic = {
	.name = "FPGAM PIC",
	.mask = fpgam_mask_irq,
	.unmask = fpgam_unmask_irq,
	.eoi = fpgam_end_irq,
};

int fpgam_get_irq(void)
{
	int vec;
	int ret;

	if (in_be16(&fpgam_regs->it_pend1))
		vec = 16 - ffs(in_be16(&fpgam_regs->it_pend1));
	else
		vec = 32 - ffs(in_be16(&fpgam_regs->it_pend2));
	
	ret=irq_linear_revmap(fpgam_pic_host, vec);
	return ret;
}

static int fpgam_pic_host_map(struct irq_host *h, unsigned int virq, irq_hw_number_t hw)
{
	pr_debug("fpgaf_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_to_desc(virq)->status |= IRQ_LEVEL;
	set_irq_chip_and_handler(virq, &fpgam_pic, handle_fasteoi_irq);
	return 0;
}

static struct irq_host_ops fpgam_pic_host_ops = {
	.map = fpgam_pic_host_map,
};

int fpgam_pic_init(void)
{
	struct device_node *np = NULL;
	struct resource res;
	unsigned int irq = NO_IRQ, hwirq;
	int ret;

	pr_debug("fpgam_pic_init\n");

	np = of_find_compatible_node(NULL, NULL, "s3k,miae-pic");
	if (np == NULL) {
		printk(KERN_ERR "FPGAM PIC init: can not find miae-pic node\n");
		return irq;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto end;

	fpgam_regs = ioremap(res.start, res.end - res.start + 1);
	if (fpgam_regs == NULL)
		goto end;

	irq = irq_of_parse_and_map(np, 0);
	if (irq == NO_IRQ)
		goto end;

	/* Initialize the FPGAF interrupt controller. */
	hwirq = (unsigned int)irq_map[irq].hwirq;

	fpgam_pic_host = irq_alloc_host(np, IRQ_HOST_MAP_LINEAR, 16, &fpgam_pic_host_ops, 16);
	if (fpgam_pic_host == NULL) {
		printk(KERN_ERR "FPGAM PIC: failed to allocate irq host!\n");
		irq = NO_IRQ;
		goto end;
	}

end:
	of_node_put(np);
	return irq;
}

void fpgam_cascade(unsigned int irq, struct irq_desc *desc)
{
	int cascade_irq;

	if ((cascade_irq = fpgam_get_irq()) >= 0) {
		struct irq_desc *cdesc = irq_to_desc(cascade_irq);

		generic_handle_irq(cascade_irq);
		if (cdesc->chip->eoi) cdesc->chip->eoi(cascade_irq);
	}
	if (desc->chip->eoi) desc->chip->eoi(irq);
}

/*
 * Init des devices
 */
void fpgam_init_platform_devices(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-fpga-m");
	if (np) {
		int ngpios = of_gpio_count(np);

		printk(KERN_INFO "Number of GPIO found = %d", ngpios);

	} else {
		printk(KERN_ERR "FPGA-M: compatible node not found\n");
	}
}
