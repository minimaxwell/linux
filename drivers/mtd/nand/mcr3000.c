/*
 * drivers/mtd/nand/mcr3000.c
 *
 * Copyright (C) 2010 CSSI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This is a device driver for the NAND flash device found on the
 *   MCR3000 board which utilizes the Toshiba TC58256FT/DC or Sandisk SDTNE-256
 *   part. This is a 256Mbit NAND flash device.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of_platform.h>
#include <asm/io.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>

/*
 * Driver identification
 */
#define DRV_NAME	"mcr3000"
#define DRV_VERSION	"1.0"
#define DRV_AUTHOR	"Florent TRINH THAI"
#define DRV_DESC	"MCR3000 on-chip NAND FLash Controller Driver"

/*
 * MTD structure for MCR3000 board
 */
static struct mtd_info *mcr3000_mtd = NULL;
void __iomem *mcr3000_fio_base;
void __iomem *mcr3000_cpld_status;

/*
 * GPIO declaration
 */
#define MCR3000_NB_GPIO 3

/*
 * Values specific to the MCR3000 board (used with PPC8xx processor)
 */
#define MCR3000_FIO_BASE	0x0c000000	/* Address where flash is mapped 			*/
#define MCR3000_CPLD_STATUS	0x10000800	/* Address where CPLD is mapped. Used for NAND R/B pin	*/

/*
 * Define partitions for flash device
 */
#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition partition_info[] = {
	{.name		= "Part 1 JFFS2 (systeme)",
	 .size		= (12*1024*1024),
	 .offset	= 0},
	{.name		= "Part 2 JFFS2 (application)",
	 .size		= (MTDPART_SIZ_FULL),
	 .offset	= (MTDPART_OFS_APPEND)}
};
#endif		//#ifdef CONFIG_MTD_PARTITIONS

#define NUM_PARTITIONS 2


///* Select the chip by setting nCE to low */
//#define NAND_CTL_SETNCE		1
///* Deselect the chip by setting nCE to high */
//#define NAND_CTL_CLRNCE		2
///* Select the command latch by setting CLE to high */
//#define NAND_CTL_SETCLE		3
///* Deselect the command latch by setting CLE to low */
//#define NAND_CTL_CLRCLE		4
///* Select the address latch by setting ALE to high */
//#define NAND_CTL_SETALE		5
///* Deselect the address latch by setting ALE to low */
//#define NAND_CTL_CLRALE		6

//static void mcr3000_hwcontrol(struct mtd_info *mtd, int cmd)
//{
//	register struct nand_chip *tthis = mtd->priv;
//
//	switch (cmd) {
//
//	case NAND_CTL_SETCLE:
//		tthis->IO_ADDR_W = p_nand + MEM_STNAND_CMD;
//		break;
//
//	case NAND_CTL_CLRCLE:
//		tthis->IO_ADDR_W = p_nand + MEM_STNAND_DATA;
//		break;
//
//	case NAND_CTL_SETALE:
//		tthis->IO_ADDR_W = p_nand + MEM_STNAND_ADDR;
//		break;
//
//	case NAND_CTL_CLRALE:
//		tthis->IO_ADDR_W = p_nand + MEM_STNAND_DATA;
//		/* FIXME: Nobody knows why this is necessary,
//		 * but it works only that way */
//		udelay(1);
//		break;
//
//	case NAND_CTL_SETNCE:
//		/* assert (force assert) chip enable */
//		au_writel((1 << (4 + NAND_CS)), MEM_STNDCTL);
//		break;
//
//	case NAND_CTL_CLRNCE:
//		/* deassert chip enable */
//		au_writel(0, MEM_STNDCTL);
//		break;
//	}
//
//	tthis->IO_ADDR_R = tthis->IO_ADDR_W;
//
//	/* Drain the writebuffer */
//	au_sync();
//}

/* 
 *	hardware specific access to control-lines
*/
#define BIT_CLE			((unsigned short)0x0800)
#define BIT_ALE			((unsigned short)0x0400)
#define BIT_NCE			((unsigned short)0x1000)

static void mcr3000_hwcontrol(struct mtd_info *mtdinfo, int cmd, unsigned int ctrl)
{
	struct nand_chip *this 	= mtdinfo->priv;
	unsigned short pddat 	= 0;


	/* The hardware control change */
	if (ctrl & NAND_CTRL_CHANGE) {

		/* saving current value */
		pddat = mpc8xx_immr->im_ioport.iop_pddat;

		/* Clearing ALE and CLE */
		pddat &= ~(BIT_CLE | BIT_ALE);

		/* Driving NCE pin */
		if (ctrl & NAND_NCE) {
			pddat &= ~BIT_NCE;
		} else {
			pddat |= BIT_NCE;
		}

		/* Driving CLE and ALE pin */
		if (ctrl & NAND_CLE) {
			pddat |= BIT_CLE;
		}
		if (ctrl & NAND_ALE) {
			pddat |= BIT_ALE;
		}

		/* applying new value */
		mpc8xx_immr->im_ioport.iop_pddat = pddat;
	}

	/* Writing the command */
	if (cmd != NAND_CMD_NONE) {
		*((unsigned char *)this->IO_ADDR_W) = (unsigned char)cmd;
	}
}

/*
 *	read device ready pin
 */
