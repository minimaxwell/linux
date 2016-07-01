/*
 * FIXME
 * drivers/mtd/nand/mpcpro.c
 *
 * Copyright (C) 2010 CSSI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This is a device driver for the NAND flash device found on the
 *   MPCPRO board which utilizes the Toshiba TC58256FT/DC or Sandisk SDTNE-256
 *   part.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of_platform.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <asm/io.h>
#include <asm/fs_pd.h>
#include <linux/of_address.h>

/*
 * Driver identification
 */
#define DRV_NAME	"mpcpro-nand"
#define DRV_VERSION	"1.0"
#define DRV_AUTHOR	"Florent TRINH THAI"
#define DRV_DESC	"MPCPRO on-chip NAND FLash Controller Driver"

/*
 * Structure for MTD MPCPRO NAND chip
 */
typedef enum
{
	NAND_GPIO_CLE = 0,
	NAND_GPIO_ALE,
	NAND_GPIO_RDY,
	NB_NAND_GPIO
} e_nand_gpio;

struct mpcpro_host
{
	struct nand_chip	nand_chip;
	struct mtd_info		mtd;
	void __iomem		*io_base;
	struct device		*dev;
	int			gpio[NB_NAND_GPIO];
};

extern int par_io_data_set(u8 port, u8 pin, u8 val);
extern int par_io_data_get(u8 port, u8 pin); 
extern int par_io_config_pin(u8 port, u8 pin, int dir, int open_drain, int assignment, int has_irq);
 
/*
 * Values specific to the MPCPRO board (used with PPC8xx processor)
 */
#define MPCPRO_FIO_BASE		0xA0000000	/* Address where flash is mapped 			*/

/*
 * Define partitions for flash device
 */
static struct mtd_partition partition_info[] = {
	{.name		= "System",
	 .size		= (MTDPART_SIZ_FULL),
	 .offset	= (MTDPART_OFS_APPEND)}
};

#define NUM_PARTITIONS 1


/* 
 *	hardware specific access to control-lines
*/
static void mpcpro_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *nand_chip 	= mtd->priv;
	struct mpcpro_host *host	= nand_chip->priv;
	u32 pin_mask, tmp_val;
	
	/* The hardware control change */
	if (ctrl & NAND_CTRL_CHANGE) {

		/* Driving NCE pin */
		if (ctrl & NAND_NCE) {

			/* Driving CLE and ALE pin */
			if (ctrl & NAND_CLE) {
				par_io_data_set(2, 25, 1);
			} else {
				par_io_data_set(2, 25, 0);
			}
			if (ctrl & NAND_ALE) {
				par_io_data_set(2, 26, 1);
			} else {
				par_io_data_set(2, 26, 0);
			}

		} else {
			par_io_data_set(2, 25, 0);
			par_io_data_set(2, 26, 0);
		}
	}
	
	/* Writing the command */
	if (cmd != NAND_CMD_NONE) {
		*((unsigned char *)host->io_base) = (unsigned char)cmd;
	}
}


/*
 *	read device ready pin
 */
static int mpcpro_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *nand_chip 	= mtd->priv;
	struct mpcpro_host *host	= nand_chip->priv;
	
	/* read nand flash rdy pin */
	return par_io_data_get(2, 24);
}

/* un peu tordu comme facon de faire, il faut trouver mieux */
#ifndef CONFIG_MTD_NAND_MCR3000
const char *part_probes[] = { "cmdlinepart", NULL };
#else
extern const char *part_probes[];
#endif


/*
 * mpcpro_remove
 */
static int mpcpro_remove(struct platform_device *ofdev)
{
	struct mpcpro_host *host = dev_get_drvdata(&ofdev->dev);
	struct mtd_info *mtd = &host->mtd;
	int i;

	nand_release(mtd);
	/*for (i=0; i<NB_NAND_GPIO ; i++) {
		if (gpio_is_valid(host->gpio[i]))
			gpio_free(host->gpio[i]);
	}*/
	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(host->io_base);
	kfree(host);
	
	return 0;
}

/*
 * mpcpro_probe
 */
