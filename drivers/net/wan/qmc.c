/* drivers/net/wan/qmc.c : a QMC HDLC on PEF2256 driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/netdevice.h>
#include <linux/hdlc.h>
#include <saf3000/saf3000.h>
#include "qmc.h"

#define MTU_MAX	(HDLC_MAX_MTU + 4) /* space for CRC and status */

static struct pef2256_dev_priv *priv = NULL;

/* helper function - Read a register */
static u8 pef2256_r8(u32 offset)
{
	return ioread8(priv->ioaddr + offset);
}

/* helper function - Write a value to a register */
static void pef2256_w8(u32 offset, u8 val)
{
	iowrite8(val, priv->ioaddr + offset);
}

/* helper function - Clear bits in a register */
static void pef2256_c8(u32 offset, u8 mask)
{
	u8 val = pef2256_r8(offset);
	iowrite8(val & ~mask, priv->ioaddr + offset);
}

/* helper function - Set bits in a register */
static void pef2256_s8(u32 offset, u8 mask)
{
	u8 val = pef2256_r8(offset);
	iowrite8(val | mask, priv->ioaddr + offset);
}

static void init_falc(void)
{
	int version;

	/* Get controller version */
	version = priv->component_id;

	/* Init FALC56 */
	/* RCLK output : DPLL clock, DCO-X enabled, DCO-X internal reference
	 * clock
	 */
	pef2256_w8(CMR1, 0x00);
	/* SCLKR selected, SCLKX selected, receive synchro pulse sourced by
	 * SYPR, transmit synchro pulse sourced by SYPX
	 */
	pef2256_w8(CMR2, 0x00);
	/* NRZ coding, no alarm simulation */
	pef2256_w8(FMR0, 0x00);
	/* E1 double frame format, 2 Mbit/s system data rate, no AIS
	 * transmission to remote end or system interface, payload loop
	 * off, transmit remote alarm on
	 */
	pef2256_w8(FMR1, 0x00);
	pef2256_w8(FMR2, 0x02);
	/* E1 default for LIM2 */
	pef2256_w8(LIM2, 0x20);
	if (priv->mode == MASTER_MODE)
		/* SEC input, active high */
		pef2256_w8(GPC1, 0x00);
	else
		/* FSC output, active high */
		pef2256_w8(GPC1, 0x40);
	/* internal second timer, power on */
	pef2256_w8(GCR, 0x00);
	/* slave mode, local loop off, mode short-haul */
	if (version == VERSION_1_2)
		pef2256_w8(LIM0, 0x00);
	else
		pef2256_w8(LIM0, 0x08);
	/* analog interface selected, remote loop off */
	pef2256_w8(LIM1, 0x00);
	if (version == VERSION_1_2) {
		/* function of ports RP(A to D) : output receive sync pulse
		 * function of ports XP(A to D) : output transmit line clock
		 */
		pef2256_w8(PC1, 0x77);
		pef2256_w8(PC2, 0x77);
		pef2256_w8(PC3, 0x77);
		pef2256_w8(PC4, 0x77);
	} else {
		/* function of ports RP(A to D) : output high
		 * function of ports XP(A to D) : output high
		 */
		pef2256_w8(PC1, 0xAA);
		pef2256_w8(PC2, 0xAA);
		pef2256_w8(PC3, 0xAA);
		pef2256_w8(PC4, 0xAA);
	}
	/* function of port RPA : input SYPR
	 * function of port XPA : input SYPX
	 */
	pef2256_w8(PC1, 0x00);
	/* SCLKR, SCLKX, RCLK configured to inputs,
	 * XFMS active low, CLK1 and CLK2 pin configuration
	 */
	pef2256_w8(PC5, 0x00);
	pef2256_w8(PC6, 0x00);
	/* the receive clock offset is cleared
	 * the receive time slot offset is cleared
	 */
	pef2256_w8(RC0, 0x00);
	pef2256_w8(RC1, 0x9C);
	/* 2.048 MHz system clocking rate, receive buffer 2 frames, transmit
	 * buffer bypass, data sampled and transmitted on the falling edge of
	 * SCLKR/X, automatic freeze signaling, data is active in the first
	 * channel phase
	 */
	pef2256_w8(SIC1, 0x00);
	pef2256_w8(SIC2, 0x00);
	pef2256_w8(SIC3, 0x00);
	/* channel loop-back and single frame mode are disabled */
	pef2256_w8(LOOP, 0x00);
	/* all bits of the transmitted service word are cleared */
	pef2256_w8(XSW, 0x1F);
	/* spare bit values are cleared */
	pef2256_w8(XSP, 0x00);
	/* no transparent mode active */
	pef2256_w8(TSWM, 0x00);
	/* the transmit clock offset is cleared
	 * the transmit time slot offset is cleared
	 */
	pef2256_w8(XC0, 0x00);
	pef2256_w8(XC1, 0x9C);
	/* transmitter in tristate mode */
	pef2256_w8(XPM2, 0x40);
	/* transmit pulse mask */
	if (version != VERSION_1_2)
		pef2256_w8(XPM0, 0x9C);

	if (version == VERSION_1_2) {
		/* master clock is 16,384 MHz (flexible master clock) */
		pef2256_w8(GCM2, 0x58);
		pef2256_w8(GCM3, 0xD2);
		pef2256_w8(GCM4, 0xC2);
		pef2256_w8(GCM5, 0x07);
		pef2256_w8(GCM6, 0x10);
	} else {
		/* master clock is 16,384 MHz (flexible master clock) */
		pef2256_w8(GCM2, 0x18);
		pef2256_w8(GCM3, 0xFB);
		pef2256_w8(GCM4, 0x0B);
		pef2256_w8(GCM5, 0x01);
		pef2256_w8(GCM6, 0x0B);
		pef2256_w8(GCM7, 0xDB);
		pef2256_w8(GCM8, 0xDF);
	}

	/* master mode */
	if (priv->mode == MASTER_MODE)
		pef2256_s8(LIM0, LIM0_MAS);

	/* transmit line in normal operation */
	pef2256_c8(XPM2, XPM2_XLT);

	if (version == VERSION_1_2) {
		/* receive input threshold = 0,21V */
		pef2256_s8(LIM1, LIM1_RIL0);
		pef2256_c8(LIM1, LIM1_RIL1);
		pef2256_s8(LIM1, LIM1_RIL2);
	} else {
		/* receive input threshold = 0,21V */
		pef2256_c8(LIM1, LIM1_RIL0);
		pef2256_c8(LIM1, LIM1_RIL1);
		pef2256_s8(LIM1, LIM1_RIL2);
	}
	/* transmit line coding = HDB3 */
	pef2256_s8(FMR0, FMR0_XC0);
	pef2256_s8(FMR0, FMR0_XC1);
	/* receive line coding = HDB3 */
	pef2256_s8(FMR0, FMR0_RC0);
	pef2256_s8(FMR0, FMR0_RC1);
	/* detection of LOS alarm = 176 pulses (soit (10 + 1) * 16) */
	pef2256_w8(PCD, 10);
	/* recovery of LOS alarm = 22 pulses (soit 21 + 1) */
	pef2256_w8(PCR, 21);
	/* DCO-X center frequency => CMR2.DCOXC */
	pef2256_s8(CMR2, CMR2_DCOXC);
	if (priv->mode == SLAVE_MODE) {
		/* select RCLK source = 2M */
		pef2256_c8(CMR1, CMR1_RS0);
		pef2256_s8(CMR1, CMR1_RS1);
		/* disable switching RCLK -> SYNC */
		pef2256_s8(CMR1, CMR1_DCS);
	}
	if (version != VERSION_1_2)
		/* during inactive channel phase RDO into tri-state mode */
		pef2256_s8(SIC3, SIC3_RTRI);
	if (!strcmp(priv->rising_edge_sync_pulse, "transmit")) {
		/* rising edge sync pulse transmit */
		pef2256_c8(SIC3, SIC3_RESR);
		pef2256_s8(SIC3, SIC3_RESX);
	} else {
		/* rising edge sync pulse receive */
		pef2256_c8(SIC3, SIC3_RESX);
		pef2256_s8(SIC3, SIC3_RESR);
	}
	/* transmit offset counter = 4
	 *  => XC0.XCO10:8 = 000 (bits 2, 1 et 0);
	 *     XC1.XCO7:0 = 4 (bits 7 ... 0)
	 */
	pef2256_w8(XC1, 4);
	/* receive offset counter = 4
	 * => RC0.RCO10:8 = 000 (bits 2, 1 et 0);
	 *    RC1.RCO7:0 = 4 (bits 7 ... 0)
	 */
	pef2256_w8(RC1, 4);

	/* Clocking rate for FALC56 */
	/* Nothing to do for clocking rate 2M  */
	/* clocking rate 4M  */
	if (priv->clock_rate == CLOCK_RATE_4M)
		pef2256_s8(SIC1, SIC1_SSC0);
	/* clocking rate 8M  */
	else if (priv->clock_rate == CLOCK_RATE_8M)
		pef2256_s8(SIC1, SIC1_SSC1);
	/* clocking rate 16M  */
	else if (priv->clock_rate == CLOCK_RATE_16M) {
		pef2256_s8(SIC1, SIC1_SSC0);
		pef2256_s8(SIC1, SIC1_SSC1);
	}

	/* data rate for FALC56 */
	/* Nothing to do for data rate 2M on the system data bus */
	/* data rate 4M on the system data bus */
	if (priv->data_rate == DATA_RATE_4M)
		pef2256_s8(FMR1, FMR1_SSD0);
	/* data rate 8M on the system data bus */
	if (priv->data_rate == DATA_RATE_8M)
		pef2256_s8(SIC1, SIC1_SSD1);
	/* data rate 16M on the system data bus */
	if (priv->data_rate == DATA_RATE_16M) {
		pef2256_s8(FMR1, FMR1_SSD0);
		pef2256_s8(SIC1, SIC1_SSD1);
	}

	/* channel phase for FALC56 */
	/* Nothing to do for channel phase 0 */
	if (priv->channel_phase == CHANNEL_PHASE_1)
		pef2256_s8(SIC2, SIC2_SICS0);
	else if (priv->channel_phase == CHANNEL_PHASE_2)
		pef2256_s8(SIC2, SIC2_SICS1);
	else if (priv->channel_phase == CHANNEL_PHASE_3) {
		pef2256_s8(SIC2, SIC2_SICS0);
		pef2256_s8(SIC2, SIC2_SICS1);
	}
	else if (priv->channel_phase == CHANNEL_PHASE_4)
		pef2256_s8(SIC2, SIC2_SICS2);
	else if (priv->channel_phase == CHANNEL_PHASE_5) {
		pef2256_s8(SIC2, SIC2_SICS0);
		pef2256_s8(SIC2, SIC2_SICS2);
	}
	else if (priv->channel_phase == CHANNEL_PHASE_6) {
		pef2256_s8(SIC2, SIC2_SICS1);
		pef2256_s8(SIC2, SIC2_SICS2);
	}
	else if (priv->channel_phase == CHANNEL_PHASE_7) {
		pef2256_s8(SIC2, SIC2_SICS0);
		pef2256_s8(SIC2, SIC2_SICS1);
		pef2256_s8(SIC2, SIC2_SICS2);
	}

	if (priv->mode == SLAVE_MODE)
		/* transmit buffer size = 2 frames */
		pef2256_s8(SIC1, SIC1_XBS1);

	/* error counter mode toutes les 1s */
	pef2256_s8(FMR1, FMR1_ECM);
	/* error counter mode COFA => GCR.ECMC = 1 (bit 4) */
	pef2256_s8(GCR, GCR_ECMC);
	/* errors in service words with no influence */
	pef2256_s8(RC0, RC0_SWD);
	/* 4 consecutive incorrect FAS = loss of sync */
	pef2256_s8(RC0, RC0_ASY4);
	/* Si-Bit in service word from XDI */
	pef2256_s8(XSW, XSW_XSIS);
	/* Si-Bit in FAS word from XDI */
	pef2256_s8(XSP, XSP_XSIF);

	/* port RCLK is output */
	pef2256_s8(PC5, PC5_CRP);
	/* status changed interrupt at both up and down */
	pef2256_s8(GCR, GCR_SCI);
	/* reset lines */
	pef2256_w8(CMDR, 0x51);
}

