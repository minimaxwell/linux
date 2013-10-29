/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <linux/cache.h>
#include <asm/byteorder.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/irq.h>

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/hdlc.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/etherdevice.h>
#include "pef2256.h"

static irqreturn_t pef2256_irq(int irq, void *dev_priv);
static int Config_HDLC(struct pef2256_dev_priv *priv);
static int init_FALC(struct pef2256_dev_priv *priv);
static int pef2256_open(struct net_device *netdev);
static int pef2256_close(struct net_device *netdev);

void print_regs(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	unsigned char *base_addr = priv->base_addr;

	netdev_info(ndev, "	MODE = 0x%02x\n", readb(base_addr + MODE));
	netdev_info(ndev, "	RAH1 = 0x%02x\n", readb(base_addr + RAH1));
	netdev_info(ndev, "	RAH2 = 0x%02x\n", readb(base_addr + RAH2));
	netdev_info(ndev, "	RAL1 = 0x%02x\n", readb(base_addr + RAL1));
	netdev_info(ndev, "	RAL2 = 0x%02x\n", readb(base_addr + RAL2));
	netdev_info(ndev, "	IPC = 0x%02x\n", readb(base_addr + IPC));
	netdev_info(ndev, "	CCR1 = 0x%02x\n", readb(base_addr + CCR1));
	netdev_info(ndev, "	CCR2 = 0x%02x\n", readb(base_addr + CCR2));
	netdev_info(ndev, "	RTR1 = 0x%02x\n", readb(base_addr + RTR1));
	netdev_info(ndev, "	RTR2 = 0x%02x\n", readb(base_addr + RTR2));
	netdev_info(ndev, "	RTR3 = 0x%02x\n", readb(base_addr + RTR3));
	netdev_info(ndev, "	RTR4 = 0x%02x\n", readb(base_addr + RTR4));
	netdev_info(ndev, "	TTR1 = 0x%02x\n", readb(base_addr + TTR1));
	netdev_info(ndev, "	TTR2 = 0x%02x\n", readb(base_addr + TTR2));
	netdev_info(ndev, "	TTR3 = 0x%02x\n", readb(base_addr + TTR3));
	netdev_info(ndev, "	TTR4 = 0x%02x\n", readb(base_addr + TTR4));
	netdev_info(ndev, "	IMR0 = 0x%02x\n", readb(base_addr + IMR0));
	netdev_info(ndev, "	IMR1 = 0x%02x\n", readb(base_addr + IMR1));
	netdev_info(ndev, "	IMR2 = 0x%02x\n", readb(base_addr + IMR2));
	netdev_info(ndev, "	IMR3 = 0x%02x\n", readb(base_addr + IMR3));
	netdev_info(ndev, "	IMR4 = 0x%02x\n", readb(base_addr + IMR4));
	netdev_info(ndev, "	IMR5 = 0x%02x\n", readb(base_addr + IMR5));
	netdev_info(ndev, "	IERR = 0x%02x\n", readb(base_addr + IERR));
	netdev_info(ndev, "	FMR0 = 0x%02x\n", readb(base_addr + FMR0));
	netdev_info(ndev, "	FMR1 = 0x%02x\n", readb(base_addr + FMR1));
	netdev_info(ndev, "	FMR2 = 0x%02x\n", readb(base_addr + FMR2));
	netdev_info(ndev, "	LOOP = 0x%02x\n", readb(base_addr + LOOP));
	netdev_info(ndev, "	XSW = 0x%02x\n", readb(base_addr + XSW));
	netdev_info(ndev, "	XSP = 0x%02x\n", readb(base_addr + XSP));
	netdev_info(ndev, "	XC0 = 0x%02x\n", readb(base_addr + XC0));
	netdev_info(ndev, "	XC1 = 0x%02x\n", readb(base_addr + XC1));
	netdev_info(ndev, "	RC0 = 0x%02x\n", readb(base_addr + RC0));
	netdev_info(ndev, "	RC1 = 0x%02x\n", readb(base_addr + RC1));
	netdev_info(ndev, "	XPM0 = 0x%02x\n", readb(base_addr + XPM0));
	netdev_info(ndev, "	XPM1 = 0x%02x\n", readb(base_addr + XPM1));
	netdev_info(ndev, "	XPM2 = 0x%02x\n", readb(base_addr + XPM2));
	netdev_info(ndev, "	TSWM = 0x%02x\n", readb(base_addr + TSWM));
	netdev_info(ndev, "	IDLE = 0x%02x\n", readb(base_addr + IDLE));
	netdev_info(ndev, "	XSA4 = 0x%02x\n", readb(base_addr + XSA4));
	netdev_info(ndev, "	XSA5 = 0x%02x\n", readb(base_addr + XSA5));
	netdev_info(ndev, "	XSA6 = 0x%02x\n", readb(base_addr + XSA6));
	netdev_info(ndev, "	XSA7 = 0x%02x\n", readb(base_addr + XSA7));
	netdev_info(ndev, "	XSA8 = 0x%02x\n", readb(base_addr + XSA8));
	netdev_info(ndev, "	FMR3 = 0x%02x\n", readb(base_addr + FMR3));
	netdev_info(ndev, "	ICB1 = 0x%02x\n", readb(base_addr + ICB1));
	netdev_info(ndev, "	ICB2 = 0x%02x\n", readb(base_addr + ICB2));
	netdev_info(ndev, "	ICB3 = 0x%02x\n", readb(base_addr + ICB3));
	netdev_info(ndev, "	ICB4 = 0x%02x\n", readb(base_addr + ICB4));
	netdev_info(ndev, "	LIM0 = 0x%02x\n", readb(base_addr + LIM0));
	netdev_info(ndev, "	LIM1 = 0x%02x\n", readb(base_addr + LIM1));
	netdev_info(ndev, "	PCD = 0x%02x\n", readb(base_addr + PCD));
	netdev_info(ndev, "	PCR = 0x%02x\n", readb(base_addr + PCR));
	netdev_info(ndev, "	LIM2 = 0x%02x\n", readb(base_addr + LIM2));
	netdev_info(ndev, "	LCR1 = 0x%02x\n", readb(base_addr + LCR1));
	netdev_info(ndev, "	LCR2 = 0x%02x\n", readb(base_addr + LCR2));
	netdev_info(ndev, "	LCR3 = 0x%02x\n", readb(base_addr + LCR3));
	netdev_info(ndev, "	SIC1 = 0x%02x\n", readb(base_addr + SIC1));
	netdev_info(ndev, "	SIC2 = 0x%02x\n", readb(base_addr + SIC2));
	netdev_info(ndev, "	SIC3 = 0x%02x\n", readb(base_addr + SIC3));
	netdev_info(ndev, "	CMR1 = 0x%02x\n", readb(base_addr + CMR1));
	netdev_info(ndev, "	CMR2 = 0x%02x\n", readb(base_addr + CMR2));
	netdev_info(ndev, "	GCR = 0x%02x\n", readb(base_addr + GCR));
	netdev_info(ndev, "	ESM = 0x%02x\n", readb(base_addr + ESM));
	netdev_info(ndev, "	CMR3 = 0x%02x\n", readb(base_addr + CMR3));
	netdev_info(ndev, "	PC1 = 0x%02x\n", readb(base_addr + PC1));
	netdev_info(ndev, "	PC2 = 0x%02x\n", readb(base_addr + PC2));
	netdev_info(ndev, "	PC3 = 0x%02x\n", readb(base_addr + PC3));
	netdev_info(ndev, "	PC4 = 0x%02x\n", readb(base_addr + PC4));
	netdev_info(ndev, "	PC5 = 0x%02x\n", readb(base_addr + PC5));
	netdev_info(ndev, "	GPC1 = 0x%02x\n", readb(base_addr + GPC1));
	netdev_info(ndev, "	PC6 = 0x%02x\n", readb(base_addr + PC6));
	netdev_info(ndev, "	CCR3 = 0x%02x\n", readb(base_addr + CCR3));
	netdev_info(ndev, "	CCR4 = 0x%02x\n", readb(base_addr + CCR4));
	netdev_info(ndev, "	CCR5 = 0x%02x\n", readb(base_addr + CCR5));
	netdev_info(ndev, "	MODE2 = 0x%02x\n", readb(base_addr + MODE2));
	netdev_info(ndev, "	MODE3 = 0x%02x\n", readb(base_addr + MODE3));
	netdev_info(ndev, "	RBC2 = 0x%02x\n", readb(base_addr + RBC2));
	netdev_info(ndev, "	RBC3 = 0x%02x\n", readb(base_addr + RBC3));
	netdev_info(ndev, "	GCM1 = 0x%02x\n", readb(base_addr + GCM1));
	netdev_info(ndev, "	GCM2 = 0x%02x\n", readb(base_addr + GCM2));
	netdev_info(ndev, "	GCM3 = 0x%02x\n", readb(base_addr + GCM3));
	netdev_info(ndev, "	GCM4 = 0x%02x\n", readb(base_addr + GCM4));
	netdev_info(ndev, "	GCM5 = 0x%02x\n", readb(base_addr + GCM5));
	netdev_info(ndev, "	GCM6 = 0x%02x\n", readb(base_addr + GCM6));
	netdev_info(ndev, "	SIS2/GCM7 = 0x%02x\n", readb(base_addr + SIS2_1));
	netdev_info(ndev, "	RSIS2/GCM8 = 0x%02x\n", readb(base_addr + RSIS2_1));
	netdev_info(ndev, "	TSEO = 0x%02x\n", readb(base_addr + TSEO));
	netdev_info(ndev, "	TSBS1 = 0x%02x\n", readb(base_addr + TSBS1));
	netdev_info(ndev, "	TSBS2 = 0x%02x\n", readb(base_addr + TSBS2));
	netdev_info(ndev, "	TSBS3 = 0x%02x\n", readb(base_addr + TSBS3));
	netdev_info(ndev, "	TSS2 = 0x%02x\n", readb(base_addr + TSS2));
	netdev_info(ndev, "	TSS3 = 0x%02x\n", readb(base_addr + TSS3));
	netdev_info(ndev, "	Res10 = 0x%02x\n", readb(base_addr + Res10));
	netdev_info(ndev, "	Res11 = 0x%02x\n", readb(base_addr + Res11));
	netdev_info(ndev, "	TPC0 = 0x%02x\n", readb(base_addr + TPC0));
	netdev_info(ndev, "	GLC1 = 0x%02x\n", readb(base_addr + GLC1));
}

