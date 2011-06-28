/*
 * drivers/mtd/nand/mod885.c
 *
 * Copyright (C) 2010 CSSI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This is a device driver for the NAND flash device found on the
 *   MOD885 board which utilizes the Toshiba TC58256FT/DC or Sandisk SDTNE-256
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
#include <asm/cpm1.h>
#include <asm/fs_pd.h>


/*
 * Driver identification
 */
#define DRV_NAME	"mod885-nand"
#define DRV_VERSION	"1.0"
#define DRV_AUTHOR	"Florent TRINH THAI"
#define DRV_DESC	"MOD885 on-chip NAND FLash Controller Driver"

/*
 * Structure for MTD MOD885 NAND chip
 */
typedef enum
{
	NAND_GPIO_CLE = 0,
	NAND_GPIO_ALE,
	NAND_GPIO_NCE,
	NB_NAND_GPIO
} e_nand_gpio;

struct mod885_host
{
	struct nand_chip	nand_chip;
	struct mtd_info		mtd;
	void __iomem		*io_base;
	void __iomem		*cpld_base;
	struct device		*dev;
	int			gpio[NB_NAND_GPIO];
};

/*
 * Values specific to the MOD885 board (used with PPC8xx processor)
 */
#define MOD885_FIO_BASE		0xC0000000	/* Address where flash is mapped 			*/
#define MOD885_CPLD_STATUS	0xC8000000	/* Address where CPLD is mapped. Used for NAND R/B pin	*/

/*
 * Define partitions for flash device
 */
#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition partition_info[] = {
	{.name		= "System",
	 .size		= (MTDPART_SIZ_FULL),
	 .offset	= (MTDPART_OFS_APPEND)}
};
#endif		//#ifdef CONFIG_MTD_PARTITIONS

#define NUM_PARTITIONS 1


