/*
 * drivers/mtd/nand/mcr2g.c
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
#include <asm/io.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>

/*
 * MTD structure for MOD885 board
 */
static struct mtd_info *mod885_mtd = NULL;
void __iomem *mod885_fio_base;
void __iomem *mod885_cpld_status;

/*
 * Values specific to the MOD885 board (used with PPC8xx processor)
 */
#define MOD885_FIO_BASE		0x0c000000	/* Address where flash is mapped 			*/
#define MOD885_CPLD_STATUS	0x10000800	/* Address where CPLD is mapped. Used for NAND R/B pin	*/

/*
 * Define partitions for flash device
 */
#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition partition_info[] = {
	{.name		= "Part 1 JFFS2 (systeme)",
	 .size		= (16*1024*1024),
	 .offset	= 0},
	{.name		= "Part 2 JFFS2 (application)",
	 .size		= (48*1024*1024),
	 .offset	= (16*1024*1024)}
};
#endif		//#ifdef CONFIG_MTD_PARTITIONS

#define NUM_PARTITIONS 2

/* 
 *	hardware specific access to control-lines
*/
#define BIT_CLE			((unsigned short)0x0800)
#define BIT_ALE			((unsigned short)0x0400)
#define BIT_NCE			((unsigned short)0x1000)

static void mod885_hwcontrol(struct mtd_info *mtdinfo, int cmd, unsigned int ctrl)
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
static int mod885_device_ready(struct mtd_info *mtd)
{
	/* The NAND FLASH is ready */
	if (*((unsigned short *)(mod885_cpld_status + 0x00000006)) & 0x0002) {
		return (1);
	}
	/* The NAND FLASH is busy */
	return (0);
}

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/*
 * Main initialization routine
 */
int __init mod885_init (void)
{
	int mtd_parts_nb = 0;
	struct nand_chip *this;
	const char *part_type = 0;
	struct mtd_partition *mtd_parts = 0;

	/* Allocate memory for MTD device structure and private data */
	mod885_mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip), GFP_KERNEL);
	if (!mod885_mtd) {
		printk ("Unable to allocate MOD885 NAND MTD device structure.\n");
		return -ENOMEM;
	}

	/* map physical address */
	mod885_fio_base = ioremap(MOD885_FIO_BASE, 0x00000010);
	if (!mod885_fio_base) {
		printk("ioremap MOD885 NAND flash failed\n");
		kfree(mod885_mtd);
		return -EIO;
	}
	mod885_cpld_status = ioremap(MOD885_CPLD_STATUS, 0x00000010);
	if (!mod885_cpld_status) {
		printk("ioremap MOD885 CPLD status for NAND flash failed\n");
		kfree(mod885_mtd);
		return -EIO;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&mod885_mtd[1]);

	/* Initialize structures */
	memset((char *) mod885_mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	mod885_mtd->priv  = this;
	mod885_mtd->owner = THIS_MODULE;
	
	/*
	 * Set GPIO Port D control register so that the pins are configured
	 * to be outputs for controlling the NAND flash.
	 */
	mpc8xx_immr->im_ioport.iop_pddir |=  0x1c00;
	mpc8xx_immr->im_ioport.iop_pdpar &= ~0x1c00;
	mpc8xx_immr->im_ioport.iop_pddat |=  0x1000; /* au repos CE doit etre à 1 */
	mpc8xx_immr->im_ioport.iop_pddat &= ~0x0c00; /* au repos ALE et CLE sont à 0 */
	
	/* insert Callback */
	this->IO_ADDR_R  = mod885_fio_base;
	this->IO_ADDR_W  = mod885_fio_base;
	this->cmd_ctrl 	 = mod885_hwcontrol;
	this->dev_ready  = mod885_device_ready;
	/* 20 us command delay time */
	this->chip_delay = 20;
	this->ecc.mode 	 = NAND_ECC_SOFT;

	/* Scan to find existance of the device */
	if (nand_scan (mod885_mtd, 1)) {
		iounmap(mod885_fio_base);
		iounmap(mod885_cpld_status);
		kfree (mod885_mtd);
		return -ENXIO;
	}

#ifdef CONFIG_MTD_PARTITIONS
	mod885_mtd->name = "mod885-nand";
	mtd_parts_nb = parse_mtd_partitions(mod885_mtd, part_probes, &mtd_parts, 0);
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
	add_mtd_partitions(mod885_mtd, mtd_parts, mtd_parts_nb);

	/* Return happy */
	return 0;
}
module_init(mod885_init);

/*
 * Clean up routine
 */
static void __exit mod885_cleanup(void)
{
	/* Release resources, unregister device */
	nand_release(mod885_mtd);

	/* unmap physical address */
	iounmap(mod885_fio_base);
	iounmap(mod885_cpld_status);

	/* Free the MTD device structure */
	kfree(mod885_mtd);
}

module_exit(mod885_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florent TRINH THAI");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on MOD885 board");