static ssize_t fs_attr_regs_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	print_regs(dev);
	return sprintf(buf, "*** printk DEBUG ***\n");
}

static DEVICE_ATTR(regs, S_IRUGO, fs_attr_regs_show, NULL);

static ssize_t fs_attr_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "%d\n", priv->mode);
}


static ssize_t fs_attr_mode_store(struct device *dev,
			struct device_attribute *attr,  const char *buf,
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	long int value;
	int ret = kstrtol(buf, 10, &value);
	int reconfigure = (value != priv->mode);

	if (value != MASTER_MODE && value != SLAVE_MODE)
		ret = -EINVAL;

	if (ret < 0)
		netdev_info(ndev, "Invalid mode (0 or 1 expected\n");
	else {
		priv->mode = value;
		if (reconfigure && priv->init_done) {
			pef2256_close(ndev);
			init_FALC(priv);
			pef2256_open(ndev);
		}
	}

	return strnlen(buf, count);
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, fs_attr_mode_show,
						fs_attr_mode_store);



static ssize_t fs_attr_Tx_TS_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "0x%08x\n", priv->Tx_TS);
}


static ssize_t fs_attr_Tx_TS_store(struct device *dev,
			struct device_attribute *attr,  const char *buf,
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	unsigned long value;
	int ret = kstrtoul(buf, 16, (long int *)&value);
	int reconfigure = (value != priv->mode);

	/* TS 0 is reserved */
	if (ret < 0 || value > TS_0)
		ret = -EINVAL;

	if (ret < 0)
		netdev_info(ndev, "Invalid Tx_TS (hex number > 0 and < 0x80000000 expected\n");
	else {
		priv->Tx_TS = value;
		if (reconfigure && priv->init_done)
			Config_HDLC(priv);
	}

	return strnlen(buf, count);
}