static ssize_t fs_attr_mode_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, (priv->mode == MASTER_MODE) ?
			"[master] slave\n" : "master [slave]\n");
}
static ssize_t fs_attr_mode_store(struct device *dev,
			struct device_attribute *attr,  const char *buf,
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	int mode = priv->mode;

	if (sysfs_streq(buf, "master"))
		priv->mode = MASTER_MODE;
	else if (sysfs_streq(buf, "slave"))
		priv->mode = SLAVE_MODE;
	else {
		netdev_info(ndev, "Invalid mode (master or slave expected)\n");
		return -EINVAL;
	}

	if ((mode != priv->mode) && netif_carrier_ok(ndev))
		init_falc();

	return strnlen(buf, count);
}
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, fs_attr_mode_show,
						fs_attr_mode_store);

static ssize_t fs_attr_hdlc_TS_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%08x\n", priv->hdlc_TS);
}
static ssize_t fs_attr_hdlc_TS_store(struct device *dev,
			struct device_attribute *attr,  const char *buf,
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	unsigned long value;
	int ret = kstrtoul(buf, 16, (long int *)&value);
	int reconfigure = (value != priv->hdlc_TS);

	/* TS 0 is reserved */
	if ((ret < 0) || (value < TS_1) || (value > TS_ALL))
		ret = -EINVAL;

	if (ret < 0) {
		netdev_info(ndev,
			"Invalid TS (hex number > 0 and < 0x80000000)\n");
	} else {
		priv->hdlc_TS = value;
		if (reconfigure && netif_carrier_ok(ndev))
			ret = qmc_canal_hdlc(0, priv->hdlc_TS, priv->parity);
		if (ret >= 0)
			ret = strnlen(buf, count);
	}

	return ret;
}
static DEVICE_ATTR(hdlc_TS, S_IRUGO | S_IWUSR, fs_attr_hdlc_TS_show,
						fs_attr_hdlc_TS_store);

