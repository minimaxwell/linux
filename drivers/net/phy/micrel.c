/*
 * drivers/net/phy/micrel.c
 *
 * Driver for Micrel PHYs
 *
 * Author: David J. Choi
 *
 * Copyright (c) 2010 Micrel, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support : ksz9021 , vsc8201, ks8001
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>

/* #define DEBUG_MICREL 1 */

#define	PHY_ID_KSZ9021			0x00221611
#define	PHY_ID_VSC8201			0x000FC413
#define	PHY_ID_KS8001			0x0022161A
#define	PHY_ID_KSZ8041			0x00221513

/*
 * Register declaration
 */
#define MII_BC_ADDR 		0x00		/* Basic Control Register */
#define MII_BC_RESET		0x8000		/* 1=software reset, 0=normal operation */
#define MII_BC_LOOP		0x4000		/* 1=loop back mode, 0=normal operation */
#define MII_BC_SPEED		0x2000		/* 1=100Mbps, 0=10Mbps (ignored if auto-nego is enabled) */
#define MII_BC_ANEG		0x1000		/* 1=enable auto-nego, 0=disable auto-nego */
#define MII_BC_PDOWN		0x0800		/* 1=power down mode, 0=normal operation */
#define MII_BC_ISOLATE		0x0400		/* 1=electrical isolation, 0=normal operation */
#define MII_BC_RE_ANEG		0x0200		/* 1=restart auto-nego, 0=normal operation */
#define MII_BC_DUPLEX		0x0100		/* 1=full duplex, 0=half duplex */
#define MII_BC_COLLISION	0x0080		/* 1=enable collision test, 0=disable collision test */
#define MII_BC_TRANSMITTER	0x0001		/* 1=disable transmitter, 0=enable transmitter */

#define MII_BS_ADDR		0x01		/* Basic Status Register */
#define MII_BS_T4		0x8000		/* 1=T4 capable, 0=not T4 capable */
#define MII_BS_100FD		0x4000		/* 1=100Mbps full duplex capable, 0=not 100Mbps full duplex capable */
#define MII_BS_100HD		0x2000		/* 1=100Mbps half duplex capable, 0=not 100Mbps half duplex capable */
#define MII_BS_10FD		0x1000		/* 1=10Mbps full duplex capable, 0=not 10Mbps full duplex capable */
#define MII_BS_10HD		0x0800		/* 1=10Mbps half duplex capable, 0=not 10Mbps half duplex capable */
#define MII_BS_PREAMBLE		0x0040		/* 1=preamble suppression, 0=normale preamble */
#define MII_BS_ANEG_COMP	0x0020		/* 1=auto-nego process completed, 0=auto-nego process not completed */
#define MII_BS_REMOTE		0x0010		/* 1=remote fault, 0=no remote fault */
#define MII_BS_ANEG_ABLE	0x0008		/* 1=capable to perform auto-nego, 0=not capable to perform auto-nego */
#define MII_BS_LINK_STAT	0x0004		/* 1=link is up, 0=link is down */
#define MII_BS_JABBER_DET	0x0002		/* 1=jabber detected, 0=jabber not detected */
#define MII_BS_EXT_CAPA		0x0001		/* 1=supports extended capabilities registers, 0=not supports */

#define MII_ICS_ADDR		0x1b		/* Interrupt Control/Status Register */
#define MII_ICS_JABBER_EN	0x8000		/* 1=enable jabber interrupt, 0=disable jabber interrupt */
#define MII_ICS_RX_ERR_EN	0x4000		/* 1=enable receive error interrupt, 0=disable receive error interrupt */
#define MII_ICS_PAGE_RX_EN	0x2000		/* 1=enable page receive interrupt, 0=disable page receive interrupt */
#define MII_ICS_PA_FAULT_EN	0x1000		/* 1=enable parallel detect fault interrupt, 0=disable parallel detect fault interrupt */
#define MII_ICS_LINK_ACK_EN	0x0800		/* 1=enable link partner ack interrupt, 0=disable link partner ack interrupt */
#define MII_ICS_LINK_DOWN_EN	0x0400		/* 1=enable link down interrupt, 0=disable link down interrupt */
#define MII_ICS_REMOTE_EN	0x0200		/* 1=enable remote fault interrupt, 0=disable remote fault interrupt */
#define MII_ICS_LINK_UP_EN	0x0100		/* 1=enable link up interrupt, 0=disable link up interrupt */
#define MII_ICS_JABBER		0x0080		/* 1=jabber occured, 0=jabber did not occured */
#define MII_ICS_RX_ERR		0x0040		/* 1=receive error occured, 0=receive error did not occured */
#define MII_ICS_PAGE_RX		0x0020		/* 1=page receive occured, 0=page receive did not occured */
#define MII_ICS_PA_FAULT	0x0010		/* 1=parallel detect fault occured, 0=parallel detect fault did not occured */
#define MII_ICS_LINK_ACK	0x0008		/* 1=link partner acknowledge occured, 0=link partner acknowledge did not occured */
#define MII_ICS_LINK_DOWN	0x0004		/* 1=link down occured, 0=link down did not occured */
#define MII_ICS_REMOTE		0x0002		/* 1=remote fault occured, 0=remote fault did not occured */
#define MII_ICS_LINK_UP		0x0001		/* 1=link up occured, 0=link up did not occured */

