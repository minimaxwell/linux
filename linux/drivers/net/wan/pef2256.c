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
	struct pef2256_regs *base_addr = (struct pef2256_regs *)priv->base_addr;

	netdev_info(ndev, "	MODE = 0x%02x\n", base_addr->MODE);
	netdev_info(ndev, "	RAH1 = 0x%02x\n", base_addr->RAH1);
	netdev_info(ndev, "	RAH2 = 0x%02x\n", base_addr->RAH2);
	netdev_info(ndev, "	RAL1 = 0x%02x\n", base_addr->RAL1);
	netdev_info(ndev, "	RAL2 = 0x%02x\n", base_addr->RAL2);
	netdev_info(ndev, "	IPC = 0x%02x\n", base_addr->IPC);
	netdev_info(ndev, "	CCR1 = 0x%02x\n", base_addr->CCR1);
	netdev_info(ndev, "	CCR2 = 0x%02x\n", base_addr->CCR2);
	netdev_info(ndev, "	RTR1 = 0x%02x\n", base_addr->RTR1);
	netdev_info(ndev, "	RTR2 = 0x%02x\n", base_addr->RTR2);
	netdev_info(ndev, "	RTR3 = 0x%02x\n", base_addr->RTR3);
	netdev_info(ndev, "	RTR4 = 0x%02x\n", base_addr->RTR4);
	netdev_info(ndev, "	TTR1 = 0x%02x\n", base_addr->TTR1);
	netdev_info(ndev, "	TTR2 = 0x%02x\n", base_addr->TTR2);
	netdev_info(ndev, "	TTR3 = 0x%02x\n", base_addr->TTR3);
	netdev_info(ndev, "	TTR4 = 0x%02x\n", base_addr->TTR4);
	netdev_info(ndev, "	IMR0 = 0x%02x\n", base_addr->IMR0);
	netdev_info(ndev, "	IMR1 = 0x%02x\n", base_addr->IMR1);
	netdev_info(ndev, "	IMR2 = 0x%02x\n", base_addr->IMR2);
	netdev_info(ndev, "	IMR3 = 0x%02x\n", base_addr->IMR3);
	netdev_info(ndev, "	IMR4 = 0x%02x\n", base_addr->IMR4);
	netdev_info(ndev, "	IMR5 = 0x%02x\n", base_addr->IMR5);
	netdev_info(ndev, "	IERR = 0x%02x\n", base_addr->IERR);
	netdev_info(ndev, "	FMR0 = 0x%02x\n", base_addr->FMR0);
	netdev_info(ndev, "	FMR1 = 0x%02x\n", base_addr->FMR1);
	netdev_info(ndev, "	FMR2 = 0x%02x\n", base_addr->FMR2);
	netdev_info(ndev, "	LOOP = 0x%02x\n", base_addr->LOOP);
	netdev_info(ndev, "	XSW = 0x%02x\n", base_addr->XSW);
	netdev_info(ndev, "	XSP = 0x%02x\n", base_addr->XSP);
	netdev_info(ndev, "	XC0 = 0x%02x\n", base_addr->XC0);
	netdev_info(ndev, "	XC1 = 0x%02x\n", base_addr->XC1);
	netdev_info(ndev, "	RC0 = 0x%02x\n", base_addr->RC0);
	netdev_info(ndev, "	RC1 = 0x%02x\n", base_addr->RC1);
	netdev_info(ndev, "	XPM0 = 0x%02x\n", base_addr->XPM0);
	netdev_info(ndev, "	XPM1 = 0x%02x\n", base_addr->XPM1);
	netdev_info(ndev, "	XPM2 = 0x%02x\n", base_addr->XPM2);
	netdev_info(ndev, "	TSWM = 0x%02x\n", base_addr->TSWM);
	netdev_info(ndev, "	IDLE = 0x%02x\n", base_addr->IDLE);
	netdev_info(ndev, "	XSA4 = 0x%02x\n", base_addr->XSA4);
	netdev_info(ndev, "	XSA5 = 0x%02x\n", base_addr->XSA5);
	netdev_info(ndev, "	XSA6 = 0x%02x\n", base_addr->XSA6);
	netdev_info(ndev, "	XSA7 = 0x%02x\n", base_addr->XSA7);
	netdev_info(ndev, "	XSA8 = 0x%02x\n", base_addr->XSA8);
	netdev_info(ndev, "	FMR3 = 0x%02x\n", base_addr->FMR3);
	netdev_info(ndev, "	ICB1 = 0x%02x\n", base_addr->ICB1);
	netdev_info(ndev, "	ICB2 = 0x%02x\n", base_addr->ICB2);
	netdev_info(ndev, "	ICB3 = 0x%02x\n", base_addr->ICB3);
	netdev_info(ndev, "	ICB4 = 0x%02x\n", base_addr->ICB4);
	netdev_info(ndev, "	LIM0 = 0x%02x\n", base_addr->LIM0);
	netdev_info(ndev, "	LIM1 = 0x%02x\n", base_addr->LIM1);
	netdev_info(ndev, "	PCD = 0x%02x\n", base_addr->PCD);
	netdev_info(ndev, "	PCR = 0x%02x\n", base_addr->PCR);
	netdev_info(ndev, "	LIM2 = 0x%02x\n", base_addr->LIM2);
	netdev_info(ndev, "	LCR1 = 0x%02x\n", base_addr->LCR1);
	netdev_info(ndev, "	LCR2 = 0x%02x\n", base_addr->LCR2);
	netdev_info(ndev, "	LCR3 = 0x%02x\n", base_addr->LCR3);
	netdev_info(ndev, "	SIC1 = 0x%02x\n", base_addr->SIC1);
	netdev_info(ndev, "	SIC2 = 0x%02x\n", base_addr->SIC2);
	netdev_info(ndev, "	SIC3 = 0x%02x\n", base_addr->SIC3);
	netdev_info(ndev, "	CMR1 = 0x%02x\n", base_addr->CMR1);
	netdev_info(ndev, "	CMR2 = 0x%02x\n", base_addr->CMR2);
	netdev_info(ndev, "	GCR = 0x%02x\n", base_addr->GCR);
	netdev_info(ndev, "	ESM = 0x%02x\n", base_addr->ESM);
	netdev_info(ndev, "	CMR3 = 0x%02x\n", base_addr->CMR3);
	netdev_info(ndev, "	PC1 = 0x%02x\n", base_addr->PC1);
	netdev_info(ndev, "	PC2 = 0x%02x\n", base_addr->PC2);
	netdev_info(ndev, "	PC3 = 0x%02x\n", base_addr->PC3);
	netdev_info(ndev, "	PC4 = 0x%02x\n", base_addr->PC4);
	netdev_info(ndev, "	PC5 = 0x%02x\n", base_addr->PC5);
	netdev_info(ndev, "	GPC1 = 0x%02x\n", base_addr->GPC1);
	netdev_info(ndev, "	PC6 = 0x%02x\n", base_addr->PC6);
	netdev_info(ndev, "	CCR3 = 0x%02x\n", base_addr->CCR3);
	netdev_info(ndev, "	CCR4 = 0x%02x\n", base_addr->CCR4);
	netdev_info(ndev, "	CCR5 = 0x%02x\n", base_addr->CCR5);
	netdev_info(ndev, "	MODE2 = 0x%02x\n", base_addr->MODE2);
	netdev_info(ndev, "	MODE3 = 0x%02x\n", base_addr->MODE3);
	netdev_info(ndev, "	RBC2 = 0x%02x\n", base_addr->RBC2);
	netdev_info(ndev, "	RBC3 = 0x%02x\n", base_addr->RBC3);
	netdev_info(ndev, "	GCM1 = 0x%02x\n", base_addr->GCM1);
	netdev_info(ndev, "	GCM2 = 0x%02x\n", base_addr->GCM2);
	netdev_info(ndev, "	GCM3 = 0x%02x\n", base_addr->GCM3);
	netdev_info(ndev, "	GCM4 = 0x%02x\n", base_addr->GCM4);
	netdev_info(ndev, "	GCM5 = 0x%02x\n", base_addr->GCM5);
	netdev_info(ndev, "	GCM6 = 0x%02x\n", base_addr->GCM6);
	netdev_info(ndev, "	SIS2/GCM7 = 0x%02x\n", base_addr->Dif1.SIS2);
	netdev_info(ndev, "	RSIS2/GCM8 = 0x%02x\n",
						base_addr->Dif2.RSIS2);
	netdev_info(ndev, "	TSEO = 0x%02x\n", base_addr->TSEO);
	netdev_info(ndev, "	TSBS1 = 0x%02x\n", base_addr->TSBS1);
	netdev_info(ndev, "	TSBS2 = 0x%02x\n", base_addr->TSBS2);
	netdev_info(ndev, "	TSBS3 = 0x%02x\n", base_addr->TSBS3);
	netdev_info(ndev, "	TSS2 = 0x%02x\n", base_addr->TSS2);
	netdev_info(ndev, "	TSS3 = 0x%02x\n", base_addr->TSS3);
	netdev_info(ndev, "	Res10 = 0x%02x\n", base_addr->Res10);
	netdev_info(ndev, "	Res11 = 0x%02x\n", base_addr->Res11);
	netdev_info(ndev, "	TPC0 = 0x%02x\n", base_addr->TPC0);
	netdev_info(ndev, "	GLC1 = 0x%02x\n", base_addr->GLC1);
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
	u32 value;
	int ret = kstrtol(buf, 10, (long int *)&value);
	int reconfigure = (value != priv->mode);

	if (ret != 0)
		return ret;

	if (value != MASTER_MODE && value != SLAVE_MODE)
		return -EINVAL;

	priv->mode = value;
	if (reconfigure && priv->init_done) {
		pef2256_close(ndev);
		init_FALC(priv);
		pef2256_open(ndev);
	}

	return count;
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
	u32 value;
	int ret = kstrtol(buf, 10, (long int *)&value);
	int reconfigure = (value != priv->mode);

	if (ret != 0)
		return ret;

	/* TS 0 is reserved */
	if (value & 0x80000000)
		return -EINVAL;

	priv->Tx_TS = value;
	if (reconfigure && priv->init_done)
		Config_HDLC(priv);

	return count;
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
	u32 value;
	int ret = kstrtol(buf, 10, (long int *)&value);
	int reconfigure = (value != priv->mode);

	if (ret != 0)
		return ret;

	/* TS 0 is reserved */
	if (value & 0x80000000)
		return -EINVAL;

	priv->Rx_TS = value;
	if (reconfigure && priv->init_done)
		Config_HDLC(priv);

	return count;
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
	struct pef2256_regs *base_addr;
	u8 dummy;

	/* Set framer E1 address */
	base_addr = (struct pef2256_regs *)priv->base_addr;

	/* Read to remove pending IT */
	dummy = base_addr->ISR0;
	dummy = base_addr->ISR1;

	/* Mask HDLC 1 Transmit IT */
	base_addr->IMR1 |= 1;
	base_addr->IMR1 |= 1 << 4;
	base_addr->IMR1 |= 1 << 5;

	/* Mask HDLC 1 Receive IT */
	base_addr->IMR0 |= 1;
	base_addr->IMR0 |= 1 << 7;
	base_addr->IMR1 |= 1 << 6;

	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->MODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->CCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->CCR2), 0x00);

	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->MODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->CCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->CCR2), 0x00);

	udelay((2 * 32) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   for FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->MODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* setting up Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->CCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->CCR2), 0x00);

	udelay((2 * 32) * 125);

	/* Init  Time Slot select */
	out_8(&(base_addr->TTR1), 0x00);
	out_8(&(base_addr->TTR2), 0x00);
	out_8(&(base_addr->TTR3), 0x00);
	out_8(&(base_addr->TTR4), 0x00);
	out_8(&(base_addr->RTR1), 0x00);
	out_8(&(base_addr->RTR2), 0x00);
	out_8(&(base_addr->RTR3), 0x00);
	out_8(&(base_addr->RTR4), 0x00);
	/* Set selected TS bits */
	/* Starting at TS 1, TS 0 is reserved */
	for (TS_idx = 1; TS_idx < 32; TS_idx++) {
		i = 7 - (TS_idx % 8);
		switch (TS_idx / 8) {
		case 0:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->TTR1), 1 << i);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->RTR1), 1 << i);
			break;
		case 1:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->TTR2), 1 << i);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->RTR2), 1 << i);
			break;
		case 2:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->TTR3), 1 << i);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->RTR3), 1 << i);
			break;
		case 3:
			if (priv->Tx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->TTR4), 1 << i);
			if (priv->Rx_TS & (1 << (31 - TS_idx)))
				setbits8(&(base_addr->RTR4), 1 << i);
			break;
		}
	}

	/* Unmask HDLC 1 Transmit IT */
	base_addr->IMR1 &= ~1;
	base_addr->IMR1 &= ~(1 << 4);
	base_addr->IMR1 &= ~(1 << 5);

	/* Unmask HDLC 1 Receive IT */
	base_addr->IMR0 &= ~1;
	base_addr->IMR0 &= ~(1 << 7);
	base_addr->IMR1 &= ~(1 << 6);

	return 0;
}


