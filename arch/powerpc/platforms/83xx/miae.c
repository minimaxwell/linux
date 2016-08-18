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
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/time.h>
#include <asm/immap_qe.h>
//FIXME ? #include <asm/fs_pd.h>
//FIXME ? #include <asm/udbg.h>

#include <saf3000/ldb_gpio.h>

#include <sysdev/simple_gpio.h>

#include <saf3000/fpgam.h>

#ifdef CONFIG_PPC_83xx 
#include "mpc83xx.h"
#else
#include "mpc8xx.h"
#endif
#include "miae.h"

/*
 * Controlleur d'IRQ du FPGA MIAE 
 */
static struct fpgam *fpgam_regs;
static struct irq_domain *fpgam_pic_host;

static void fpgam_mask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	if (vec < 16)
		clrbits16(&fpgam_regs->it_mask1, 1 << (15-vec));
	else {
		vec -= 16;
		clrbits16(&fpgam_regs->it_mask2, 1 << (15-vec));
	}
}

static void fpgam_unmask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	if (vec < 16)
		setbits16(&fpgam_regs->it_mask1, 1 << (15-vec));
	else {
		vec -= 16;
		setbits16(&fpgam_regs->it_mask2, 1 << (15-vec));
	}
}

static void fpgam_end_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	if (vec < 16)
		out_be16(&fpgam_regs->it_ack1, ~(1 << (15-vec)));
	else {
		vec -= 16;
		out_be16(&fpgam_regs->it_ack2, ~(1 << (15-vec)));
	}
}

static struct irq_chip fpgam_pic = {
	.name = "FPGAM PIC",
	.irq_mask = fpgam_mask_irq,
	.irq_unmask = fpgam_unmask_irq,
	.irq_eoi = fpgam_end_irq,
};

int fpgam_get_irq(void)
{
	int vec;
	int ret;
	int pending1 = in_be16(&fpgam_regs->it_pend1) & 0x87FF;
	int pending2 = in_be16(&fpgam_regs->it_pend2);

	if (pending1) {
		vec = 16 - ffs(pending1);
		ret = irq_linear_revmap(fpgam_pic_host, vec);
	}
	else if (pending2) {
		vec = 32 - ffs(pending2);
		ret = irq_linear_revmap(fpgam_pic_host, vec);
	}
	else {
		ret = -1;
	}
	
	return ret;
}

static int fpgam_pic_host_map(struct irq_domain *h, unsigned int virq, irq_hw_number_t hw)
{
	pr_debug("fpgaf_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &fpgam_pic, handle_fasteoi_irq);
	return 0;
}

static struct irq_domain_ops fpgam_pic_host_ops = {
	.map = fpgam_pic_host_map,
};

int fpgam_pic_init(void)
{
	struct device_node *np = NULL;
	struct resource res;
	unsigned int irq = NO_IRQ, hwirq;
	int ret;

	pr_debug("fpgam_pic_init\n");
	printk(KERN_ERR"fpgam_pic_init\n");

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

	/* Initialize the FPGAM interrupt controller. */
	hwirq = (unsigned int)virq_to_hw(irq);

	fpgam_pic_host = irq_domain_add_linear(np, 32, &fpgam_pic_host_ops, NULL);
	if (fpgam_pic_host == NULL) {
		printk(KERN_ERR "FPGAM PIC: failed to allocate irq host!\n");
		irq = NO_IRQ;
		goto end;
	}

end:
	of_node_put(np);
	return irq;
}

void fpgam_cascade(struct irq_desc *desc)
{
	int cascade_irq = fpgam_get_irq();

	while (cascade_irq >= 0) {
		generic_handle_irq(cascade_irq);
		cascade_irq = fpgam_get_irq();
	}
}

int fpgam_ident(void)
{
	return in_be16(&fpgam_regs->ident);
} 
EXPORT_SYMBOL(fpgam_ident);

void fpgam_reset_eth(void)
{
	out_be16(&fpgam_regs->reset, 0xf8);
	out_be16(&fpgam_regs->reset, 0xff);
	/* 
	 * After the de-assertion of reset, it is recommended to wait a minimum 
 	 * of 100 µs before starting programming on the MIIM (MDC/MDIO) interface.
	 */
	udelay(100);
}
EXPORT_SYMBOL(fpgam_reset_eth);

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
