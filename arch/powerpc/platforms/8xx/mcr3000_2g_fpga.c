/* 
 * arch/powerpc/platforms/8xx/mcr3000_2g_fpga.c
 *
 * Authors : Patrick VASSEUR
 * Copyright (c) 2014  CSSI
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
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>

#include <saf3000/fpga.h>


/*
 * Controleur d'IRQ du FPGA Radio 
 */
static struct fpga *fpga_regs;
static struct irq_domain *fpga_pic_host;

static void fpga_mask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

pr_err("fpga_mask_irq() vec = %d\n", vec);
//	clrbits16(&fpgaf_regs->it_mask, 1<<(15-vec));
}

static void fpga_unmask_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

pr_err("fpga_unmask_irq() vec = %d\n", vec);
//	setbits16(&fpgaf_regs->it_mask, 1<<(15-vec));
}

static void fpga_end_irq(struct irq_data *d)
{
//	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

//pr_err("fpga_end_irq() vec = %d\n", vec);
//	out_be16(&fpgaf_regs->it_ack, ~(1 << (15-vec)));
}

static struct irq_chip fpga_pic = {
	.name = "FPGA PIC",
	.irq_mask = fpga_mask_irq,
	.irq_unmask = fpga_unmask_irq,
	.irq_eoi = fpga_end_irq,
};

int fpga_get_irq(void)
{
	int vec = 0;
	int ret = -1, rssint, rint;
	
	rssint = in_be16(&fpga_regs->mRSSINT_1) << 16;
	rssint = in_be16(&fpga_regs->mRSSINT);
	/* si IT RINT9 */
	if (rssint & IDENT_BIT_RINT9) {
		rint = in_be16(&fpga_regs->mRINT9);
		if (rint & RINT9_UART_MSK) vec = 1;
	}
	if (rssint)
		ret = irq_linear_revmap(fpga_pic_host, vec);

	return ret;
}

static int fpga_pic_host_map(struct irq_domain *h, unsigned int virq, irq_hw_number_t hw)
{
	pr_debug("fpga_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &fpga_pic, handle_fasteoi_irq);
	return 0;
}

static struct irq_domain_ops fpga_pic_host_ops = {
	.map = fpga_pic_host_map,
};


int fpga_pic_init(void)
{
	struct device_node *np = NULL;
	struct resource res;
	unsigned int irq = NO_IRQ, hwirq;
	int ret;

	pr_debug("fpga_pic_init()\n");

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-fpga-pic");
	if (np == NULL) {
		pr_err("fpga_pic_init() : can not find mcr3000-fpga-pic node\n");
		return irq;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_err("fpga_pic_init() : can not find resource address\n");
		goto end;
	}

	fpga_regs = ioremap(res.start, res.end - res.start + 1);
	if (fpga_regs == NULL) {
		pr_err("fpga_pic_init() : can not remap resource address\n");
		goto end;
	}

	irq = irq_of_parse_and_map(np, 0);
	if (irq == NO_IRQ) {
		pr_err("fpga_pic_init() : can not find irq\n");
		goto end;
	}

	/* Initialize the FPGA interrupt controller. */
	hwirq = (unsigned int)virq_to_hw(irq);

	fpga_pic_host = irq_domain_add_linear(np, 2, &fpga_pic_host_ops, NULL);
	if (fpga_pic_host == NULL) {
		pr_err("FPGA RADIO PIC: failed to allocate irq host!\n");
		irq = NO_IRQ;
		goto end;
	}

end:
	of_node_put(np);
	return irq;
}

void fpga_cascade(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int cascade_irq = fpga_get_irq();

	while (cascade_irq >= 0) {
		generic_handle_irq(cascade_irq);
		cascade_irq = fpga_get_irq();
	}

	chip->irq_eoi(&desc->irq_data);
}

/*
 * Init des devices
 */
void fpga_init_platform_devices(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-fpga");
	if (np == NULL)
		pr_err("FPGA-RADIO : compatible node not found\n");
	else
		pr_info("fpga_init_platform_devices() OK\n");
}