static int mcr3000_device_ready(struct mtd_info *mtd)
{
	/* The NAND FLASH is ready */
	if (*((unsigned short *)(mcr3000_cpld_status + 0x00000006)) & 0x0002) {
		return (1);
	}
	/* The NAND FLASH is busy */
	return (0);
}

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/*
 * mcr3000_remove
 */
static int __devexit mcr3000_remove(struct of_device *ofdev)
{
	/* Release resources, unregister device */
	nand_release(mcr3000_mtd);

	/* unmap physical address */
	iounmap(mcr3000_fio_base);
	iounmap(mcr3000_cpld_status);

	/* Free the MTD device structure */
	kfree(mcr3000_mtd);
	
	return 0;
}

/*
 * mcr3000_probe
 */
static int __devinit mcr3000_probe(struct of_device *ofdev, const struct of_device_id *ofid)
{
	int mtd_parts_nb = 0;
	struct nand_chip *this;
	const char *part_type = 0;
	struct mtd_partition *mtd_parts = 0;



	printk ("Probing NAND FLASH.\n");


	/* Allocate memory for MTD device structure and private data */
	mcr3000_mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip), GFP_KERNEL);
	if (!mcr3000_mtd) {
		printk ("Unable to allocate MCR3000 NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* map physical address */
	mcr3000_fio_base = ioremap(MCR3000_FIO_BASE, 0x00000010);
	if (!mcr3000_fio_base) {
		printk("ioremap MCR3000 NAND flash failed\n");
		kfree(mcr3000_mtd);
		return -EIO;
	}
	mcr3000_cpld_status = ioremap(MCR3000_CPLD_STATUS, 0x00000010);
	if (!mcr3000_cpld_status) {
		printk("ioremap MCR3000 CPLD status for NAND flash failed\n");
		kfree(mcr3000_mtd);
		return -EIO;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&mcr3000_mtd[1]);

	/* Initialize structures */
	memset((char *) mcr3000_mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	mcr3000_mtd->priv  = this;
	mcr3000_mtd->owner = THIS_MODULE;
	
	/* GPIO definition */
//	if (ngpios < MCR3000_NB_GPIO) {
//		dev_err(dev, "missing GPIO definition in device tree\n");
//		_Result = -EINVAL;
//		goto ERREUR;
//	}
//	for (i = 0; i < M_DRV_ALM_NB_GPIO; i++) {
//		int gpio = ldb_gpio_init(np, dev, i, dir[i]);
//		if (gpio == -1) {
//			dev_err(dev, "problem with GPIO init\n");
//			_Result = -EINVAL;
//			goto ERREUR;
//		}
//		stAlmSuivi.mGpio[i] = gpio;
//	}	

	/*
	 * Set GPIO Port D control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	mpc8xx_immr->im_ioport.iop_pddir |=  0x1c00;
	mpc8xx_immr->im_ioport.iop_pdpar &= ~0x1c00;
	mpc8xx_immr->im_ioport.iop_pddat |=  0x1000; /* au repos CE doit etre à 1 */
	mpc8xx_immr->im_ioport.iop_pddat &= ~0x0c00; /* au repos ALE et CLE sont à 0 */
	
	/* insert Callback */
	this->IO_ADDR_R  = mcr3000_fio_base;
	this->IO_ADDR_W  = mcr3000_fio_base;
	this->cmd_ctrl 	 = mcr3000_hwcontrol;
	this->dev_ready  = mcr3000_device_ready;
	/* 20 us command delay time */
	this->chip_delay = 20;
	this->ecc.mode 	 = NAND_ECC_SOFT;

	/* Scan to find existance of the device */
	if (nand_scan (mcr3000_mtd, 1)) {
		iounmap(mcr3000_fio_base);
		iounmap(mcr3000_cpld_status);
		kfree (mcr3000_mtd);
		return -ENXIO;
	}

#ifdef CONFIG_MTD_PARTITIONS
	mcr3000_mtd->name = "mcr3000-nand";
	mtd_parts_nb = parse_mtd_partitions(mcr3000_mtd, part_probes, &mtd_parts, 0);
	if (mtd_parts_nb > 0) {
		part_type = "command line";
	} else {
		mtd_parts_nb = 0;
	}
#endif
	if (mtd_parts_nb == 0) {
		mtd_parts = partition_info;
		mtd_parts_nb = NUM_PARTITIONS;
		part_type = "static";
	}

	/* Register the partitions */
	printk(KERN_NOTICE "Using %s partition definition\n", part_type);
	add_mtd_partitions(mcr3000_mtd, mtd_parts, mtd_parts_nb);
	
	
	
	printk ("NAND FLASH probed.\n");
	
	
	
	/* Return happy */
	return 0;
}

static const struct of_device_id mcr3000_match[] =
{
	{
		.compatible   = "s3k,mcr3000-nand",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mcr3000_match);

/*
 * driver device registration
 */
static struct of_platform_driver mcr3000_driver = {
	.probe		= mcr3000_probe,
	.remove		= __devexit_p(mcr3000_remove),
	.driver		=
	{
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= mcr3000_match,
	},
};

/*
 * Main initialization routine
 */
int __init mcr3000_init (void)
{

	printk ("mcr3000_init\n");


	return of_register_platform_driver(&mcr3000_driver);
}

/*
 * Clean up routine
 */
static void __exit mcr3000_cleanup(void)
{
	of_unregister_platform_driver(&mcr3000_driver);
}

module_init(mcr3000_init);
module_exit(mcr3000_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_ALIAS("platform:" DRV_NAME);