/* 
 *	hardware specific access to control-lines
*/
static void mod885_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *nand_chip 	= mtd->priv;
	struct mod885_host *host	= nand_chip->priv;
	
	/* The hardware control change */
	if (ctrl & NAND_CTRL_CHANGE) {

		/* Driving NCE pin */
		if (ctrl & NAND_NCE) {
			gpio_set_value(host->gpio[NAND_GPIO_NCE], 0);

			/* Driving CLE and ALE pin */
			if (ctrl & NAND_CLE) {
				gpio_set_value(host->gpio[NAND_GPIO_CLE], 1);
			} else {
				gpio_set_value(host->gpio[NAND_GPIO_CLE], 0);
			}
			if (ctrl & NAND_ALE) {
				gpio_set_value(host->gpio[NAND_GPIO_ALE], 1);
			} else {
				gpio_set_value(host->gpio[NAND_GPIO_ALE], 0);
			}

		} else {
			gpio_set_value(host->gpio[NAND_GPIO_NCE], 1);
			gpio_set_value(host->gpio[NAND_GPIO_ALE], 0);
			gpio_set_value(host->gpio[NAND_GPIO_CLE], 0);
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
static int mod885_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *nand_chip 	= mtd->priv;
	struct mod885_host *host	= nand_chip->priv;
	
	/* The NAND FLASH is ready */
	if (*((unsigned short *)(host->cpld_base + 0x00000002)) & 0x0004) {
		return (1);
	}
	/* The NAND FLASH is busy */
	return (0);
}

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif


/*
 * mod885_remove
 */
static int __devexit mod885_remove(struct platform_device *ofdev)
{
	struct mod885_host *host = dev_get_drvdata(&ofdev->dev);
	struct mtd_info *mtd = &host->mtd;
	int i;

	nand_release(mtd);
	for (i=0; i<NB_NAND_GPIO ; i++) {
		if (gpio_is_valid(host->gpio[i]))
			gpio_free(host->gpio[i]);
	}
	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(host->io_base);
	iounmap(host->cpld_base);
	kfree(host);
	
	return 0;
}

/*
 * mod885_probe
 */
static int __devinit mod885_probe(struct platform_device *ofdev, const struct of_device_id *ofid)
{
	struct mod885_host *host;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
	int res = 0;
	int i;

	/* Allocate memory for the device structure (and zero it) */
	host = kzalloc(sizeof(struct mod885_host), GFP_KERNEL);
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
	
	host->cpld_base = ioremap(MOD885_CPLD_STATUS, 0x00000010);
	if (host->cpld_base == NULL) {
		dev_err(&ofdev->dev, "ioremap for CPLD failed\n");
		res = -EIO;
		goto IOREMAP_CPLD_ERROR;
	}

	mtd = &host->mtd;
	nand_chip = &host->nand_chip;
	host->dev = &ofdev->dev;

	nand_chip->priv = host;		/* link the private data structures */
	mtd->priv = nand_chip;
	mtd->name = "mod885-nand";
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &ofdev->dev;

	/* nand address base */
	nand_chip->IO_ADDR_R = (void *)host->io_base;
	nand_chip->IO_ADDR_W = (void *)host->io_base;

	nand_chip->cmd_ctrl 	= mod885_hwcontrol;
	nand_chip->dev_ready 	= mod885_device_ready;
	nand_chip->ecc.mode 	= NAND_ECC_SOFT;	/* enable ECC */
	nand_chip->chip_delay 	= 20;			/* 20us command delay time */

	dev_set_drvdata(&ofdev->dev, host);
	
	/*
	 * Set GPIO Port D control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	if (of_gpio_count((&ofdev->dev)->of_node) != NB_NAND_GPIO) {
		dev_err(&ofdev->dev, "missing GPIO definition in device tree\n");
		res = -EINVAL;
		goto GPIO_COUNT_ERROR;
	}
	for (i=0; i<NB_NAND_GPIO; i++) {
		host->gpio[i] = of_get_gpio((&ofdev->dev)->of_node, i);
		if (!gpio_is_valid(host->gpio[i])) {
			dev_err(&ofdev->dev, "invalid gpio #%d: %d\n", i, host->gpio[i]);
			res = -EINVAL;
			goto GPIO_ERROR;
		}

		res = gpio_request(host->gpio[i], dev_driver_string((struct device *)&ofdev->dev));
		if (res) {
			dev_err(&ofdev->dev, "can't request gpio #%d: %d\n", i, res);
			res = -EINVAL;
			goto GPIO_ERROR;
		}

		/* for NCE the default state is 1 */
		if (i == NAND_GPIO_NCE) {
			res = gpio_direction_output(host->gpio[i], 1);
			
		/* for all other IO the default state is 0 */
		} else {
			res = gpio_direction_output(host->gpio[i], 0);
		}
		
		if (res) {
			dev_err(&ofdev->dev,"can't set direction for gpio #%d: %d\n", i, res);
			res = -EINVAL;
			goto GPIO_ERROR;
		}
	}
	
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

#ifdef CONFIG_MTD_PARTITIONS
	num_partitions = parse_mtd_partitions(mtd, part_probes, &partitions, 0);
	if (num_partitions < 0) {
		dev_err(&ofdev->dev, "partitions undeclared\n");
		res = num_partitions;
		goto PARTITION_ERROR;
	}
#endif
	if (num_partitions == 0) {
		partitions 	= partition_info;
		num_partitions 	= NUM_PARTITIONS;
		dev_notice(&ofdev->dev, "Using static partition definition\n");
	}
	
	if (add_mtd_partitions(mtd, partitions, num_partitions)) {
		dev_err(&ofdev->dev, "unable to add mtd partition\n");
		res = -EINVAL;
		goto PARTITION_ERROR;
	}
	
	/* we are happy */
	return 0;
	
PARTITION_ERROR:
	nand_release(mtd);
	
GPIO_ERROR:
	for (i=0; i<NB_NAND_GPIO ; i++) {
		if (gpio_is_valid(host->gpio[i]))
			gpio_free(host->gpio[i]);
	}
	
GPIO_COUNT_ERROR:
	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(host->cpld_base);
	
IOREMAP_CPLD_ERROR:
	iounmap(host->io_base);
	
IOREMAP_NAND_ERROR:
	kfree(host);
	
MEMALLOC_ERROR:
	return res;
}


static const struct of_device_id mod885_match[] =
{
	{
		.compatible   = "s3k,mod885-nand",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mod885_match);

/*
 * driver device registration
 */
static struct of_platform_driver mod885_driver = {
	.probe		= mod885_probe,
	.remove		= __devexit_p(mod885_remove),
	.driver		=
	{
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= mod885_match,
	},
};

/*
 * Main initialization routine
 */
int __init mod885_init (void)
{
	return of_register_platform_driver(&mod885_driver);
}

/*
 * Clean up routine
 */
static void __exit mod885_cleanup(void)
{
	of_unregister_platform_driver(&mod885_driver);
}

module_init(mod885_init);
module_exit(mod885_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_ALIAS("platform:" DRV_NAME);