static int kszphy_config_init(struct phy_device *phydev)
{
	int rc = phy_read(phydev, MII_BC_ADDR);
	if (rc < 0) return rc;
	#ifdef DEBUG_MICREL
	printk("PHY%d control reg read 0x%04x\n", phydev->addr, rc);
	#endif
	
	rc = phy_read(phydev, MII_BS_ADDR);
	if (rc < 0) return rc;
	#ifdef DEBUG_MICREL
	printk("PHY%d status reg read 0x%04x\n", phydev->addr, rc);
	#endif
	
	return 0;
}

static int kszphy_phy_ack_interrupt(struct phy_device *phydev)
{
	int rc = phy_read(phydev, MII_ICS_ADDR);
	if (rc < 0) return rc;
	#ifdef DEBUG_MICREL
	printk("PHY%d ICS 0x%04x\n", phydev->addr, rc);
	rc = phy_read(phydev, MII_BC_ADDR);
	if (rc < 0) return rc;
	printk("PHY%d control reg read 0x%04x\n", phydev->addr, rc);
	rc = phy_read(phydev, MII_BS_ADDR);
	if (rc < 0) return rc;
	printk("PHY%d status reg read 0x%04x\n", phydev->addr, rc);
	#endif
	return 0;
}

static int kszphy_phy_config_intr(struct phy_device *phydev)
{
	int rc = phy_read(phydev, MII_ICS_ADDR);
	if (rc < 0) return rc;
	rc = phy_write(phydev, MII_ICS_ADDR, rc | MII_ICS_LINK_UP_EN | MII_ICS_LINK_DOWN_EN);
	return 0;
}


static struct phy_driver ks8001_driver = {
	.phy_id		= PHY_ID_KS8001,
	.name		= "Micrel KS8001",
	.phy_id_mask	= 0x00fffff0,
	.features	= PHY_BASIC_FEATURES,
	.flags		= PHY_POLL,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE,},
};

static struct phy_driver vsc8201_driver = {
	.phy_id		= PHY_ID_VSC8201,
	.name		= "Micrel VSC8201",
	.phy_id_mask	= 0x00fffff0,
	.features	= PHY_BASIC_FEATURES,
	.flags		= PHY_POLL,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE,},
};

static struct phy_driver ksz9021_driver = {
	.phy_id		= PHY_ID_KSZ9021,
	.phy_id_mask	= 0x000fff10,
	.name		= "Micrel KSZ9021 Gigabit PHY",
	.features	= PHY_GBIT_FEATURES | SUPPORTED_Pause,
	.flags		= PHY_POLL,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.driver		= { .owner = THIS_MODULE, },
};

static struct phy_driver ksz8041_driver = {
	.phy_id		= PHY_ID_KSZ8041,
	.phy_id_mask	= 0x00fffff0,
	.name		= "Micrel KSZ8041",
	.features	= PHY_BASIC_FEATURES,
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	
	.ack_interrupt	= kszphy_phy_ack_interrupt,
	.config_intr	= kszphy_phy_config_intr,
	
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.driver		= { .owner = THIS_MODULE, },
};

static int __init ksphy_init(void)
{
	int ret;

	ret = phy_driver_register(&ks8001_driver);
	if (ret)
		goto err1;

	ret = phy_driver_register(&vsc8201_driver);
	if (ret)
		goto err2;

	ret = phy_driver_register(&ksz9021_driver);
	if (ret)
		goto err3;

	ret = phy_driver_register(&ksz8041_driver);
	if (ret)
		goto err4;

	return 0;

err4:
	phy_driver_unregister(&ksz9021_driver);
err3:
	phy_driver_unregister(&vsc8201_driver);
err2:
	phy_driver_unregister(&ks8001_driver);
err1:
	return ret;
}

static void __exit ksphy_exit(void)
{
	phy_driver_unregister(&ks8001_driver);
	phy_driver_unregister(&vsc8201_driver);
	phy_driver_unregister(&ksz9021_driver);
	phy_driver_unregister(&ksz8041_driver);
}

module_init(ksphy_init);
module_exit(ksphy_exit);

MODULE_DESCRIPTION("Micrel PHY driver");
MODULE_AUTHOR("David J. Choi");
MODULE_LICENSE("GPL");

static struct mdio_device_id micrel_tbl[] = {
	{ PHY_ID_KSZ9021, 0x000fff10 },
	{ PHY_ID_VSC8201, 0x00fffff0 },
	{ PHY_ID_KS8001, 0x00fffff0 },
	{ PHY_ID_KSZ8041, 0x00fffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, micrel_tbl);
