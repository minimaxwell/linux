/* drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
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
#include <linux/of_address.h>

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

static int pef2256_open(struct net_device *netdev);
static int pef2256_close(struct net_device *netdev);

/* helper function - Read a register */
static u8 pef2256_r8(struct pef2256_dev_priv *priv, u32 offset)
{
	return ioread8(priv->ioaddr + offset);
}

/* helper function - Read a register */
static u16 pef2256_r16(struct pef2256_dev_priv *priv, u32 offset)
{
	return ioread16be(priv->ioaddr + offset);
}

/* helper function - Write a value to a register */
static void pef2256_w8(struct pef2256_dev_priv *priv, u32 offset, u8 val)
{
	iowrite8(val, priv->ioaddr + offset);
}

static void pef2256_w16(struct pef2256_dev_priv *priv, u32 offset, u16 val)
{
	iowrite16be(val, priv->ioaddr + offset);
}

/* helper function - Clear bits in a register */
static void pef2256_c8(struct pef2256_dev_priv *priv, u32 offset, u8 mask)
{
	u8 val = pef2256_r8(priv, offset);
	iowrite8(val & ~mask, priv->ioaddr + offset);
}

/* helper function - Set bits in a register */
static void pef2256_s8(struct pef2256_dev_priv *priv, u32 offset, u8 mask)
{
	u8 val = pef2256_r8(priv, offset);
	iowrite8(val | mask, priv->ioaddr + offset);
}

static void config_hdlc_timeslot(struct pef2256_dev_priv *priv, int ts)
{
	static struct {
		u32 ttr;
		u32 rtr;
	} regs[] = {
		{ TTR1, RTR1 },
		{ TTR2, RTR2 },
		{ TTR3, RTR3 },
		{ TTR4, RTR4 },
	};
	int cfg_bit = 1 << (31 - ts);
	int reg_bit = 1 << (7 - (ts % 8));
	int j = ts / 8;

	if (j >= 4)
		return;

	if (priv->Tx_TS & cfg_bit)
		pef2256_s8(priv, regs[j].ttr, 1 << reg_bit);

	if (priv->Rx_TS & cfg_bit)
		pef2256_s8(priv, regs[j].rtr, 1 << reg_bit);
}