static int mpcpro_probe(struct platform_device *ofdev)
{
	struct mpcpro_host *host;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
	int res = 0;
	int i;

	/* Allocate memory for the device structure (and zero it) */
	host = kzalloc(sizeof(struct mpcpro_host), GFP_KERNEL);
	if (!host) {
		dev_err(&ofdev->dev, "failed to allocate device structure.\n");
		res = -ENOMEM;
		goto MEMALLOC_ERROR;
	}

	host->io_base = of_iomap(ofdev->dev.of_node, 0);
	if (host->io_base == NULL) {
		dev_err(&ofdev->dev, "ioremap for NAND FLASH failed\n");
		res = -EIO;
		goto IOREMAP_NAND_ERROR;
	}
	
	mtd = &host->mtd;
	nand_chip = &host->nand_chip;
	host->dev = &ofdev->dev;

	nand_chip->priv = host;		/* link the private data structures */
	mtd->priv = nand_chip;
	mtd->name = "mpcpro-nand";
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &ofdev->dev;

	/* nand address base */
	nand_chip->IO_ADDR_R = (void *)host->io_base;
	nand_chip->IO_ADDR_W = (void *)host->io_base;

	nand_chip->cmd_ctrl 	= mpcpro_hwcontrol;
	nand_chip->dev_ready 	= mpcpro_device_ready;
	nand_chip->ecc.mode 	= NAND_ECC_SOFT;	/* enable ECC */
	nand_chip->chip_delay 	= 60;			/* 20us command delay time */

	dev_set_drvdata(&ofdev->dev, host);
	
	/*
	 * Set GPIO Port D control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	//FIXME
	//hard settings

	par_io_config_pin(2, 24, 2, 0, 0, 0); /* RDY */
	par_io_config_pin(2, 25, 1, 0, 0, 0); /* CLE */
	par_io_config_pin(2, 26, 1, 0, 0, 0); /* ALE */

	par_io_data_set(2, 25, 0);
	par_io_data_set(2, 25, 0);

//	if (of_gpio_count((&ofdev->dev)->of_node) != NB_NAND_GPIO) {
//		dev_err(&ofdev->dev, "missing GPIO definition in device tree\n");
//		res = -EINVAL;
//		goto GPIO_COUNT_ERROR;
//	}
//	for (i=0; i<NB_NAND_GPIO; i++) {
//		host->gpio[i] = of_get_gpio((&ofdev->dev)->of_node, i);
//		if (!gpio_is_valid(host->gpio[i])) {
//			dev_err(&ofdev->dev, "invalid gpio #%d: %d\n", i, host->gpio[i]);
//			res = -EINVAL;
//			goto GPIO_ERROR;
//		}
//
//		res = gpio_request(host->gpio[i], dev_driver_string((struct device *)&ofdev->dev));
//		if (res) {
//			dev_err(&ofdev->dev, "can't request gpio #%d: %d\n", i, res);
//			res = -EINVAL;
//			goto GPIO_ERROR;
//		}
//
//		/* for NCE the default state is 1 */
//		if (i == NAND_GPIO_NCE) {
//			res = gpio_direction_output(host->gpio[i], 1);
//			
//		/* for all other IO the default state is 0 */
//		} else {
//			res = gpio_direction_output(host->gpio[i], 0);
//		}
//		
//		if (res) {
//			dev_err(&ofdev->dev,"can't set direction for gpio #%d: %d\n", i, res);
//			res = -EINVAL;
//			goto GPIO_ERROR;
//		}
//	}
//	
	/* first scan to find the device and get the page size */
	if (nand_scan_ident(mtd, 1, NULL) < 0) {
		dev_err(&ofdev->dev, "unable to scan NAND FLASH identity\n");
		res = -ENXIO;
		goto GPIO_ERROR;
	}

	/* second phase scan */
	if (nand_scan_tail(mtd)) {
		dev_err(&ofdev->dev, "unable to scan NAND FLASH integrity\n");
		res = -ENXIO;
		goto GPIO_ERROR;
	}

	if (mtd_device_parse_register(mtd, part_probes, 0, partitions, num_partitions)) {
		dev_err(&ofdev->dev, "unable to add mtd partition\n");
		res = -EINVAL;
		goto PARTITION_ERROR;
	}
	
	/* we are happy */
	return 0;
	
PARTITION_ERROR:
	nand_release(mtd);
	
GPIO_ERROR:
	/*for (i=0; i<NB_NAND_GPIO ; i++) {
		if (gpio_is_valid(host->gpio[i]))
			gpio_free(host->gpio[i]);
	}*/
	
GPIO_COUNT_ERROR:
	dev_set_drvdata(&ofdev->dev, NULL);
	
IOREMAP_CPLD_ERROR:
	iounmap(host->io_base);
	
IOREMAP_NAND_ERROR:
	kfree(host);
	
MEMALLOC_ERROR:
	return res;
}


static const struct of_device_id mpcpro_match[] =
{
	{
		.compatible   = "s3k,mpcpro-nand",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mpcpro_match);

/*
 * driver device registration
 */
static struct platform_driver mpcpro_driver = {
	.probe		= mpcpro_probe,
	.remove		= mpcpro_remove,
	.driver		=
	{
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= mpcpro_match,
	},
};

/*
 * Main initialization routine
 */
int __init mpcpro_init (void)
{
	return platform_driver_register(&mpcpro_driver);
}

/*
 * Clean up routine
 */
static void __exit mpcpro_cleanup(void)
{
	platform_driver_unregister(&mpcpro_driver);
}

module_init(mpcpro_init);
module_exit(mpcpro_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_ALIAS("platform:" DRV_NAME);