/*
 * Init FALC56
 */
static int init_FALC(struct pef2256_dev_priv *priv)
{
	struct pef2256_regs *base_addr;
	int Version;

	/* Get controller version */
	Version = priv->component_id;

	/* Init FALC56 */
	base_addr = (struct pef2256_regs *)priv->base_addr;
	/* RCLK output : DPLL clock, DCO-X enabled, DCO-X internal reference
	   clock */
	out_8(&(base_addr->CMR1), 0x00);
	/* SCLKR selected, SCLKX selected, receive synchro pulse sourced by
	   SYPR, transmit synchro pulse sourced by SYPX */
	out_8(&(base_addr->CMR2), 0x00);
	/* NRZ coding, no alarm simulation */
	out_8(&(base_addr->FMR0), 0x00);
	/* E1 double frame format, 2 Mbit/s system data rate, no AIS
	   transmission to remote end or system interface, payload loop
	   off, transmit remote alarm on */
	out_8(&(base_addr->FMR1), 0x00);
	out_8(&(base_addr->FMR2), 0x02);
	/* E1 default for LIM2 */
	out_8(&(base_addr->LIM2), 0x20);
	if (priv->mode == MASTER_MODE)
		/* SEC input, active high */
		out_8(&(base_addr->GPC1), 0x00);
	else
		/* FSC output, active high */
		out_8(&(base_addr->GPC1), 0x40);
	/* internal second timer, power on */
	out_8(&(base_addr->GCR), 0x00);
	/* slave mode, local loop off, mode short-haul */
	if (Version == VERSION_1_2)
		out_8(&(base_addr->LIM0), 0x00);
	else
		out_8(&(base_addr->LIM0), 0x08);
	/* analog interface selected, remote loop off */
	out_8(&(base_addr->LIM1), 0x00);
	if (Version == VERSION_1_2) {
		/* function of ports RP(A to D) : output receive sync pulse
		   function of ports XP(A to D) : output transmit line clock */
		out_8(&(base_addr->PC1), 0x77);
		out_8(&(base_addr->PC2), 0x77);
		out_8(&(base_addr->PC3), 0x77);
		out_8(&(base_addr->PC4), 0x77);
	} else {
		/* function of ports RP(A to D) : output high
		   function of ports XP(A to D) : output high */
		out_8(&(base_addr->PC1), 0xAA);
		out_8(&(base_addr->PC2), 0xAA);
		out_8(&(base_addr->PC3), 0xAA);
		out_8(&(base_addr->PC4), 0xAA);
	}
	/* function of port RPA : input SYPR
	   function of port XPA : input SYPX */
	out_8(&(base_addr->PC1), 0x00);
	/* SCLKR, SCLKX, RCLK configured to inputs,
	   XFMS active low, CLK1 and CLK2 pin configuration */
	out_8(&(base_addr->PC5), 0x00);
	out_8(&(base_addr->PC6), 0x00);
	/* the receive clock offset is cleared
	   the receive time slot offset is cleared */
	out_8(&(base_addr->RC0), 0x00);
	out_8(&(base_addr->RC1), 0x9C);
	/* 2.048 MHz system clocking rate, receive buffer 2 frames, transmit
	   buffer bypass, data sampled and transmitted on the falling edge of
	   SCLKR/X, automatic freeze signaling, data is active in the first
	   channel phase */
	out_8(&(base_addr->SIC1), 0x00);
	out_8(&(base_addr->SIC2), 0x00);
	out_8(&(base_addr->SIC3), 0x00);
	/* channel loop-back and single frame mode are disabled */
	out_8(&(base_addr->LOOP), 0x00);
	/* all bits of the transmitted service word are cleared */
	out_8(&(base_addr->XSW), 0x1F);
	/* spare bit values are cleared */
	out_8(&(base_addr->XSP), 0x00);
	/* no transparent mode active */
	out_8(&(base_addr->TSWM), 0x00);
	/* the transmit clock offset is cleared
	   the transmit time slot offset is cleared */
	out_8(&(base_addr->XC0), 0x00);
	out_8(&(base_addr->XC1), 0x9C);
	/* transmitter in tristate mode */
	out_8(&(base_addr->XPM2), 0x40);
	/* transmit pulse mask */
	if (Version != VERSION_1_2)
		out_8(&(base_addr->XPM0), 0x9C);

	if (Version == VERSION_1_2) {
		/* master clock is 16,384 MHz (flexible master clock) */
		out_8(&(base_addr->GCM2), 0x58);
		out_8(&(base_addr->GCM3), 0xD2);
		out_8(&(base_addr->GCM4), 0xC2);
		out_8(&(base_addr->GCM5), 0x07);
		out_8(&(base_addr->GCM6), 0x10);
	} else {
		/* master clock is 16,384 MHz (flexible master clock) */
		out_8(&(base_addr->GCM2), 0x18);
		out_8(&(base_addr->GCM3), 0xFB);
		out_8(&(base_addr->GCM4), 0x0B);
		out_8(&(base_addr->GCM5), 0x01);
		out_8(&(base_addr->GCM6), 0x0B);
		out_8(&(base_addr->Dif1.GCM7), 0xDB);
		out_8(&(base_addr->Dif2.GCM8), 0xDF);
	}

	/* master mode => LIM0.MAS = 1 (bit 0) */
	if (priv->mode == MASTER_MODE)
		setbits8(&(base_addr->LIM0), 1 << 0);

	/* transmit line in normal operation => XPM2.XLT = 0 (bit 6) */
	clrbits8(&(base_addr->XPM2), 1 << 6);

	if (Version == VERSION_1_2) {
		/* receive input threshold = 0,21V =>
			LIM1.RIL2:0 = 101 (bits 6, 5 et 4) */
		setbits8(&(base_addr->LIM1), 1 << 4);
		setbits8(&(base_addr->LIM1), 1 << 6);
	} else {
		/* receive input threshold = 0,21V =>
			LIM1.RIL2:0 = 100 (bits 6, 5 et 4) */
		setbits8(&(base_addr->LIM1), 1 << 6);
	}
	/* transmit line coding = HDB3 => FMR0.XC1:0 = 11 (bits 7 et 6) */
	setbits8(&(base_addr->FMR0), 1 << 6);
	setbits8(&(base_addr->FMR0), 1 << 7);
	/* receive line coding = HDB3 => FMR0.RC1:0 = 11 (bits 5 et 4) */
	setbits8(&(base_addr->FMR0), 1 << 4);
	setbits8(&(base_addr->FMR0), 1 << 5);
	/* detection of LOS alarm = 176 pulses (soit (10 + 1) * 16) */
	out_8(&(base_addr->PCD), 10);
	/* recovery of LOS alarm = 22 pulses (soit 21 + 1) */
	out_8(&(base_addr->PCR), 21);
	/* DCO-X center frequency => CMR2.DCOXC = 1 (bit 5) */
	setbits8(&(base_addr->CMR2), 1 << 5);
	if (priv->mode == SLAVE_MODE) {
		/* select RCLK source = 2M => CMR1.RS(1:0) = 10 (bits 5 et 4) */
		setbits8(&(base_addr->CMR1), 1 << 5);
		/* disable switching RCLK -> SYNC => CMR1.DCS = 1 (bit 3) */
		setbits8(&(base_addr->CMR1), 1 << 3);
	}
	if (Version != VERSION_1_2)
		/* during inactive channel phase RDO into tri-state mode */
		setbits8(&(base_addr->SIC3), 1 << 5);
	if (!strcmp(priv->rising_edge_sync_pulse, "transmit")) {
		/* rising edge sync pulse transmit => SIC3.RESX = 1 (bit 3) */
		setbits8(&(base_addr->SIC3), 1 << 3);
	} else {
		/* rising edge sync pulse receive => SIC3.RESR = 1 (bit 2) */
		setbits8(&(base_addr->SIC3), 1 << 2);
	}
	/* transmit offset counter = 4
	   => XC0.XCO10:8 = 000 (bits 2, 1 et 0);
	      XC1.XCO7:0 = 4 (bits 7 ... 0) */
	out_8(&(base_addr->XC1), 4);
	/* receive offset counter = 4
	   => RC0.RCO10:8 = 000 (bits 2, 1 et 0);
	      RC1.RCO7:0 = 4 (bits 7 ... 0) */
	out_8(&(base_addr->RC1), 4);

	/* clocking rate 8M and data rate 2M on the system highway */
	setbits8(&(base_addr->SIC1), 1 << 7);
	/* data rate 4M on the system highway */
	if (priv->data_rate == DATA_RATE_4M)
		setbits8(&(base_addr->FMR1), 1 << 1);
	/* data rate 8M on the system highway */
	if (priv->data_rate == DATA_RATE_8M)
		setbits8(&(base_addr->SIC1), 1 << 6);
	/* channel phase for FALC56 */
	if ((priv->channel_phase == CHANNEL_PHASE_1)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		setbits8(&(base_addr->SIC2), 1 << 1);
	if ((priv->channel_phase == CHANNEL_PHASE_2)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		setbits8(&(base_addr->SIC2), 1 << 2);

	if (priv->mode == SLAVE_MODE) {
		/* transmit buffer size = 2 frames =>
			SIC1.XBS1:0 = 10 (bits 1 et 0) */
		setbits8(&(base_addr->SIC1), 1 << 1);
	}

	/* transmit in multiframe => FMR1.XFS = 1 (bit 3) */
	setbits8(&(base_addr->FMR1), 1 << 3);
	/* receive in multiframe => FMR2.RFS1:0 = 10 (bits 7 et 6) */
	setbits8(&(base_addr->FMR2), 1 << 7);
	/* Automatic transmission of submultiframe status =>
		XSP.AXS = 1 (bit 3) */
	setbits8(&(base_addr->XSP), 1 << 3);

	/* error counter mode toutes les 1s => FMR1.ECM = 1 (bit 2) */
	setbits8(&(base_addr->FMR1), 1 << 2);
	/* error counter mode COFA => GCR.ECMC = 1 (bit 4) */
	setbits8(&(base_addr->GCR), 1 << 4);
	/* errors in service words with no influence => RC0.SWD = 1 (bit 7) */
	setbits8(&(base_addr->RC0), 1 << 7);
	/* 4 consecutive incorrect FAS = loss of sync => RC0.ASY4 = 1 (bit 6) */
	setbits8(&(base_addr->RC0), 1 << 6);
	/* Si-Bit in service word from XDI => XSW.XSIS = 1 (bit 7) */
	setbits8(&(base_addr->XSW), 1 << 7);
	/* Si-Bit in FAS word from XDI => XSP.XSIF = 1 (bit 2) */
	setbits8(&(base_addr->XSP), 1 << 2);

	/* port RCLK is output => PC5.CRP = 1 (bit 0) */
	setbits8(&(base_addr->PC5), 1 << 0);
	/* visibility of the masked interrupts => GCR.VIS = 1 (bit 7) */
	setbits8(&(base_addr->GCR), 1 << 7);
	/* reset lines
	   => CMDR.RRES = 1 (bit 6); CMDR.XRES = 1 (bit 4);
	      CMDR.SRES = 1 (bit 0) */
	out_8(&(base_addr->CMDR), 0x51);

	return 0;
}



static int pef2256_open(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	struct pef2256_regs *base_addr = (struct pef2256_regs *)priv->base_addr;
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
			base_addr->VSTR, base_addr->WID, priv->component_id);
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

	/* Do E1 stuff */

	return 0;
}



static int pef2256_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int ret;

	ret = hdlc_ioctl(dev, ifr, cmd);
	return ret;
}