static void pef2256_errors(void)
{
	if (/*pef2256_r8(FRS1) & FRS1_PDEN ||*/
	    pef2256_r8(FRS0) & (FRS0_LOS | FRS0_AIS))
		netif_carrier_off(priv->netdev);
	else
		netif_carrier_on(priv->netdev);
}

void wan_qmc_hdlc_tx(int canal)
{
	struct sk_buff *skb = *priv->tx_cur;

	spin_lock(&priv->tx_lock);

	priv->netdev->stats.tx_packets++;
	priv->netdev->stats.tx_bytes += skb->len;
	dev_kfree_skb_irq(skb);

	if (++priv->tx_cur == priv->tx_skb_pool + NB_TX)
		priv->tx_cur = priv->tx_skb_pool;
	priv->nb_free++;

	if (priv->do_wake) {
		netif_wake_queue(priv->netdev);
		priv->do_wake = 0;
	}

	spin_unlock(&priv->tx_lock);
}
EXPORT_SYMBOL(wan_qmc_hdlc_tx);

void wan_qmc_hdlc_rx(int canal, int size)
{
	int i;
	struct sk_buff *skb = priv->rx_skb[priv->rx_cur];

	if ((priv->nb_ready <= 1) || (size <= 3) || (size > MTU_MAX)) {
		/* trame rejetee -> on conserve le skb */
		priv->netdev->stats.rx_dropped++;
		qmc_enreg_desc(0, DIR_RX, 0, skb->data);
		priv->rx_skb[priv->rx_cur] = skb;
	} else {
		skb_put(skb, size);
		skb->protocol = hdlc_type_trans(skb, priv->netdev);
		priv->netdev->stats.rx_packets++;
		priv->netdev->stats.rx_bytes += size;
		netif_rx(skb);
		priv->nb_ready--;
		if (++priv->rx_cur == NB_RX)
			priv->rx_cur = 0;
	}

	for (i = priv->nb_ready; i < NB_RX; i++) {
		skb = dev_alloc_skb(MTU_MAX);
		if (unlikely(skb == NULL))
			break;
		priv->rx_skb[priv->rx_next] = skb;
		qmc_enreg_desc(0, DIR_RX, 0, skb->data);
		priv->nb_ready++;
		if (++priv->rx_next == NB_RX)
			priv->rx_next = 0;
	}
}
EXPORT_SYMBOL(wan_qmc_hdlc_rx);

static irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	u8 r_gis;

	r_gis = pef2256_r8(GIS);

	while (r_gis & GIS_INT_ALL) {
		/* We only care about ISR2 */
		if (r_gis & GIS_INT2) {
			u8 isr2 = pef2256_r8(ISR2);

			/* An error status has changed */
			if (isr2 & INT2_LOS || isr2 & INT2_AIS)
				pef2256_errors();
		}

		r_gis = pef2256_r8(GIS);
	}

	return IRQ_HANDLED;
}

static int pef2256_open(struct net_device *netdev)
{
	int ret, i;
	u8 dummy;

	ret = hdlc_open(netdev);
	if (ret)
		return ret;

	ret = request_irq(priv->irq, pef2256_irq, 0, "e1-wan", priv);
	if (ret) {
		dev_err(priv->dev, "Cannot request irq. Device seems busy.\n");
		hdlc_close(netdev);
		return -EBUSY;
	}

	init_falc();

	/* Mask all ITs */
	pef2256_w8(IMR0, 0xff);
	pef2256_w8(IMR1, 0xff);
	pef2256_w8(IMR2, 0xff);
	pef2256_w8(IMR3, 0xff);
	pef2256_w8(IMR4, 0xff);
	pef2256_w8(IMR5, 0xff);
	/* Read to remove pending IT */
	dummy = pef2256_r8(ISR0);
	dummy = pef2256_r8(ISR1);
	dummy = pef2256_r8(ISR2);
	dummy = pef2256_r8(ISR3);
	dummy = pef2256_r8(ISR4);
	dummy = pef2256_r8(ISR5);

	ret = qmc_canal_hdlc(0, priv->hdlc_TS, priv->parity);
	if (ret) {
		dev_err(priv->dev, "Cannot access QMC. Device seems busy\n");
		free_irq(priv->irq, priv);
		hdlc_close(netdev);
		return -EBUSY;
	}

	/* Unmask errors IT */
	pef2256_c8(IMR2, INT2_LOS);
	pef2256_c8(IMR2, INT2_AIS);

	priv->nb_free = NB_TX;
	priv->tx_cur = priv->tx_skb_pool;
	priv->tx_avail = priv->tx_skb_pool;

	for (i = 0, priv->nb_ready = 0; i < NB_RX; i++) {
		priv->rx_skb[i] = dev_alloc_skb(MTU_MAX);
		if (unlikely(priv->rx_skb[i] == NULL))
			break;
		qmc_enreg_desc(0, DIR_RX, 0, priv->rx_skb[i]->data);
		priv->nb_ready++;
	}
	if (!priv->nb_ready) {
		dev_err(priv->dev, "Cannot allocate skb\n");
		free_irq(priv->irq, priv);
		hdlc_close(netdev);
		return -ENOMEM;
	}
	priv->rx_cur = 0;
	if (priv->nb_ready == NB_RX)
		priv->rx_next = 0;
	else
		priv->rx_next = priv->nb_ready;

	netif_start_queue(netdev);
	pef2256_errors();

	return 0;
}