static DEVICE_ATTR(Tx_TS, S_IRUGO | S_IWUSR, fs_attr_Tx_TS_show,
			fs_attr_Tx_TS_store);


static ssize_t fs_attr_Rx_TS_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "0x%08x\n", priv->Rx_TS);
}


static ssize_t fs_attr_Rx_TS_store(struct device *dev,
			struct device_attribute *attr,  const char *buf,
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	unsigned long value;
	int ret = kstrtoul(buf, 16, &value);
	int reconfigure = (value != priv->mode);

	/* TS 0 is reserved */
	if (ret < 0 || value > TS_0)
		ret = -EINVAL;

	if (ret < 0)
		netdev_info(ndev, "Invalid Rx_TS (hex number > 0 and < 0x80000000 expected\n");
	else {
		priv->Rx_TS = value;
		if (reconfigure && priv->init_done)
			Config_HDLC(priv);
	}

	return strnlen(buf, count);
}

static DEVICE_ATTR(Rx_TS, S_IRUGO | S_IWUSR, fs_attr_Rx_TS_show,
	 fs_attr_Rx_TS_store);

/*
 * Setting up HDLC channel
 */
int Config_HDLC(struct pef2256_dev_priv *priv)
{
	int i;
	int TS_idx;
	unsigned char *base_addr;
	u8 dummy;

	/* Set framer E1 address */
	base_addr = priv->base_addr;

	/* Read to remove pending IT */
	dummy = readb(base_addr + ISR0);
	dummy = readb(base_addr + ISR1);

	/* Mask HDLC 1 Transmit IT */
	writeb(readb(base_addr + IMR1) | 1, base_addr + IMR1);
	writeb(readb(base_addr + IMR1) | (1 << 4), base_addr + IMR1);
	writeb(readb(base_addr + IMR1) | (1 << 5), base_addr + IMR1);

	/* Mask HDLC 1 Receive IT */
	writeb(readb(base_addr + IMR0) | 1, base_addr + IMR0);
	writeb(readb(base_addr + IMR0) | (1 << 7), base_addr + IMR0);
	writeb(readb(base_addr + IMR1) | (1 << 6), base_addr + IMR1);

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	writeb(1 << 3, base_addr + MODE);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO is 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	writeb(0x10 | (1 << 3), base_addr + CCR1);
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	writeb(0x00, base_addr + CCR2);

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	writeb(1 << 3, base_addr + MODE);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO is 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	writeb(0x10 | (1 << 3), base_addr + CCR1);
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	writeb(0x00, base_addr + CCR2);

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	writeb(1 << 3, base_addr + MODE);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO is 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	writeb(0x10 | (1 << 3), base_addr + CCR1);
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	writeb(0x00, base_addr + CCR2);

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	/* Init  Time Slot select */
	writeb(0x00, base_addr + TTR1);
	writeb(0x00, base_addr + TTR2);
	writeb(0x00, base_addr + TTR3);
	writeb(0x00, base_addr + TTR4);
	writeb(0x00, base_addr + RTR1);
	writeb(0x00, base_addr + RTR2);
	writeb(0x00, base_addr + RTR3);
	writeb(0x00, base_addr + RTR4);
	/* Set selected TS bits */
	/* Starting at TS 1, TS 0 is reserved */
	for (TS_idx = 1; TS_idx < 32; TS_idx++) {
		i = 7 - (TS_idx % 8);
		switch (TS_idx / 8) {
		case 0:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + TTR1) | (1 << i),
					base_addr + TTR1);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + RTR1) | (1 << i),
					base_addr + RTR1);
			break;
		case 1:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + TTR2) | (1 << i),
					base_addr + TTR2);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + RTR2) | (1 << i),
					base_addr + RTR2);
			break;
		case 2:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + TTR3) | (1 << i),
					base_addr + TTR3);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + RTR3) | (1 << i),
					base_addr + RTR3);
			break;
		case 3:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + TTR4) | (1 << i),
					base_addr + TTR4);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				writeb(readb(base_addr + RTR4) | (1 << i),
					base_addr + RTR4);
			break;
		}
	}

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	/* Unmask HDLC 1 Transmit IT */
	writeb(readb(base_addr + IMR1) & ~1, base_addr + IMR1);
	writeb(readb(base_addr + IMR1) & ~(1 << 4), base_addr + IMR1);
	writeb(readb(base_addr + IMR1) & ~(1 << 5), base_addr + IMR1);

	/* Unmask HDLC 1 Receive IT */
	writeb(readb(base_addr + IMR0) & ~1, base_addr + IMR0);
	writeb(readb(base_addr + IMR0) & ~(1 << 7), base_addr + IMR0);
	writeb(readb(base_addr + IMR1) & ~(1 << 6), base_addr + IMR1);

	/*
	 * The hardware requires a delay up to 2*32*125 usec to take commands
         *  into account
	 */
	udelay((2 * 32) * 125);

	return 0;
}