static int pef2256_rx(struct pef2256_dev_priv *priv)
{
	struct sk_buff *skb;
	int idx, size;
	struct pef2256_regs *base_addr;

	base_addr = priv->base_addr;

	/* RDO has been received -> wait for RME */
	if (priv->rx_len == -1) {
		/* Acknowledge the FIFO */
		setbits8(&(base_addr->CMDR), 1 << 7);

		if (priv->ISR0 & (1 << 7))
			priv->rx_len = 0;

		return 0;
	}

	/* RPF : a block is available in the receive FIFO */
	if (priv->ISR0 & 1) {
		for (idx = 0; idx < 32; idx++)
			priv->rx_buff[priv->rx_len + idx] =
				base_addr->FIFO.RFIFO[idx & 1];

		/* Acknowledge the FIFO */
		setbits8(&(base_addr->CMDR), 1 << 7);

		priv->rx_len += 32;
	}

	/* RME : Message end : Read the receive FIFO */
	if (priv->ISR0 & (1 << 7)) {
		/* Get size of last block */
		size = base_addr->RBCL & 0x1F;

		/* Read last block */
		for (idx = 0; idx < size; idx++)
			priv->rx_buff[priv->rx_len + idx] =
				base_addr->FIFO.RFIFO[idx & 1];

		/* Acknowledge the FIFO */
		setbits8(&(base_addr->CMDR), 1 << 7);

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
	struct pef2256_regs *base_addr;
	u8 *tx_buff = priv->tx_skb->data;

	base_addr = (struct pef2256_regs *)priv->base_addr;

	/* ALLS : transmit all done */
	if (priv->ISR1 & (1 << 5)) {
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
			base_addr->FIFO.XFIFO[idx & 1] =
				tx_buff[priv->tx_len + idx];

		priv->tx_len += size;

		if (priv->tx_len == priv->tx_skb->len)
			base_addr->CMDR |= ((1 << 3) | (1 << 1));
		else
			setbits8(&(base_addr->CMDR), 1 << 3);
	}

	return 0;
}


irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	struct pef2256_dev_priv *priv = (struct pef2256_dev_priv *)dev_priv;
	struct pef2256_regs *base_addr;
	u8 GIS;

	base_addr = (struct pef2256_regs *)priv->base_addr;
	GIS = base_addr->GIS;

	priv->ISR0 = priv->ISR1 = 0;

	/* We only care about ISR0 and ISR1 */
	/* ISR0 */
	if (GIS & 1)
		priv->ISR0 = base_addr->ISR0 & ~(base_addr->IMR0);
	/* ISR1 */
	if (GIS & (1 << 1))
		priv->ISR1 = base_addr->ISR1 & ~(base_addr->IMR1);

	/* Don't do anything else before init is done */
	if (!priv->init_done)
		return IRQ_HANDLED;

	/* RDO : Receive data overflow -> RX error */
	if (priv->ISR1 & (1 << 6)) {
		/* Acknowledge the FIFO */
		setbits8(&(base_addr->CMDR), 1 << 7);
		priv->netdev->stats.rx_errors++;
		/* RME received ? */
		if (priv->ISR0 & (1 << 7))
			priv->rx_len = 0;
		else
			priv->rx_len = -1;
		return IRQ_HANDLED;
	}

	/* XDU : Transmit data underrun -> TX error */
	if (priv->ISR1 & (1 << 4)) {
		priv->netdev->stats.tx_errors++;
		/* dev_kfree_skb(priv->tx_skb); */
		priv->tx_skb = NULL;
		netif_wake_queue(priv->netdev);
		return IRQ_HANDLED;
	}

	/* RPF or RME : FIFO received */
	if (priv->ISR0 & (1 | (1 << 7)))
		pef2256_rx(priv);

	/* XPR or ALLS : FIFO sent */
	if (priv->ISR1 & (1 | (1 << 5)))
		pef2256_tx(priv);

	return IRQ_HANDLED;
}


