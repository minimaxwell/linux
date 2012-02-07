/* 
 * arch/powerpc/platforms/8xx/mcr3000_2g.c
 *
 * Copyright 2011 CSSI
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

#include <saf3000/fpgaf.h>

#include "mpc8xx.h"
#include "mcr3000_2g.h"

/*
 * Controlleur d'IRQ du FPGA Firmware 
 */
static struct fpgaf *fpgaf_regs;
static struct irq_host *fpgaf_pic_host;

static void fpgaf_mask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	clrbits16(&fpgaf_regs->it_mask, 1<<(15-vec));
}

static void fpgaf_unmask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	setbits16(&fpgaf_regs->it_mask, 1<<(15-vec));
}

static void fpgaf_end_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	clrbits16(&fpgaf_regs->it_ack, 1<<(15-vec));
}

static struct irq_chip fpgaf_pic = {
	.name = "FPGAF PIC",
	.irq_mask = fpgaf_mask_irq,
	.irq_unmask = fpgaf_unmask_irq,
	.irq_eoi = fpgaf_end_irq,
};

int fpgaf_get_irq(void)
{
	int vec;
	int ret;

	vec = 16 - ffs(in_be16(&fpgaf_regs->it_pend));
	
	ret=irq_linear_revmap(fpgaf_pic_host, vec);
	return ret;
}

static int fpgaf_pic_host_map(struct irq_host *h, unsigned int virq, irq_hw_number_t hw)
{
	pr_debug("fpgaf_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &fpgaf_pic, handle_fasteoi_irq);
	return 0;
}

static struct irq_host_ops fpgaf_pic_host_ops = {
	.map = fpgaf_pic_host_map,
};

int fpgaf_pic_init(void)
{
	struct device_node *np = NULL;
	struct resource res;
	unsigned int irq = NO_IRQ, hwirq;
	int ret;

	pr_debug("fpgaf_pic_init\n");

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000_2g-pic");
	if (np == NULL) {
		printk(KERN_ERR "FPGAF PIC init: can not find mcr3000_2g-pic node\n");
		return irq;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto end;

	fpgaf_regs = ioremap(res.start, res.end - res.start + 1);
	if (fpgaf_regs == NULL)
		goto end;

	irq = irq_of_parse_and_map(np, 0);
	if (irq == NO_IRQ)
		goto end;

	/* Initialize the FPGAF interrupt controller. */
	hwirq = (unsigned int)virq_to_hw(irq);

	fpgaf_pic_host = irq_alloc_host(np, IRQ_HOST_MAP_LINEAR, 16, &fpgaf_pic_host_ops, 16);
	if (fpgaf_pic_host == NULL) {
		printk(KERN_ERR "FPGAF PIC: failed to allocate irq host!\n");
		irq = NO_IRQ;
		goto end;
	}

end:
	of_node_put(np);
	return irq;
}

void fpgaf_cascade(unsigned int irq, struct irq_desc *desc)
{
	int cascade_irq;
	struct irq_chip *chip;

	if ((cascade_irq = fpgaf_get_irq()) >= 0) {
		struct irq_desc *cdesc = irq_to_desc(cascade_irq);

		generic_handle_irq(cascade_irq);
		chip = irq_desc_get_chip(cdesc);
		if (chip->irq_eoi) chip->irq_eoi(&cdesc->irq_data);
	}
	chip = irq_desc_get_chip(desc);
	if (chip->irq_eoi) chip->irq_eoi(&desc->irq_data);
}

/*
 * Init des devices
 */
void fpgaf_init_platform_devices(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-fpga-f");
	if (np) {
		int ngpios = of_gpio_count(np);

		printk(KERN_INFO "Number of GPIO found = %d", ngpios);

	} else {
		printk(KERN_ERR "FPGA-F: compatible node not found\n");
	}
}