/*
 * Init FALC56
 */
static int init_FALC(struct pef2256_dev_priv *priv)
{
	unsigned char *base_addr;
	int Version;

	/* Get controller version */
	Version = priv->component_id;

	/* Init FALC56 */
	base_addr = priv->base_addr;
	/* RCLK output : DPLL clock, DCO-X enabled, DCO-X internal reference
	   clock */
	writeb(0x00, base_addr + CMR1);
	/* SCLKR selected, SCLKX selected, receive synchro pulse sourced by
	   SYPR, transmit synchro pulse sourced by SYPX */
	writeb(0x00, base_addr + CMR2);
	/* NRZ coding, no alarm simulation */
	writeb(0x00, base_addr + FMR0);
	/* E1 double frame format, 2 Mbit/s system data rate, no AIS
	   transmission to remote end or system interface, payload loop
	   off, transmit remote alarm on */
	writeb(0x00, base_addr + FMR1);
	writeb(0x02, base_addr + FMR2);
	/* E1 default for LIM2 */
	writeb(0x20, base_addr + LIM2);
	if (priv->mode == MASTER_MODE)
		/* SEC input, active high */
		writeb(0x00, base_addr + GPC1);
	else
		/* FSC output, active high */
		writeb(0x40, base_addr + GPC1);
	/* internal second timer, power on */
	writeb(0x00, base_addr + GCR);
	/* slave mode, local loop off, mode short-haul */
	if (Version == VERSION_1_2)
		writeb(0x00, base_addr + LIM0);
	else
		writeb(0x08, base_addr + LIM0);
	/* analog interface selected, remote loop off */
	writeb(0x00, base_addr + LIM1);
	if (Version == VERSION_1_2) {
		/* function of ports RP(A to D) : output receive sync pulse
		   function of ports XP(A to D) : output transmit line clock */
		writeb(0x77, base_addr + PC1);
		writeb(0x77, base_addr + PC2);
		writeb(0x77, base_addr + PC3);
		writeb(0x77, base_addr + PC4);
	} else {
		/* function of ports RP(A to D) : output high
		   function of ports XP(A to D) : output high */
		writeb(0xAA, base_addr + PC1);
		writeb(0xAA, base_addr + PC2);
		writeb(0xAA, base_addr + PC3);
		writeb(0xAA, base_addr + PC4);
	}
	/* function of port RPA : input SYPR
	   function of port XPA : input SYPX */
	writeb(0x00, base_addr + PC1);
	/* SCLKR, SCLKX, RCLK configured to inputs,
	   XFMS active low, CLK1 and CLK2 pin configuration */
	writeb(0x00, base_addr + PC5);
	writeb(0x00, base_addr + PC6);
	/* the receive clock offset is cleared
	   the receive time slot offset is cleared */
	writeb(0x00, base_addr + RC0);
	writeb(0x9C, base_addr + RC1);
	/* 2.048 MHz system clocking rate, receive buffer 2 frames, transmit
	   buffer bypass, data sampled and transmitted on the falling edge of
	   SCLKR/X, automatic freeze signaling, data is active in the first
	   channel phase */
	writeb(0x00, base_addr + SIC1);
	writeb(0x00, base_addr + SIC2);
	writeb(0x00, base_addr + SIC3);
	/* channel loop-back and single frame mode are disabled */
	writeb(0x00, base_addr + LOOP);
	/* all bits of the transmitted service word are cleared */
	writeb(0x1F, base_addr + XSW);
	/* spare bit values are cleared */
	writeb(0x00, base_addr + XSP);
	/* no transparent mode active */
	writeb(0x00, base_addr + TSWM);
	/* the transmit clock offset is cleared
	   the transmit time slot offset is cleared */
	writeb(0x00, base_addr + XC0);
	writeb(0x9C, base_addr + XC1);
	/* transmitter in tristate mode */
	writeb(0x40, base_addr + XPM2);
	/* transmit pulse mask */
	if (Version != VERSION_1_2)
		writeb(0x9C, base_addr + XPM0);

	if (Version == VERSION_1_2) {
		/* master clock is 16,384 MHz (flexible master clock) */
		writeb(0x58, base_addr + GCM2);
		writeb(0xD2, base_addr + GCM3);
		writeb(0xC2, base_addr + GCM4);
		writeb(0x07, base_addr + GCM5);
		writeb(0x10, base_addr + GCM6);
	} else {
		/* master clock is 16,384 MHz (flexible master clock) */
		writeb(0x18, base_addr + GCM2);
		writeb(0xFB, base_addr + GCM3);
		writeb(0x0B, base_addr + GCM4);
		writeb(0x01, base_addr + GCM5);
		writeb(0x0B, base_addr + GCM6);
		writeb(0xDB, base_addr + GCM7);
		writeb(0xDF, base_addr + GCM8);
	}

	/* master mode => LIM0.MAS = 1 (bit 0) */
	if (priv->mode == MASTER_MODE)
		writeb(readb(base_addr + LIM0) | (1 << 0), base_addr + LIM0);

	/* transmit line in normal operation => XPM2.XLT = 0 (bit 6) */
	writeb(readb(base_addr + XPM2) & ~(1 << 6), base_addr + XPM2);

	if (Version == VERSION_1_2) {
		/* receive input threshold = 0,21V =>
			LIM1.RIL2:0 = 101 (bits 6, 5 et 4) */
		writeb(readb(base_addr + LIM1) | (1 << 4), base_addr + LIM1);
		writeb(readb(base_addr + LIM1) | (1 << 6), base_addr + LIM1);
	} else {
		/* receive input threshold = 0,21V =>
			LIM1.RIL2:0 = 100 (bits 6, 5 et 4) */
		writeb(readb(base_addr + LIM1) | (1 << 6), base_addr + LIM1);
	}
	/* transmit line coding = HDB3 => FMR0.XC1:0 = 11 (bits 7 et 6) */
	writeb(readb(base_addr + FMR0) | (1 << 6), base_addr + FMR0);
	writeb(readb(base_addr + FMR0) | (1 << 7), base_addr + FMR0);
	/* receive line coding = HDB3 => FMR0.RC1:0 = 11 (bits 5 et 4) */
	writeb(readb(base_addr + FMR0) | (1 << 4), base_addr + FMR0);
	writeb(readb(base_addr + FMR0) | (1 << 5), base_addr + FMR0);
	/* detection of LOS alarm = 176 pulses (soit (10 + 1) * 16) */
	writeb(10, base_addr + PCD);
	/* recovery of LOS alarm = 22 pulses (soit 21 + 1) */
	writeb(21, base_addr + PCR);
	/* DCO-X center frequency => CMR2.DCOXC = 1 (bit 5) */
	writeb(readb(base_addr + CMR2) | (1 << 5), base_addr + CMR2);
	if (priv->mode == SLAVE_MODE) {
		/* select RCLK source = 2M => CMR1.RS(1:0) = 10 (bits 5 et 4) */
		writeb(readb(base_addr + CMR1) | (1 << 5), base_addr + CMR1);
		/* disable switching RCLK -> SYNC => CMR1.DCS = 1 (bit 3) */
		writeb(readb(base_addr + CMR1) | (1 << 3), base_addr + CMR1);
	}
	if (Version != VERSION_1_2)
		/* during inactive channel phase RDO into tri-state mode */
		writeb(readb(base_addr + SIC3) | (1 << 5), base_addr + SIC3);
	if (!strcmp(priv->rising_edge_sync_pulse, "transmit")) {
		/* rising edge sync pulse transmit => SIC3.RESX = 1 (bit 3) */
		writeb(readb(base_addr + SIC3) | (1 << 3), base_addr + SIC3);
	} else {
		/* rising edge sync pulse receive => SIC3.RESR = 1 (bit 2) */
		writeb(readb(base_addr + SIC3) | (1 << 2), base_addr + SIC3);
	}
	/* transmit offset counter = 4
	   => XC0.XCO10:8 = 000 (bits 2, 1 et 0);
	      XC1.XCO7:0 = 4 (bits 7 ... 0) */
	writeb(4, base_addr + XC1);
	/* receive offset counter = 4
	   => RC0.RCO10:8 = 000 (bits 2, 1 et 0);
	      RC1.RCO7:0 = 4 (bits 7 ... 0) */
	writeb(4, base_addr + RC1);

	/* Nothing to do if clock rate = 8 Mhz or data rate = 2 Mb/s */

	/* clocking rate 4M  */ 
	if (priv->clock_rate == CLOCK_RATE_4M)
		writeb(readb(base_addr + SIC1) | (1 << 3), base_addr + SIC1);
	/* clocking rate 8M  */ 
	if (priv->clock_rate == CLOCK_RATE_8M)
		writeb(readb(base_addr + SIC1) | (1 << 7), base_addr + SIC1);
	/* clocking rate 16M  */ 
	if (priv->clock_rate == CLOCK_RATE_16M) {
		writeb(readb(base_addr + SIC1) | (1 << 3), base_addr + SIC1);
		writeb(readb(base_addr + SIC1) | (1 << 7), base_addr + SIC1);
	}

	/* data rate 4M on the system data bus */
	if (priv->data_rate == DATA_RATE_4M)
		writeb(readb(base_addr + FMR1) | (1 << 1), base_addr + FMR1);
	/* data rate 8M on the system data bus */
	if (priv->data_rate == DATA_RATE_8M)
		writeb(readb(base_addr + SIC1) | (1 << 6), base_addr + SIC1);
	/* data rate 16M on the system data bus */
	if (priv->data_rate == DATA_RATE_16M) {
		writeb(readb(base_addr + FMR1) | (1 << 1), base_addr + FMR1);
		writeb(readb(base_addr + SIC1) | (1 << 6), base_addr + SIC1);
	}

	/* channel phase for FALC56 */
	if ((priv->channel_phase == CHANNEL_PHASE_1)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		writeb(readb(base_addr + SIC2) | (1 << 1), base_addr + SIC2);
	if ((priv->channel_phase == CHANNEL_PHASE_2)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		writeb(readb(base_addr + SIC2) | (1 << 2), base_addr + SIC2);

	if (priv->mode == SLAVE_MODE) {
		/* transmit buffer size = 2 frames =>
			SIC1.XBS1:0 = 10 (bits 1 et 0) */
		writeb(readb(base_addr + SIC1) | (1 << 1), base_addr + SIC1);
	}

	/* transmit in multiframe => FMR1.XFS = 1 (bit 3) */
	writeb(readb(base_addr + FMR1) | (1 << 3), base_addr + FMR1);
	/* receive in multiframe => FMR2.RFS1:0 = 10 (bits 7 et 6) */
	writeb(readb(base_addr + FMR2) | (1 << 7), base_addr + FMR2);
	/* Automatic transmission of submultiframe status =>
		XSP.AXS = 1 (bit 3) */
	writeb(readb(base_addr + XSP) | (1 << 3), base_addr + XSP);

	/* error counter mode toutes les 1s => FMR1.ECM = 1 (bit 2) */
	writeb(readb(base_addr + FMR1) | (1 << 2), base_addr + FMR1);
	/* error counter mode COFA => GCR.ECMC = 1 (bit 4) */
	writeb(readb(base_addr + GCR) | (1 << 4), base_addr + GCR);
	/* errors in service words with no influence => RC0.SWD = 1 (bit 7) */
	writeb(readb(base_addr + RC0) | (1 << 7), base_addr + RC0);
	/* 4 consecutive incorrect FAS = loss of sync => RC0.ASY4 = 1 (bit 6) */
	writeb(readb(base_addr + RC0) | (1 << 6), base_addr + RC0);
	/* Si-Bit in service word from XDI => XSW.XSIS = 1 (bit 7) */
	writeb(readb(base_addr + XSW) | (1 << 7), base_addr + XSW);
	/* Si-Bit in FAS word from XDI => XSP.XSIF = 1 (bit 2) */
	writeb(readb(base_addr + XSP) | (1 << 2), base_addr + XSP);

	/* port RCLK is output => PC5.CRP = 1 (bit 0) */
	writeb(readb(base_addr + PC5) | (1 << 0), base_addr + PC5);
	/* visibility of the masked interrupts => GCR.VIS = 1 (bit 7) */
	writeb(readb(base_addr + GCR) | (1 << 7), base_addr + GCR);
	/* reset lines
	   => CMDR.RRES = 1 (bit 6); CMDR.XRES = 1 (bit 4);
	      CMDR.SRES = 1 (bit 0) */
	writeb(0x51, base_addr + CMDR);

	return 0;
}



static int pef2256_open(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	unsigned char *base_addr = priv->base_addr;
	int ret;

	if (hdlc_open(netdev))
		return -EAGAIN;

	ret = request_irq(priv->irq, pef2256_irq, 0, "e1-wan", priv);
	if (ret) {
		dev_err(priv->dev, "Cannot request irq. Device seems busy.\n");
		return -EBUSY;
	}

	if (priv->component_id != VERSION_UNDEF) {
		ret = init_FALC(priv);
	} else {
		dev_err(priv->dev, "Composant ident (%X/%X) = %d\n",
			readb(base_addr + VSTR), readb(base_addr + WID),
				priv->component_id);
		ret = -ENODEV;
	}

	if (ret < 0)
		return ret;

	priv->tx_skb = NULL;
	priv->rx_len = 0;

	Config_HDLC(priv);

	netif_carrier_on(netdev);
	netif_start_queue(netdev);

	priv->init_done = 1;

	return 0;
}


static int pef2256_close(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	if (!priv->init_done)
		return 0;

	priv->init_done = 0;
	netif_stop_queue(netdev);
	hdlc_close(netdev);
	free_irq(priv->irq, priv);

	return 0;
}



static int pef2256_rx(struct pef2256_dev_priv *priv)
{
	struct sk_buff *skb;
	int idx, size;
	unsigned char *base_addr;

	base_addr = priv->base_addr;

printk("R");

	/* RDO has been received -> wait for RME */
	if (priv->rx_len == -1) {
		/* Acknowledge the FIFO */
		writeb(readb(base_addr + CMDR) | (1 << 7), base_addr + CMDR);

		if (priv->R_ISR0 & (1 << 7))
			priv->rx_len = 0;

		return 0;
	}

	/* RPF : a block is available in the receive FIFO */
	if (priv->R_ISR0 & 1) {
		for (idx = 0; idx < 32; idx++)
			priv->rx_buff[priv->rx_len + idx] =
				readb(base_addr + RFIFO + (idx & 1));

		/* Acknowledge the FIFO */
		writeb(readb(base_addr + CMDR) | (1 << 7), base_addr + CMDR);

		priv->rx_len += 32;
	}

	/* RME : Message end : Read the receive FIFO */
	if (priv->R_ISR0 & (1 << 7)) {
		/* Get size of last block */
		size = readb(base_addr + RBCL) & 0x1F;

		/* Read last block */
		for (idx = 0; idx < size; idx++)
			priv->rx_buff[priv->rx_len + idx] =
				readb(base_addr + RFIFO + (idx & 1));

		/* Acknowledge the FIFO */
		writeb(readb(base_addr + CMDR) | (1 << 7), base_addr + CMDR);

		priv->rx_len += size;

		/* Packet received */
		if (priv->rx_len > 0) {
			skb = dev_alloc_skb(priv->rx_len);
			if (!skb) {
				priv->rx_len = 0;
				priv->netdev->stats.rx_dropped++;
				return -ENOMEM;
			}
			memcpy(skb->data, priv->rx_buff, priv->rx_len);
			skb_put(skb, priv->rx_len);
			priv->rx_len = 0;
			skb->protocol = hdlc_type_trans(skb, priv->netdev);
			priv->netdev->stats.rx_packets++;
			priv->netdev->stats.rx_bytes += skb->len;
			netif_rx(skb);
		}
	}

	return 0;
}


static int pef2256_tx(struct pef2256_dev_priv *priv)
{
	int idx, size;
	unsigned char *base_addr;
	u8 *tx_buff = priv->tx_skb->data;

	base_addr = priv->base_addr;

	/* ALLS : transmit all done */
	if (priv->R_ISR1 & (1 << 5)) {
		priv->netdev->stats.tx_packets++;
		priv->netdev->stats.tx_bytes += priv->tx_skb->len;
		/* dev_kfree_skb(priv->tx_skb); */
		priv->tx_skb = NULL;
		priv->tx_len = 0;
		netif_wake_queue(priv->netdev);
	}
	/* XPR : write a new block in transmit FIFO */
	else if (priv->tx_len < priv->tx_skb->len) {
		size = priv->tx_skb->len - priv->tx_len;
		if (size > 32)
			size = 32;

		for (idx = 0; idx < size; idx++)
			writeb(tx_buff[priv->tx_len + idx],
				base_addr + XFIFO + (idx & 1));

		priv->tx_len += size;

		if (priv->tx_len == priv->tx_skb->len)
			writeb(readb(base_addr + CMDR) | ((1 << 3) | (1 << 1)),
				base_addr + CMDR);
		else
			writeb(readb(base_addr + CMDR) | (1 << 3),
				base_addr + CMDR);
	}

	return 0;
}


irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	struct pef2256_dev_priv *priv = (struct pef2256_dev_priv *)dev_priv;
	unsigned char *base_addr;
	u8 R_GIS;

	base_addr = priv->base_addr;
	R_GIS = readb(base_addr + GIS);

	priv->R_ISR0 = priv->R_ISR1 = 0;

	/* We only care about ISR0 and ISR1 */
	/* ISR0 */
	if (R_GIS & 1)
		priv->R_ISR0 =
			readb(base_addr + ISR0) & ~(readb(base_addr + IMR0));

	/* ISR1 */
	if (R_GIS & (1 << 1))
		priv->R_ISR1 =
			readb(base_addr + ISR1) & ~(readb(base_addr + IMR1));

	/* Don't do anything else before init is done */
	if (!priv->init_done)
		return IRQ_HANDLED;

	/* RDO : Receive data overflow -> RX error */
	if (priv->R_ISR1 & (1 << 6)) {
		/* Acknowledge the FIFO */
		writeb(readb(base_addr + CMDR) | (1 << 7), base_addr + CMDR);
		priv->netdev->stats.rx_errors++;
		/* RME received ? */
		if (priv->R_ISR0 & (1 << 7))
			priv->rx_len = 0;
		else
			priv->rx_len = -1;
		return IRQ_HANDLED;
	}

	/* XDU : Transmit data underrun -> TX error */
	if (priv->R_ISR1 & (1 << 4)) {
		priv->netdev->stats.tx_errors++;
		/* dev_kfree_skb(priv->tx_skb); */
		priv->tx_skb = NULL;
		netif_wake_queue(priv->netdev);
		return IRQ_HANDLED;
	}

	/* RPF or RME : FIFO received */
	if (priv->R_ISR0 & (1 | (1 << 7)))
		pef2256_rx(priv);

	/* XPR or ALLS : FIFO sent */
	if (priv->R_ISR1 & (1 | (1 << 5)))
		pef2256_tx(priv);

	return IRQ_HANDLED;
}


static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	int idx, size;
	unsigned char *base_addr;
	u8 *tx_buff = skb->data;

	base_addr = priv->base_addr;

	priv->tx_skb = skb;
	priv->tx_len = 0;

	size = priv->tx_skb->len - priv->tx_len;
	if (size > 32)
		size = 32;

	for (idx = 0; idx < size; idx++)
		writeb(tx_buff[priv->tx_len + idx],
			base_addr + XFIFO + (idx & 1));

	priv->tx_len += size;

	writeb(readb(base_addr + CMDR) | (1 << 3), base_addr + CMDR);
	if (priv->tx_len == priv->tx_skb->len)
		writeb(readb(base_addr + CMDR) | (1 << 1), base_addr + CMDR);

	netif_stop_queue(netdev);
	return NETDEV_TX_OK;
}

