/*
 * misc setup functions for MPC83xx
 *
 * Maintainer: Kumar Gala <galak@kernel.crashing.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/hw_irq.h>
#include <asm/ipic.h>
#include <asm/qe_ic.h>
#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>
#include <asm-generic/gpio.h>

#include <saf3000/saf3000.h>

#include "mpc83xx.h"
#include "mcr3000_2g.h"
#include "miae.h"

static __be32 __iomem *restart_reg_base;
extern void __init direct16_gpiochip_init(const char *);

static int __init mpc83xx_restart_init(void)
{
	/* map reset restart_reg_baseister space */
	restart_reg_base = ioremap(get_immrbase() + 0x900, 0xff);

	return 0;
}

arch_initcall(mpc83xx_restart_init);

void mpc83xx_restart(char *cmd)
{
#define RST_OFFSET	0x00000900
#define RST_PROT_REG	0x00000018
#define RST_CTRL_REG	0x0000001c

	local_irq_disable();

	if (restart_reg_base) {
		/* enable software reset "RSTE" */
		out_be32(restart_reg_base + (RST_PROT_REG >> 2), 0x52535445);

		/* set software hard reset */
		out_be32(restart_reg_base + (RST_CTRL_REG >> 2), 0x2);
	} else {
		printk (KERN_EMERG "Error: Restart registers not mapped, spinning!\n");
	}

	for (;;) ;
}

long __init mpc83xx_time_init(void)
{
#define SPCR_OFFSET	0x00000110
#define SPCR_TBEN	0x00400000
	__be32 __iomem *spcr = ioremap(get_immrbase() + SPCR_OFFSET, 4);
	__be32 tmp;

	tmp = in_be32(spcr);
	out_be32(spcr, tmp | SPCR_TBEN);

	iounmap(spcr);

	return 0;
}

void __init mpc83xx_ipic_init_IRQ(void)
{
	struct device_node *np;

	/* looking for fsl,pq2pro-pic which is asl compatible with fsl,ipic */
	np = of_find_compatible_node(NULL, NULL, "fsl,ipic");
	if (!np)
		np = of_find_node_by_type(NULL, "ipic");
	if (!np)
		return;

	ipic_init(np, 0);

	of_node_put(np);

	/* Initialize the default interrupt mapping priorities,
	 * in case the boot rom changed something on us.
	 */
	ipic_set_default_priority();
}

#ifdef CONFIG_QUICC_ENGINE
void __init mpc83xx_qe_init_IRQ(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,qe-ic");
	if (!np) {
		np = of_find_node_by_type(NULL, "qeic");
		if (!np)
			return;
	}
	qe_ic_init(np, 0, qe_ic_cascade_low_ipic, qe_ic_cascade_high_ipic);
	of_node_put(np);
}

void __init mpc83xx_ipic_and_qe_init_IRQ(void)
{
	struct device_node *np;
	const char *model = "";
	int irq;
	
	mpc83xx_ipic_init_IRQ();
	mpc83xx_qe_init_IRQ();

	np = of_find_node_by_path("/");
	if (np) {
		model = of_get_property(np, "model", NULL);
		/* MCR3000_2G configuration */
		if (!strcmp(model, "MCR3000_2G")) {
			irq = fpgaf_pic_init();
			if (irq == NO_IRQ)
				printk("@@@ failure !\n");
			if (irq != NO_IRQ)
				irq_set_chained_handler(irq, fpgaf_cascade);
			irq = fpga_pic_init();
			if (irq == NO_IRQ)
				printk("@@@ failure !\n");
			if (irq != NO_IRQ)
				irq_set_chained_handler(irq, fpga_cascade);
		}
		/* MIAE configuration */
		if (!strcmp(model, "MIAE")) {
			irq = fpgam_pic_init();
			if (irq != NO_IRQ)
				irq_set_chained_handler(irq, fpgam_cascade);
		}

		/* if CMPC885 configuration there nothing to do */

	} else {
		printk(KERN_ERR "MODEL: failed to identify model\n");
	}
}
#endif /* CONFIG_QUICC_ENGINE */

static const struct of_device_id of_bus_ids[] __initconst = {
	{ .type = "soc", },
	{ .compatible = "soc", },
	{ .compatible = "simple-bus" },
	{ .compatible = "gianfar" },
	{ .compatible = "gpio-leds", },
	{ .type = "qe", },
	{ .compatible = "fsl,qe", },
	{},
};

int __init mpc83xx_declare_of_platform_devices(void)
{
	struct device_node *np;
	const char *model = "";

	np = of_find_node_by_path("/");
	if (np) {

		model = of_get_property(np, "model", NULL);

		//mpc8xx_early_ping_watchdog();
		proc_mkdir("s3k",0);
		of_platform_bus_probe(NULL, of_bus_ids, NULL);

		/* MCR3000_2G configuration */
		if (!strcmp(model, "MCR3000_2G")) {
			pr_info("%s declare_of_platform_devices()\n", model);
		
			u16_gpiochip_init("s3k,mcr3000-fpga-f-gpio");
			fpgaf_init_platform_devices();
			fpga_init_platform_devices();
			
			fpga_clk_init();
			//cpm1_clk_setup(CPM_CLK_SMC2, CPM_CLK5, CPM_CLK_RTX);

			/* FIXME: hu ? */
			/*mpc8xx_immr = ioremap(get_immrbase(), 0x4000);
			if (!mpc8xx_immr) {
				printk(KERN_CRIT "Could not map IMMR\n");
			} else {
				cpm8xx_t __iomem *cpmp; *//* Pointer to comm processor space *//*
				struct spi_pram *spp;

				cpmp = &mpc8xx_immr->im_cpm;
				spp = (struct spi_pram *)&cpmp->cp_dparam[PROFF_SPI];
				out_be16(&spp->rpbase, 0x1bc0);
			}
			iounmap(mpc8xx_immr);
			*/
		} 
		/* MIAE configuration */
		else if (!strcmp(model, "MIAE")) {
			pr_info("CMM declare_of_platform_devices()\n");
		
			u16_gpiochip_init("s3k,mcr3000-fpga-m-gpio");
			direct16_gpiochip_init("s3k,mcr3000-fpga-m-direct-gpio");
			fpgam_init_platform_devices();
		} 
		/* CMPC885 configuration by default */
		else {
			pr_info("%s declare_of_platform_devices()\n", model);
		
		}
	} else {
		pr_err("MODEL: failed to identify model\n");
	}

	//of_platform_bus_probe(NULL, of_bus_ids, NULL); ??
	return 0;
}

#ifdef CONFIG_PCI
void __init mpc83xx_setup_pci(void)
{
	struct device_node *np;

	for_each_compatible_node(np, "pci", "fsl,mpc8349-pci")
		mpc83xx_add_bridge(np);
	for_each_compatible_node(np, "pci", "fsl,mpc8314-pcie")
		mpc83xx_add_bridge(np);
}
#endif
