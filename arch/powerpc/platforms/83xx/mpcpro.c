/*
 * arch/powerpc/platforms/83xx/mpcpro.c
 *
 * Copyright (C) Freescale Semiconductor, Inc. 2007. All rights reserved.
 *
 * Copyright (C) CS Systemes d'Information, Inc. 2015. All rights reserved.
 *
 * Description:
 * MPCPRO CS board specific routines.
 * This file is based on mpc832x_mds.c and mpc8313_rdb.c
 * Author: Michael Barkowski <michael.barkowski@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/spi/spi.h>
#include <linux/spi/mmc_spi.h>
#include <linux/mmc/host.h>
#include <linux/of_platform.h>
#include <linux/fsl_devices.h>
#include <linux/proc_fs.h>

#include <asm/time.h>
#include <asm/ipic.h>
#include <asm/udbg.h>
#include <asm/qe.h>
#include <asm/qe_ic.h>
#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>
#include <asm-generic/gpio.h>

#include <saf3000/saf3000.h>

#include "mpc83xx.h"
#include "mcr3000_2g.h"
#include "miae.h"

extern void __init direct16_gpiochip_init(const char *);

#undef DEBUG
#ifdef DEBUG
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

#ifdef CONFIG_QUICC_ENGINE
static int __init of_fsl_spi_probe(char *type, char *compatible, u32 sysclk,
				   struct spi_board_info *board_infos,
				   unsigned int num_board_infos,
				   void (*cs_control)(struct spi_device *dev,
						      bool on))
{
	struct device_node *np;
	unsigned int i = 0;

	for_each_compatible_node(np, type, compatible) {
		int ret;
		unsigned int j;
		const void *prop;
		struct resource res[2];
		struct platform_device *pdev;
		struct fsl_spi_platform_data pdata = {
			.cs_control = cs_control,
		};

		memset(res, 0, sizeof(res));

		pdata.sysclk = sysclk;

		prop = of_get_property(np, "reg", NULL);
		if (!prop)
			goto err;
		pdata.bus_num = *(u32 *)prop;

		prop = of_get_property(np, "cell-index", NULL);
		if (prop)
			i = *(u32 *)prop;

		prop = of_get_property(np, "mode", NULL);
		if (prop && !strcmp(prop, "cpu-qe"))
			pdata.flags = SPI_QE_CPU_MODE;

		for (j = 0; j < num_board_infos; j++) {
			if (board_infos[j].bus_num == pdata.bus_num)
				pdata.max_chipselect++;
		}

		if (!pdata.max_chipselect)
			continue;

		ret = of_address_to_resource(np, 0, &res[0]);
		if (ret)
			goto err;

		ret = of_irq_to_resource(np, 0, &res[1]);
		if (ret == NO_IRQ)
			goto err;

		pdev = platform_device_alloc("mpc83xx_spi", i);
		if (!pdev)
			goto err;

		ret = platform_device_add_data(pdev, &pdata, sizeof(pdata));
		if (ret)
			goto unreg;

		ret = platform_device_add_resources(pdev, res,
						    ARRAY_SIZE(res));
		if (ret)
			goto unreg;

		ret = platform_device_add(pdev);
		if (ret)
			goto unreg;

		goto next;
unreg:
		platform_device_del(pdev);
err:
		pr_err("%s: registration failed\n", np->full_name);
next:
		i++;
	}

	return i;
}

static int __init fsl_spi_init(struct spi_board_info *board_infos,
			       unsigned int num_board_infos,
			       void (*cs_control)(struct spi_device *spi,
						  bool on))
{
	u32 sysclk = -1;
	int ret;

	/* SPI controller is either clocked from QE or SoC clock */
	sysclk = get_brgfreq();
	if (sysclk == -1) {
		sysclk = fsl_get_sys_freq();
		if (sysclk == -1)
			return -ENODEV;
	}

	ret = of_fsl_spi_probe(NULL, "fsl,spi", sysclk, board_infos,
			       num_board_infos, cs_control);
	if (!ret)
		of_fsl_spi_probe("spi", "fsl_spi", sysclk, board_infos,
				 num_board_infos, cs_control);

	return spi_register_board_info(board_infos, num_board_infos);
}

static void cmpcpro_spi_cs_control(struct spi_device *spi, bool on)
{
	//pr_debug("%s %d %d\n", __func__, spi->chip_select, on);
	//par_io_data_set(3, 13, on);
}