static const struct net_device_ops pef2256_ops = {
	.ndo_open       = pef2256_open,
	.ndo_stop       = pef2256_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = hdlc_ioctl,
};


static int pef2256_hdlc_attach(struct net_device *netdev,
				unsigned short encoding, unsigned short parity)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	if (encoding != ENCODING_NRZ &&
	    encoding != ENCODING_NRZI &&
	    encoding != ENCODING_FM_MARK &&
	    encoding != ENCODING_FM_SPACE &&
	    encoding != ENCODING_MANCHESTER)
		return -EINVAL;

	if (parity != PARITY_NONE &&
	    parity != PARITY_CRC16_PR0_CCITT &&
	    parity != PARITY_CRC16_PR1_CCITT &&
	    parity != PARITY_CRC32_PR0_CCITT &&
	    parity != PARITY_CRC32_PR1_CCITT)
		return -EINVAL;

	priv->encoding = encoding;
	priv->parity = parity;
	return 0;
}


/*
 * Loading module
 */
static int pef2256_probe(struct platform_device *pdev)
{
	struct pef2256_dev_priv *priv;
	int ret = -ENOMEM;
	struct net_device *netdev;
	hdlc_device *hdlc;
	unsigned char *base_addr;
	struct device_node *np = (&pdev->dev)->of_node;
	const char *str_data;

	if (!pdev->dev.of_node)
		return -EINVAL;

	dev_err(&pdev->dev, "Found PEF2256\n");

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ret;

	priv->dev = &pdev->dev;

	if (of_property_read_u32(np, "clock-rate", &priv->clock_rate)) {
		dev_err(&pdev->dev, "failed to read clock-rate -> using 8Mhz\n");
		priv->clock_rate = CLOCK_RATE_8M;
	}

	if (of_property_read_u32(np, "data-rate", &priv->data_rate)) {
		dev_err(&pdev->dev, "failed to read data-rate -> using 8Mb\n");
		priv->data_rate = DATA_RATE_8M;
	}

	if (of_property_read_u32(np, "channel-phase", &priv->channel_phase)) {
		dev_err(&pdev->dev, "failed to read channel phase -> using 0\n");
		priv->channel_phase = CHANNEL_PHASE_0;
	}

	if (of_property_read_string(np, "rising-edge-sync-pulse", &str_data)) {
		dev_err(&pdev->dev, "failed to read rising edge sync pulse -> using \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else if (strcmp(str_data, "transmit") && strcmp(str_data, "receive")) {
		dev_err(&pdev->dev, "invalid rising edge sync pulse \"%s\" -> using \"transmit\"\n", str_data);
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else
		strncpy(priv->rising_edge_sync_pulse, str_data, 10);

	priv->irq = platform_get_irq(pdev, 0);
	if (!priv->irq) {
		dev_err(priv->dev, "no irq defined\n");
		return -EINVAL;
	}

	priv->base_addr = of_iomap(np, 0);
	if (!priv->base_addr) {
		dev_err(&pdev->dev, "of_iomap failed\n");
		ret = -ENOMEM;
		goto free_priv;
	}

	/* Get the component Id */
	base_addr = priv->base_addr;
	priv->component_id = VERSION_UNDEF;
	if (readb(base_addr + VSTR) == 0x00) {
		if ((readb(base_addr + WID) & WID_IDENT_1) ==
			WID_IDENT_1_2)
			priv->component_id = VERSION_1_2;
	} else if (readb(base_addr + VSTR) == 0x05) {
		if ((readb(base_addr + WID) & WID_IDENT_2) ==
			WID_IDENT_2_1)
			priv->component_id = VERSION_2_1;
		else if ((readb(base_addr + WID) & WID_IDENT_2) ==
			WID_IDENT_2_2)
			priv->component_id = VERSION_2_2;
	}

	priv->tx_skb = NULL;

	/* Default settings ; Rx and Tx use TS 1, mode = MASTER */
	priv->Rx_TS = 0x40000000;
	priv->Tx_TS = 0x40000000;
	priv->mode = 0;

	netdev = alloc_hdlcdev(priv);
	if (!netdev) {
		dev_err(&pdev->dev, "alloc_hdlcdev failed\n");
		ret = -ENOMEM;
		goto free_regs;
	}

	priv->netdev = netdev;
	hdlc = dev_to_hdlc(netdev);
	netdev->netdev_ops = &pef2256_ops;
	SET_NETDEV_DEV(netdev, &pdev->dev);
	hdlc->attach = pef2256_hdlc_attach;
	hdlc->xmit = pef2256_start_xmit;

	dev_set_drvdata(&pdev->dev, netdev);

	ret = register_hdlc_device(netdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't register hdlc device\n");
		goto free_dev;
	}

	/*
	 * These files are required to configure HDLC : mode
         * (master or slave), time slots used to transmit and
         * receive data. They are mandatory.
	 */
	ret = device_create_file(priv->dev, &dev_attr_mode);
	ret |= device_create_file(priv->dev, &dev_attr_Tx_TS);
	ret |= device_create_file(priv->dev, &dev_attr_Rx_TS);

	if (ret)
		goto remove_files;

	/*
	 * This file is only used to display debug infos.
	 * A failure can be safely ignored.
	 */
	device_create_file(priv->dev, &dev_attr_regs);

	priv->init_done = 0;

	return 0;

remove_files:
	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);
free_dev:
	free_netdev(priv->netdev);
free_regs:
	iounmap(priv->base_addr);
free_priv:;
	kfree(priv);

	return ret;
}


/*
 * Removing module
 */
static int pef2256_remove(struct platform_device *pdev)
{
	struct net_device *ndev = dev_get_drvdata(&pdev->dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;


	device_remove_file(priv->dev, &dev_attr_regs);
	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);

	free_netdev(priv->netdev);

	iounmap(priv->base_addr);

	kfree(priv);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(pdev);
	return 0;
}

static const struct of_device_id pef2256_match[] = {
	{
		.compatible = "infineon,pef2256",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pef2256_match);


static struct platform_driver pef2256_driver = {
	.probe		= pef2256_probe,
	.remove		= pef2256_remove,
	.driver		= {
		.name	= "pef2256",
		.owner	= THIS_MODULE,
		.of_match_table	= pef2256_match,
	},
};


module_platform_driver(pef2256_driver);

/* GENERAL INFORMATIONS */
MODULE_AUTHOR("CHANTELAUZE Jerome - April 2013");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Infineon PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