static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	int idx, size;
	struct pef2256_regs *base_addr;
	u8 *tx_buff = skb->data;

	base_addr = (struct pef2256_regs *)priv->base_addr;

	priv->tx_skb = skb;
	priv->tx_len = 0;

	size = priv->tx_skb->len - priv->tx_len;
	if (size > 32)
		size = 32;

	for (idx = 0; idx < size; idx++)
		base_addr->FIFO.XFIFO[idx & 1] = tx_buff[priv->tx_len + idx];

	priv->tx_len += size;

	setbits8(&(base_addr->CMDR), 1 << 3);
	if (priv->tx_len == priv->tx_skb->len)
		setbits8(&(base_addr->CMDR), 1 << 1);

	netif_stop_queue(netdev);
	return NETDEV_TX_OK;
}

static const struct net_device_ops pef2256_ops = {
	.ndo_open       = pef2256_open,
	.ndo_stop       = pef2256_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = pef2256_ioctl,
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
static const struct of_device_id pef2256_match[];
static int pef2256_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct pef2256_dev_priv *priv;
	int ret = -ENOMEM;
	struct net_device *netdev;
	hdlc_device *hdlc;
	int sys_ret;
	struct pef2256_regs *base_addr;
	struct device_node *np = (&ofdev->dev)->of_node;
	const u32 *data;
	int len;

	match = of_match_device(pef2256_match, &ofdev->dev);
	if (!match)
		return -EINVAL;

	dev_err(&ofdev->dev, "Found PEF2256\n");

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ret;

	priv->dev = &ofdev->dev;

	data = of_get_property(np, "data-rate", &len);
	if (!data || len != 4) {
		dev_err(&ofdev->dev, "failed to read data-rate -> using 8Mb\n");
		priv->data_rate = DATA_RATE_8M;
	} else
		priv->data_rate = *data;

	data = of_get_property(np, "channel-phase", &len);
	if (!data || len != 4) {
		dev_err(&ofdev->dev, "failed to read channel phase -> using 0\n");
		priv->channel_phase = CHANNEL_PHASE_0;
	} else
		priv->channel_phase = *data;

	data = of_get_property(np, "rising-edge-sync-pulse", NULL);
	if (!data) {
		dev_err(&ofdev->dev, "failed to read rising edge sync pulse -> using \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else if (strcmp((char *)data, "transmit") &&
			strcmp((char *)data, "receive")) {
		dev_err(&ofdev->dev, "invalid rising edge sync pulse -> using \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else
		strncpy(priv->rising_edge_sync_pulse, (char *)data, 10);

	priv->irq = of_irq_to_resource(np, 0, NULL);
	if (!priv->irq) {
		dev_err(priv->dev, "no irq defined\n");
		return -EINVAL;
	}

	priv->base_addr = of_iomap(np, 0);
	if (!priv->base_addr) {
		dev_err(&ofdev->dev, "of_iomap failed\n");
		kfree(priv);
		return ret;
	}

	/* Get the component Id */
	base_addr = (struct pef2256_regs *)priv->base_addr;
	priv->component_id = VERSION_UNDEF;
	if (base_addr->VSTR == 0x00) {
		if ((base_addr->WID & WID_IDENT_1) ==
			WID_IDENT_1_2)
			priv->component_id = VERSION_1_2;
	} else if (base_addr->VSTR == 0x05) {
		if ((base_addr->WID & WID_IDENT_2) ==
			WID_IDENT_2_1)
			priv->component_id = VERSION_2_1;
		else if ((base_addr->WID & WID_IDENT_2) == WID_IDENT_2_2)
			priv->component_id = VERSION_2_2;
	}

	priv->tx_skb = NULL;

	/* Default settings ; Rx and Tx use TS 1, mode = MASTER */
	priv->Rx_TS = 0x40000000;
	priv->Tx_TS = 0x40000000;
	priv->mode = 0;

	netdev = alloc_hdlcdev(priv);
	if (!netdev) {
		ret = -ENOMEM;
		return ret;
	}

	priv->netdev = netdev;
	hdlc = dev_to_hdlc(netdev);
	netdev->netdev_ops = &pef2256_ops;
	SET_NETDEV_DEV(netdev, &ofdev->dev);
	hdlc->attach = pef2256_hdlc_attach;
	hdlc->xmit = pef2256_start_xmit;

	dev_set_drvdata(&ofdev->dev, netdev);

	ret = register_hdlc_device(netdev);
	if (ret < 0) {
		pr_err("unable to register\n");
		return ret;
	}

	sys_ret = 0;
	sys_ret |= device_create_file(priv->dev, &dev_attr_mode);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Tx_TS);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Rx_TS);
	sys_ret |= device_create_file(priv->dev, &dev_attr_regs);

	if (sys_ret) {
		device_remove_file(priv->dev, &dev_attr_mode);
		unregister_hdlc_device(priv->netdev);
		free_netdev(priv->netdev);
	}

	priv->init_done = 0;

	return 0;
}


/*
 * Suppression du module
 */
static int pef2256_remove(struct platform_device *ofdev)
{
	struct net_device *ndev = dev_get_drvdata(&ofdev->dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);
	free_netdev(priv->netdev);

	/* Do E1 stuff */

	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(ofdev);
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


static int __init pef2256_init(void)
{
	int ret;
	ret = platform_driver_register(&pef2256_driver);
	return ret;
}
module_init(pef2256_init);


static void __exit pef2256_exit(void)
{
	platform_driver_unregister(&pef2256_driver);
}
module_exit(pef2256_exit);


/* GENERAL INFORMATIONS */
MODULE_AUTHOR("CHANTELAUZE Jerome - April 2013");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("Infineon PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