static int pef2256_close(struct net_device *netdev)
{
	netif_stop_queue(netdev);
	netif_carrier_off(netdev);
	hdlc_close(netdev);
	free_irq(priv->irq, priv);

	return 0;
}

static netdev_tx_t qmc_start_xmit(struct sk_buff *skb,
					struct net_device *netdev)
{
	unsigned long flags;

	spin_lock_irqsave(&priv->tx_lock, flags);

	if (unlikely(qmc_enreg_desc(0, DIR_TX, skb->len, skb->data))) {
		netif_stop_queue(netdev);
		spin_unlock_irqrestore(&priv->tx_lock, flags);
		priv->do_wake = 1;
		dev_warn(priv->dev, "tx queue full!.\n");
		return NETDEV_TX_BUSY;
	}

	*priv->tx_avail = skb;

	if (++priv->tx_avail == priv->tx_skb_pool + NB_TX)
		priv->tx_avail = priv->tx_skb_pool;

	if (--priv->nb_free == 0) {
		netif_stop_queue(netdev);
		priv->do_wake = 1;
	}

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return NETDEV_TX_OK;
}

static int qmc_hdlc_attach(struct net_device *netdev,
				unsigned short encoding, unsigned short parity)
{
	unsigned short crc = CRC_16;

	if (encoding != ENCODING_NRZ)
		return -EINVAL;

	if ((parity != PARITY_CRC16_PR1_CCITT)
	    && (parity != PARITY_CRC32_PR1_CCITT))
		return -EINVAL;

	if (parity == PARITY_CRC32_PR1_CCITT)
		crc = CRC_32;
	/* changement type CRC => MaJ registres QMC */
	if ((priv->parity != crc) && priv->hdlc_TS)
		qmc_canal_hdlc(0, priv->hdlc_TS, crc);

	priv->encoding = encoding;
	priv->parity = crc;
	return 0;
}