static struct mmc_spi_platform_data cmpcpro_mmc_pdata = {
	.ocr_mask = MMC_VDD_33_34,
};

static struct spi_board_info cmpcpro_spi_boardinfo = {
	.bus_num = 0x4c0,
	.chip_select = 0,
	.max_speed_hz = 50000000,
	.modalias = "mmc_spi",
	.platform_data = &cmpcpro_mmc_pdata,
};

static int __init cmpcpro_spi_init(void)
{
	par_io_config_pin(3,  0, 3, 0, 1, 0); /* SPI1 MOSI, I/O */
	par_io_config_pin(3,  1, 3, 0, 1, 0); /* SPI1 MISO, I/O */
	par_io_config_pin(3,  2, 3, 0, 1, 0); /* SPI1 CLK,  I/O */
//	par_io_config_pin(3,  3, 2, 0, 1, 0); /* SPI1 SEL,  I   */

//	par_io_config_pin(3, 13, 1, 0, 0, 0); /* !SD_CS,    O */
//	par_io_config_pin(3, 14, 2, 0, 0, 0); /* SD_INSERT, I */
//	par_io_config_pin(3, 15, 2, 0, 0, 0); /* SD_PROTECT,I */
	

	/*
	 * Don't bother with legacy stuff when device tree contains
	 * mmc-spi-slot node.
	 */
	//if (of_find_compatible_node(NULL, NULL, "mmc-spi-slot"))
	//	return 0;
	return fsl_spi_init(&cmpcpro_spi_boardinfo, 1, cmpcpro_spi_cs_control);
}
machine_device_initcall(cmpcpro, cmpcpro_spi_init);

void __init cmpcpro_ipic_and_qe_init_IRQ(void)
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
			if (irq != NO_IRQ)
				irq_set_chained_handler(irq, fpgaf_cascade);
			irq = fpga_pic_init();
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

int __init cmpcpro_declare_of_platform_devices(void)
{
	struct device_node *np;
	const char *model = "";

	np = of_find_node_by_path("/");
	if (np) {
		model = of_get_property(np, "model", NULL);

		//mpc8xx_early_ping_watchdog();
		proc_mkdir("s3k",0);
		of_platform_bus_probe(NULL, of_bus_ids, NULL);

		if (!strcmp(model, "MCR3000_2G")) {
			/* MCR3000_2G configuration */
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
		} else if (!strcmp(model, "MIAE")) {
			/* MIAE configuration */
			pr_info("CMM declare_of_platform_devices()\n");

			u16_gpiochip_init("s3k,mcr3000-fpga-m-gpio");
			direct16_gpiochip_init("s3k,mcr3000-fpga-m-direct-gpio");
			fpgam_init_platform_devices();
		} else {
			/* CMPC885 configuration by default */
			pr_info("%s declare_of_platform_devices()\n", model);
		}
	} else {
		pr_err("MODEL: failed to identify model\n");
	}

	//of_platform_bus_probe(NULL, of_bus_ids, NULL); ??
	return 0;
}

/* ************************************************************************
 *
 * Setup the architecture
 *
 */
static void __init cmpcpro_setup_arch(void)
{
#if defined(CONFIG_QUICC_ENGINE)
	struct device_node *np;
#endif

	if (ppc_md.progress)
		ppc_md.progress("mpc832x_rdb_setup_arch()", 0);

	mpc83xx_setup_pci();

#ifdef CONFIG_QUICC_ENGINE
	qe_reset();

	if ((np = of_find_node_by_name(NULL, "par_io")) != NULL) {
		par_io_init(np);
		of_node_put(np);

		for (np = NULL; (np = of_find_node_by_name(np, "ucc")) != NULL;)
			par_io_of_config(np);
	}
#endif				/* CONFIG_QUICC_ENGINE */
}

machine_device_initcall(cmpcpro, cmpcpro_declare_of_platform_devices);

/*
 * Called very early, MMU is off, device-tree isn't unflattened
 */
static int __init cmpcpro_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	return of_flat_dt_is_compatible(root, "CMPCPRO");
}

define_machine(cmpcpro) {
	.name		= "CMPCPRO",
	.probe		= cmpcpro_probe,
	.setup_arch	= cmpcpro_setup_arch,
	.init_IRQ	= cmpcpro_ipic_and_qe_init_IRQ,
	.get_irq	= ipic_get_irq,
	.restart	= mpc83xx_restart,
	.time_init	= mpc83xx_time_init,
	.calibrate_decr	= generic_calibrate_decr,
	.progress	= udbg_progress,
};