/* Setting up HDLC channel */
static void config_hdlc(struct pef2256_dev_priv *priv)
{
	int TS_idx;
	u8 dummy;

	/* Read to remove pending IT */
	dummy = pef2256_r8(priv, ISR0);
	dummy = pef2256_r8(priv, ISR1);
	dummy = pef2256_r8(priv, ISR2);

	/* Mask HDLC 1 Transmit IT */
	pef2256_s8(priv, IMR1, IMR1_XPR);
	pef2256_s8(priv, IMR1, IMR1_XDU);
	pef2256_s8(priv, IMR1, IMR1_ALLS);

	/* Mask HDLC 1 Receive IT */
	pef2256_s8(priv, IMR0, IMR0_RPF);
	pef2256_s8(priv, IMR0, IMR0_RME);
	pef2256_s8(priv, IMR1, IMR1_RDO);

	/* Mask errors IT */
	pef2256_s8(priv, IMR0, IMR0_PDEN);
	pef2256_s8(priv, IMR2, IMR2_LOS);
	pef2256_s8(priv, IMR2, IMR2_AIS);

	udelay(FALC_HW_CMD_DELAY_US);

	/* MODE.HRAC = 0 (Receiver inactive)
	 * MODE.DIV = 0 (Data normal operation)
	 * for FALC V2.2 : MODE.HDLCI = 0 (normal operation)
	 * MODE.MDS2:0 = 100 (No address comparison)
	 * MODE.HRAC = 1 (Receiver active)
	 */
	pef2256_w8(priv, MODE, 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	 * CCR1.XMFA = 0 (No transmit multiframe alignment)
	 * CCR1.RFT1:0 = 00 (RFIFO is 32 bytes)
	 * setting up Interframe Time Fill
	 * CCR1.ITF = 1 (Interframe Time Fill Continuous flag)
	 */
	pef2256_w8(priv, CCR1, 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	 * CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	 * CCR2.RADD = 0 (No write address in RFIFO)
	 */
	pef2256_w8(priv, CCR2, 0x00);

	udelay(FALC_HW_CMD_DELAY_US);

	/* Init  Time Slot select */
	pef2256_w8(priv, TTR1, 0x00);
	pef2256_w8(priv, TTR2, 0x00);
	pef2256_w8(priv, TTR3, 0x00);
	pef2256_w8(priv, TTR4, 0x00);
	pef2256_w8(priv, RTR1, 0x00);
	pef2256_w8(priv, RTR2, 0x00);
	pef2256_w8(priv, RTR3, 0x00);
	pef2256_w8(priv, RTR4, 0x00);
	/* Set selected TS bits */
	/* Starting at TS 1, TS 0 is reserved */
	for (TS_idx = 1; TS_idx < 32; TS_idx++)
		config_hdlc_timeslot(priv, TS_idx);

	udelay(FALC_HW_CMD_DELAY_US);

	/* Unmask HDLC 1 Transmit IT */
	pef2256_c8(priv, IMR1, IMR1_XPR);
	pef2256_c8(priv, IMR1, IMR1_XDU);
	pef2256_c8(priv, IMR1, IMR1_ALLS);

	/* Unmask HDLC 1 Receive IT */
	pef2256_c8(priv, IMR0, IMR0_RPF);
	pef2256_c8(priv, IMR0, IMR0_RME);
	pef2256_c8(priv, IMR1, IMR1_RDO);

	/* Unmask errors IT */
	pef2256_c8(priv, IMR0, IMR0_PDEN);
	pef2256_c8(priv, IMR2, IMR2_LOS);
	pef2256_c8(priv, IMR2, IMR2_AIS);
}


static void init_falc(struct pef2256_dev_priv *priv)
{
	int version;

	/* Get controller version */
	version = priv->component_id;

	/* Init FALC56 */
	/* RCLK output : DPLL clock, DCO-X enabled, DCO-X internal reference
	 * clock
	 */
	pef2256_w8(priv, CMR1, 0x00);
	/* SCLKR selected, SCLKX selected, receive synchro pulse sourced by
	 * SYPR, transmit synchro pulse sourced by SYPX
	 */
	pef2256_w8(priv, CMR2, 0x00);
	/* NRZ coding, no alarm simulation */
	pef2256_w8(priv, FMR0, 0x00);
	/* E1 double frame format, 2 Mbit/s system data rate, no AIS
	 * transmission to remote end or system interface, payload loop
	 * off, transmit remote alarm on
	 */
	pef2256_w8(priv, FMR1, 0x00);
	pef2256_w8(priv, FMR2, 0x02);
	/* E1 default for LIM2 */
	pef2256_w8(priv, LIM2, 0x20);
	if (priv->mode == MASTER_MODE)
		/* SEC input, active high */
		pef2256_w8(priv, GPC1, 0x00);
	else
		/* FSC output, active high */
		pef2256_w8(priv, GPC1, 0x40);
	/* internal second timer, power on */
	pef2256_w8(priv, GCR, 0x00);
	/* slave mode, local loop off, mode short-haul */
	if (version == VERSION_1_2)
		pef2256_w8(priv, LIM0, 0x00);
	else
		pef2256_w8(priv, LIM0, 0x08);
	/* analog interface selected, remote loop off */
	pef2256_w8(priv, LIM1, 0x00);
	if (version == VERSION_1_2) {
		/* function of ports RP(A to D) : output receive sync pulse
		 * function of ports XP(A to D) : output transmit line clock
		 */
		pef2256_w8(priv, PC1, 0x77);
		pef2256_w8(priv, PC2, 0x77);
		pef2256_w8(priv, PC3, 0x77);
		pef2256_w8(priv, PC4, 0x77);
	} else {
		/* function of ports RP(A to D) : output high
		 * function of ports XP(A to D) : output high
		 */
		pef2256_w8(priv, PC1, 0xAA);
		pef2256_w8(priv, PC2, 0xAA);
		pef2256_w8(priv, PC3, 0xAA);
		pef2256_w8(priv, PC4, 0xAA);
	}
	/* function of port RPA : input SYPR
	 * function of port XPA : input SYPX
	 */
	pef2256_w8(priv, PC1, 0x00);
	/* SCLKR, SCLKX, RCLK configured to inputs,
	 * XFMS active low, CLK1 and CLK2 pin configuration
	 */
	pef2256_w8(priv, PC5, 0x00);
	pef2256_w8(priv, PC6, 0x00);
	/* the receive clock offset is cleared
	 * the receive time slot offset is cleared
	 */
	pef2256_w8(priv, RC0, 0x00);
	pef2256_w8(priv, RC1, 0x9C);
	/* 2.048 MHz system clocking rate, receive buffer 2 frames, transmit
	 * buffer bypass, data sampled and transmitted on the falling edge of
	 * SCLKR/X, automatic freeze signaling, data is active in the first
	 * channel phase
	 */
	pef2256_w8(priv, SIC1, 0x00);
	pef2256_w8(priv, SIC2, 0x00);
	pef2256_w8(priv, SIC3, 0x00);
	/* channel loop-back and single frame mode are disabled */
	pef2256_w8(priv, LOOP, 0x00);
	/* all bits of the transmitted service word are cleared */
	pef2256_w8(priv, XSW, 0x1F);
	/* spare bit values are cleared */
	pef2256_w8(priv, XSP, 0x00);
	/* no transparent mode active */
	pef2256_w8(priv, TSWM, 0x00);
	/* the transmit clock offset is cleared
	 * the transmit time slot offset is cleared
	 */
	pef2256_w8(priv, XC0, 0x00);
	pef2256_w8(priv, XC1, 0x9C);
	/* transmitter in tristate mode */
	pef2256_w8(priv, XPM2, 0x40);
	/* transmit pulse mask */
	if (version != VERSION_1_2)
		pef2256_w8(priv, XPM0, 0x9C);

	if (version == VERSION_1_2) {
		/* master clock is 16,384 MHz (flexible master clock) */
		pef2256_w8(priv, GCM2, 0x58);
		pef2256_w8(priv, GCM3, 0xD2);
		pef2256_w8(priv, GCM4, 0xC2);
		pef2256_w8(priv, GCM5, 0x07);
		pef2256_w8(priv, GCM6, 0x10);
	} else {
		/* master clock is 16,384 MHz (flexible master clock) */
		pef2256_w8(priv, GCM2, 0x18);
		pef2256_w8(priv, GCM3, 0xFB);
		pef2256_w8(priv, GCM4, 0x0B);
		pef2256_w8(priv, GCM5, 0x01);
		pef2256_w8(priv, GCM6, 0x0B);
		pef2256_w8(priv, GCM7, 0xDB);
		pef2256_w8(priv, GCM8, 0xDF);
	}

	/* master mode */
	if (priv->mode == MASTER_MODE)
		pef2256_s8(priv, LIM0, LIM0_MAS);

	/* transmit line in normal operation */
	pef2256_c8(priv, XPM2, XPM2_XLT);

	if (version == VERSION_1_2) {
		/* receive input threshold = 0,21V */
		pef2256_s8(priv, LIM1, LIM1_RIL0);
		pef2256_c8(priv, LIM1, LIM1_RIL1);
		pef2256_s8(priv, LIM1, LIM1_RIL2);
	} else {
		/* receive input threshold = 0,21V */
		pef2256_c8(priv, LIM1, LIM1_RIL0);
		pef2256_c8(priv, LIM1, LIM1_RIL1);
		pef2256_s8(priv, LIM1, LIM1_RIL2);
	}
	/* transmit line coding = HDB3 */
	pef2256_s8(priv, FMR0, FMR0_XC0);
	pef2256_s8(priv, FMR0, FMR0_XC1);
	/* receive line coding = HDB3 */
	pef2256_s8(priv, FMR0, FMR0_RC0);
	pef2256_s8(priv, FMR0, FMR0_RC1);
	/* detection of LOS alarm = 176 pulses (soit (10 + 1) * 16) */
	pef2256_w8(priv, PCD, 10);
	/* recovery of LOS alarm = 22 pulses (soit 21 + 1) */
	pef2256_w8(priv, PCR, 21);
	/* DCO-X center frequency => CMR2.DCOXC */
	pef2256_s8(priv, CMR2, CMR2_DCOXC);
	if (priv->mode == SLAVE_MODE) {
		/* select RCLK source = 2M */
		pef2256_c8(priv, CMR1, CMR1_RS0);
		pef2256_s8(priv, CMR1, CMR1_RS1);
		/* disable switching RCLK -> SYNC */
		pef2256_s8(priv, CMR1, CMR1_DCS);
	}
	if (version != VERSION_1_2)
		/* during inactive channel phase RDO into tri-state mode */
		pef2256_s8(priv, SIC3, 1 << 5);
	if (!strcmp(priv->rising_edge_sync_pulse, "transmit")) {
		/* rising edge sync pulse transmit */
		pef2256_c8(priv, SIC3, SIC3_RESR);
		pef2256_s8(priv, SIC3, SIC3_RESX);
	} else {
		/* rising edge sync pulse receive */
		pef2256_c8(priv, SIC3, SIC3_RESX);
		pef2256_s8(priv, SIC3, SIC3_RESR);
	}
	/* transmit offset counter = 4
	 *  => XC0.XCO10:8 = 000 (bits 2, 1 et 0);
	 *     XC1.XCO7:0 = 4 (bits 7 ... 0)
	 */
	pef2256_w8(priv, XC1, 4);
	/* receive offset counter = 4
	 * => RC0.RCO10:8 = 000 (bits 2, 1 et 0);
	 *    RC1.RCO7:0 = 4 (bits 7 ... 0)
	 */
	pef2256_w8(priv, RC1, 4);

	/* Clocking rate for FALC56 */

	/* Nothing to do for clocking rate 2M  */

	/* clocking rate 4M  */
	if (priv->clock_rate == CLOCK_RATE_4M)
		pef2256_s8(priv, SIC1, SIC1_SSC0);

	/* clocking rate 8M  */
	if (priv->clock_rate == CLOCK_RATE_8M)
		pef2256_s8(priv, SIC1, SIC1_SSC1);

	/* clocking rate 16M  */
	if (priv->clock_rate == CLOCK_RATE_16M) {
		pef2256_s8(priv, SIC1, SIC1_SSC0);
		pef2256_s8(priv, SIC1, SIC1_SSC1);
	}

	/* data rate for FALC56 */

	/* Nothing to do for data rate 2M on the system data bus */

	/* data rate 4M on the system data bus */
	if (priv->data_rate == DATA_RATE_4M)
		pef2256_s8(priv, FMR1, FMR1_SSD0);

	/* data rate 8M on the system data bus */
	if (priv->data_rate == DATA_RATE_8M)
		pef2256_s8(priv, SIC1, SIC1_SSD1);

	/* data rate 16M on the system data bus */
	if (priv->data_rate == DATA_RATE_16M) {
		pef2256_s8(priv, FMR1, FMR1_SSD0);
		pef2256_s8(priv, SIC1, SIC1_SSD1);
	}

	/* channel phase for FALC56 */

	/* Nothing to do for channel phase 1 */

	if (priv->channel_phase == CHANNEL_PHASE_2)
		pef2256_s8(priv, SIC2, SIC2_SICS0);

	if (priv->channel_phase == CHANNEL_PHASE_3)
		pef2256_s8(priv, SIC2, SIC2_SICS1);

	if (priv->channel_phase == CHANNEL_PHASE_4) {
		pef2256_s8(priv, SIC2, SIC2_SICS0);
		pef2256_s8(priv, SIC2, SIC2_SICS1);
	}

	if (priv->channel_phase == CHANNEL_PHASE_5)
		pef2256_s8(priv, SIC2, SIC2_SICS2);

	if (priv->channel_phase == CHANNEL_PHASE_6) {
		pef2256_s8(priv, SIC2, SIC2_SICS0);
		pef2256_s8(priv, SIC2, SIC2_SICS2);
	}

	if (priv->channel_phase == CHANNEL_PHASE_7) {
		pef2256_s8(priv, SIC2, SIC2_SICS1);
		pef2256_s8(priv, SIC2, SIC2_SICS2);
	}

	if (priv->channel_phase == CHANNEL_PHASE_8) {
		pef2256_s8(priv, SIC2, SIC2_SICS0);
		pef2256_s8(priv, SIC2, SIC2_SICS1);
		pef2256_s8(priv, SIC2, SIC2_SICS2);
	}

	if (priv->mode == SLAVE_MODE)
		/* transmit buffer size = 2 frames */
		pef2256_s8(priv, SIC1, SIC1_XBS1);

	/* transmit in multiframe */
	pef2256_s8(priv, FMR1, FMR1_XFS);
	/* receive in multiframe */
	pef2256_s8(priv, FMR2, FMR2_RFS1);
	/* Automatic transmission of submultiframe status */
	pef2256_s8(priv, XSP, XSP_AXS);

	/* error counter mode toutes les 1s */
	pef2256_s8(priv, FMR1, FMR1_ECM);
	/* error counter mode COFA => GCR.ECMC = 1 (bit 4) */
	pef2256_s8(priv, GCR, GCR_ECMC);
	/* errors in service words with no influence */
	pef2256_s8(priv, RC0, RC0_SWD);
	/* 4 consecutive incorrect FAS = loss of sync */
	pef2256_s8(priv, RC0, RC0_ASY4);
	/* Si-Bit in service word from XDI */
	pef2256_s8(priv, XSW, XSW_XSIS);
	/* Si-Bit in FAS word from XDI */
	pef2256_s8(priv, XSP, XSP_XSIF);

	/* port RCLK is output */
	pef2256_s8(priv, PC5, PC5_CRP);
	/* status changed interrupt at both up and down */
	pef2256_s8(priv, GCR, GCR_SCI);
	/* visibility of the masked interrupts */
	pef2256_s8(priv, GCR, GCR_VIS);
	/* reset lines
	 *  => CMDR.RRES = 1 (bit 6); CMDR.XRES = 1 (bit 4);
	 *     CMDR.SRES = 1 (bit 0)
	 */
	pef2256_w8(priv, CMDR, 0x51);
}

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

	if (ret < 0) {
		netdev_info(ndev, "Invalid mode (0 or 1 expected\n");
	} else {
		priv->mode = value;
		if (reconfigure && netif_carrier_ok(ndev))
			init_falc(priv);
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

	if (ret < 0) {
		netdev_info(ndev, "Invalid Tx_TS (hex number > 0 and < 0x80000000 expected\n");
	} else {
		priv->Tx_TS = value;
		if (reconfigure && netif_carrier_ok(ndev))
			config_hdlc(priv);
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

	if (ret < 0) {
		netdev_info(ndev, "Invalid Rx_TS (hex number > 0 and < 0x80000000 expected\n");
	} else {
		priv->Rx_TS = value;
		if (reconfigure && netif_carrier_ok(ndev))
			config_hdlc(priv);
	}

	return strnlen(buf, count);
}

static DEVICE_ATTR(Rx_TS, S_IRUGO | S_IWUSR, fs_attr_Rx_TS_show,
	 fs_attr_Rx_TS_store);


static void pef2256_fifo_ack(struct pef2256_dev_priv *priv)
{
	pef2256_s8(priv, CMDR, 1 << 7);
}


static void pef2256_rx(struct pef2256_dev_priv *priv, u8 isr0)
{
	int idx;

	/* RDO has been received -> wait for RME */
	if (priv->stats.rx_bytes == -1) {
		pef2256_fifo_ack(priv);

		if (isr0 & ISR0_RME)
			priv->stats.rx_bytes = 0;

		return;
	}

	/* RPF : a block is available in the receive FIFO */
	if (isr0 & ISR0_RPF) {
		for (idx = 0; idx < 16; idx++)
			priv->rx_buff[(priv->stats.rx_bytes)/2 + idx] =
				pef2256_r16(priv, RFIFO);

		pef2256_fifo_ack(priv);

		priv->stats.rx_bytes += 32;
	}

	/* RME : Message end : Read the receive FIFO */
	if (isr0 & ISR0_RME) {
		/* Get size of last block */
		int size = pef2256_r8(priv, RBCL) & 0x1F;

		/* Read last block */
		for (idx = 0; idx < (size+1)/2; idx++)
			priv->rx_buff[(priv->stats.rx_bytes)/2 + idx] =
				pef2256_r16(priv, RFIFO);

		pef2256_fifo_ack(priv);

		priv->stats.rx_bytes += size;

		/* Packet received */
		if (priv->stats.rx_bytes > 0) {
			struct sk_buff *skb =
				dev_alloc_skb(priv->stats.rx_bytes);

			if (!skb) {
				priv->stats.rx_bytes = 0;
				priv->netdev->stats.rx_dropped++;
				return;
			}
			memcpy(skb->data, priv->rx_buff, priv->stats.rx_bytes);
			skb_put(skb, priv->stats.rx_bytes);
			priv->stats.rx_bytes = 0;
			skb->protocol = hdlc_type_trans(skb, priv->netdev);
			priv->netdev->stats.rx_packets++;
			priv->netdev->stats.rx_bytes += skb->len;
			netif_rx(skb);
		}
	}
}


static void pef2256_tx(struct pef2256_dev_priv *priv, u8 isr1)
{
	int idx, size;
	u16 *tx_buff = (u16*)priv->tx_skb->data;

	/* ALLS : transmit all done */
	if (isr1 & ISR1_ALLS) {
		priv->netdev->stats.tx_packets++;
		priv->netdev->stats.tx_bytes += priv->tx_skb->len;
		dev_kfree_skb_irq(priv->tx_skb);
		priv->tx_skb = NULL;
		priv->stats.tx_bytes = 0;
		netif_wake_queue(priv->netdev);
	} else
		/* XPR : write a new block in transmit FIFO */
		if (priv->stats.tx_bytes < priv->tx_skb->len) {
			size = priv->tx_skb->len - priv->stats.tx_bytes;
			if (size > 32)
				size = 32;

			for (idx = 0; (idx < size + 1 / 2); idx++)
				pef2256_w16(priv, XFIFO,
					tx_buff[priv->stats.tx_bytes /2 + idx]);

			priv->stats.tx_bytes += size;

			if (priv->stats.tx_bytes == priv->tx_skb->len)
				pef2256_s8(priv, CMDR, (1 << 3) | (1 << 1));
			else
				pef2256_s8(priv, CMDR, 1 << 3);
		}
}

static void pef2256_errors(struct pef2256_dev_priv *priv)
{
	if (pef2256_r8(priv, FRS1) & FRS1_PDEN ||
	    pef2256_r8(priv, FRS0) & (FRS0_LOS | FRS0_AIS)) {
		if (priv->tx_skb) {
			priv->netdev->stats.tx_errors++;
			dev_kfree_skb_irq(priv->tx_skb);
			priv->tx_skb = NULL;
			priv->stats.tx_bytes = 0;
			netif_wake_queue(priv->netdev);
		}
		if (priv->stats.rx_bytes > 0) {
			priv->netdev->stats.rx_errors++;
			priv->stats.rx_bytes = 0;
		}
		netif_carrier_off(priv->netdev);
	} else
		netif_carrier_on(priv->netdev);
}

static irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	struct pef2256_dev_priv *priv = (struct pef2256_dev_priv *)dev_priv;
	u8 r_gis;
	u8 isr0, isr1, isr2;

	r_gis = pef2256_r8(priv, GIS);

	/* We only care about ISR0, ISR1 and ISR2 */
	/* ISR0 */
	if (r_gis & GIS_ISR0)
		isr0 = pef2256_r8(priv, ISR0) & ~(pef2256_r8(priv, IMR0));
	else
		isr0 = 0;

	/* ISR1 */
	if (r_gis & GIS_ISR1)
		isr1 = pef2256_r8(priv, ISR1) & ~(pef2256_r8(priv, IMR1));
	else
		isr1 = 0;

	/* ISR2 */
	if (r_gis & GIS_ISR2)
		isr2 = pef2256_r8(priv, ISR2) & ~(pef2256_r8(priv, IMR2));
	else
		isr2 = 0;

	if (r_gis & GIS_ISR3)
		pef2256_r8(priv, ISR3);
	if (r_gis & GIS_ISR4)
		pef2256_r8(priv, ISR4);
	if (r_gis & GIS_ISR5)
		pef2256_r8(priv, ISR5);

	/* An error status has changed */
	if (isr0 & ISR0_PDEN || isr2 & ISR2_LOS || isr2 & ISR2_AIS)
		pef2256_errors(priv);

	/* RDO : Receive data overflow -> RX error */
	if (isr1 & ISR1_RDO) {
		pef2256_fifo_ack(priv);
		netdev_err(priv->netdev, "Receive data overflow\n");
		priv->netdev->stats.rx_errors++;
		/* RME received ? */
		if (isr0 & ISR0_RME)
			priv->stats.rx_bytes = 0;
		else
			priv->stats.rx_bytes = -1;
	} else {
		/* RPF or RME : FIFO received */
		if (isr0 & (ISR0_RPF | ISR0_RME))
			pef2256_rx(priv, isr0);
	}

	/* XDU : Transmit data underrun -> TX error */
	if (isr1 & ISR1_XDU) {
		netdev_err(priv->netdev, "Transmit data underrun\n");
		priv->netdev->stats.tx_errors++;
		dev_kfree_skb_irq(priv->tx_skb);
		priv->tx_skb = NULL;
	} else {
		/* XPR or ALLS : FIFO sent */
		if (isr1 & (ISR1_XPR | ISR1_ALLS)) {
			if (priv->tx_skb)
				pef2256_tx(priv, isr1);
			else
				netif_wake_queue(priv->netdev);
		}
	}

	return IRQ_HANDLED;
}


static int pef2256_open(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	int ret;
	u8 dummy;

	if (priv->component_id == VERSION_UNDEF) {
		dev_err(priv->dev, "Composant ident (%X/%X) = %d\n",
			pef2256_r8(priv, VSTR), pef2256_r8(priv, WID),
			priv->component_id);
		return -ENODEV;
	}

	ret = hdlc_open(netdev);
	if (ret)
		return ret;

	/* We mask HDLC 1 receive/transmit IT to prevent the component sending
	 * such interrupts before it is initialized and configured.
	*/

	/* Read to remove pending IT */
	dummy = pef2256_r8(priv, ISR0);
	dummy = pef2256_r8(priv, ISR1);
	dummy = pef2256_r8(priv, ISR2);

	/* Mask HDLC 1 Transmit IT */
	pef2256_s8(priv, IMR1, IMR1_XPR);
	pef2256_s8(priv, IMR1, IMR1_XDU);
	pef2256_s8(priv, IMR1, IMR1_ALLS);

	/* Mask HDLC 1 Receive IT */
	pef2256_s8(priv, IMR0, IMR0_RPF);
	pef2256_s8(priv, IMR0, IMR0_RME);
	pef2256_s8(priv, IMR1, IMR1_RDO);

	/* Mask errors IT */
	pef2256_s8(priv, IMR0, IMR0_PDEN);
	pef2256_s8(priv, IMR2, IMR2_LOS);
	pef2256_s8(priv, IMR2, IMR2_AIS);

	ret = request_irq(priv->irq, pef2256_irq, 0, "e1-wan", priv);
	if (ret) {
		dev_err(priv->dev, "Cannot request irq. Device seems busy.\n");
		hdlc_close(netdev);
		return -EBUSY;
	}

	init_falc(priv);

	priv->tx_skb = NULL;
	priv->stats.rx_bytes = 0;

	config_hdlc(priv);

	netif_start_queue(netdev);
	pef2256_errors(priv);

	return 0;
}


static int pef2256_close(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	netif_stop_queue(netdev);
	netif_carrier_off(netdev);
	hdlc_close(netdev);
	free_irq(priv->irq, priv);

	return 0;
}



static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	int idx, size;
	u16 *tx_buff = (u16*)skb->data;

	priv->tx_skb = skb;
	priv->stats.tx_bytes = 0;

	size = priv->tx_skb->len - priv->stats.tx_bytes;
	if (size > 32)
		size = 32;

	for (idx = 0; idx < (size + 1) / 2; idx++)
		pef2256_w16(priv, XFIFO,
			   tx_buff[priv->stats.tx_bytes/2 + idx]);

	priv->stats.tx_bytes += size;

	pef2256_s8(priv, CMDR, 1 << 3);
	if (priv->stats.tx_bytes == priv->tx_skb->len)
		pef2256_s8(priv, CMDR, 1 << 1);

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


static int pef2256_probe(struct platform_device *pdev)
{
	struct pef2256_dev_priv *priv;
	int ret = -ENOMEM;
	struct net_device *netdev;
	hdlc_device *hdlc;
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
		dev_err(&pdev->dev,
"failed to read rising edge sync pulse -> using \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else if (strcmp(str_data, "transmit") &&
		   strcmp(str_data, "receive")) {
		dev_err(&pdev->dev,
"invalid rising edge sync pulse \"%s\" -> using \"transmit\"\n", str_data);
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else {
		strncpy(priv->rising_edge_sync_pulse, str_data, 10);
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (!priv->irq) {
		dev_err(priv->dev, "no irq defined\n");
		goto free_priv;
	}

	priv->ioaddr = of_iomap(np, 0);
	if (!priv->ioaddr) {
		dev_err(&pdev->dev, "of_iomap failed\n");
		goto free_priv;
	}

	/* Get the component Id */
	priv->component_id = VERSION_UNDEF;
	if (pef2256_r8(priv, VSTR) == 0x00) {
		if ((pef2256_r8(priv, WID) & WID_IDENT_1) == WID_IDENT_1_2)
			priv->component_id = VERSION_1_2;
	} else if (pef2256_r8(priv, VSTR) == 0x05) {
		if ((pef2256_r8(priv, WID) & WID_IDENT_2) == WID_IDENT_2_1)
			priv->component_id = VERSION_2_1;
		else if ((pef2256_r8(priv, WID) & WID_IDENT_2) == WID_IDENT_2_2)
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

	/* These files are required to configure HDLC : mode
	 * (master or slave), time slots used to transmit and
	 * receive data. They are mandatory.
	 */
	ret = device_create_file(priv->dev, &dev_attr_mode);
	ret |= device_create_file(priv->dev, &dev_attr_Tx_TS);
	ret |= device_create_file(priv->dev, &dev_attr_Rx_TS);

	if (ret)
		goto remove_files;

	return 0;

remove_files:
	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);
free_dev:
	free_netdev(priv->netdev);
free_regs:
	iounmap(priv->ioaddr);
free_priv:;
	kfree(priv);

	return ret;
}


static int pef2256_remove(struct platform_device *pdev)
{
	struct net_device *ndev = dev_get_drvdata(&pdev->dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);

	free_netdev(priv->netdev);

	iounmap(priv->ioaddr);

	kfree(priv);

	kfree(pdev);
	return 0;
}

static const struct of_device_id pef2256_match[] = {
	{
		.compatible = "lantiq,pef2256",
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
MODULE_DESCRIPTION("Lantiq PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