static const struct net_device_ops pef2256_ops = {
	.ndo_open       = pef2256_open,
	.ndo_stop       = pef2256_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = hdlc_ioctl,
};

static int pef2256_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const char *data;
	struct resource *res;
	struct net_device *netdev;
	hdlc_device *hdlc;

	if (!pdev->dev.of_node)
		return -EINVAL;

	dev_err(dev, "Found PEF2256\n");

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return ret;

	priv->dev = dev;

	if (of_property_read_u32(np, "clock-rate", &priv->clock_rate)) {
		dev_err(dev, "default clock-rate : 8Mhz\n");
		priv->clock_rate = CLOCK_RATE_8M;
	}

	if (of_property_read_u32(np, "data-rate", &priv->data_rate)) {
		dev_err(dev, "default data-rate : 8Mb\n");
		priv->data_rate = DATA_RATE_8M;
	}

	if (of_property_read_u32(np, "channel-phase", &priv->channel_phase)) {
		dev_err(dev, "default channel-phase : 0\n");
		priv->channel_phase = CHANNEL_PHASE_0;
	}

	if (of_property_read_string(np, "rising-edge-sync-pulse", &data)) {
		dev_err(dev, "default rising-edge-sync-pulse : \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else if (strcmp(data, "transmit") && strcmp(data, "receive")) {
		dev_err(dev, "invalid rising-edge-sync-pulse \"%s\"\n", data);
		dev_err(dev, "default rising-edge-sync-pulse : \"transmit\"\n");
		strcpy(priv->rising_edge_sync_pulse, "transmit");
	} else {
		strncpy(priv->rising_edge_sync_pulse, data, 10);
	}

	priv->irq = platform_get_irq(pdev, 0);
	if (!priv->irq) {
		dev_err(dev, "no irq defined\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->ioaddr = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(priv->ioaddr)) {
		dev_err(dev, "devm_ioremap failed\n");
		return ret;
	}

	/* Get the component Id */
	priv->component_id = VERSION_UNDEF;
	if (pef2256_r8(VSTR) == 0x00) {
		if ((pef2256_r8(WID) & WID_IDENT_1) == WID_IDENT_1_2)
			priv->component_id = VERSION_1_2;
	} else if (pef2256_r8(VSTR) == 0x05) {
		if ((pef2256_r8(WID) & WID_IDENT_2) == WID_IDENT_2_1)
			priv->component_id = VERSION_2_1;
		else if ((pef2256_r8(WID) & WID_IDENT_2) == WID_IDENT_2_2)
			priv->component_id = VERSION_2_2;
	}

	/* Default settings ; Rx and Tx use TS 1, mode = MASTER */
	priv->hdlc_TS = TS_1;

	netdev = alloc_hdlcdev(priv);
	if (!netdev) {
		dev_err(dev, "alloc_hdlcdev failed\n");
		return ret;
	}

	priv->netdev = netdev;
	hdlc = dev_to_hdlc(netdev);
	netdev->netdev_ops = &pef2256_ops;
	SET_NETDEV_DEV(netdev, dev);
	hdlc->attach = qmc_hdlc_attach;
	hdlc->xmit = qmc_start_xmit;

	dev_set_drvdata(dev, netdev);

	ret = register_hdlc_device(netdev);
	if (ret < 0) {
		dev_err(dev, "Can't register hdlc device\n");
		goto free_dev;
	}

	/* These files are required to configure HDLC : mode
	 * (master or slave), time slots used to transmit and
	 * receive data. They are mandatory.
	 */
	ret = device_create_file(priv->dev, &dev_attr_mode);
	ret |= device_create_file(priv->dev, &dev_attr_hdlc_TS);

	if (ret)
		goto remove_files;

	spin_lock_init(&priv->tx_lock);

	return 0;

remove_files:
	device_remove_file(priv->dev, &dev_attr_hdlc_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);
free_dev:
	free_netdev(priv->netdev);

	return ret;
}

static int pef2256_remove(struct platform_device *pdev)
{
	device_remove_file(priv->dev, &dev_attr_hdlc_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);

	free_netdev(priv->netdev);

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
MODULE_AUTHOR("VASSEUR Patrick - June 2016");
MODULE_VERSION("0.1");
MODULE_DESCRIPTION("HDLC via QMC on Lantiq PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
