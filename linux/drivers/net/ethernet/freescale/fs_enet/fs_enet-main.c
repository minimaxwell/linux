/*
 * Combined Ethernet driver for Motorola MPC8xx and MPC82xx.
 *
 * Copyright (c) 2003 Intracom S.A.
 *  by Pantelis Antoniou <panto@intracom.gr>
 *
 * 2005 (c) MontaVista Software, Inc.
 * Vitaly Bordug <vbordug@ru.mvista.com>
 *
 * Heavily based on original FEC driver by Dan Malek <dan@embeddededge.com>
 * and modifications by Joakim Tjernlund <joakim.tjernlund@lumentis.se>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_net.h>

#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <ldb/ldb_gpio.h>

#include <net/arp.h>

#include "fs_enet.h"

#define PHY_ISOLATE 1
#define PHY_POWER_DOWN 2

/*************************************************/

MODULE_AUTHOR("Pantelis Antoniou <panto@intracom.gr>");
MODULE_DESCRIPTION("Freescale Ethernet Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);

/**  
 *  double_attachement_state is not handled properly by kernel
 *  and PHY0/1 acting together (during auto-negotiation) will 
 *  set system in unstable state (where both PHY are active at the
 *  same time and therefor, packets are duplicated.
 *  
 *  we use aneg_counter and aneg_status to identify differents cases 
 *  (PHY0 connected & PHY1 not connected, PHY1 connected & PHY0 not
 *  connected, both connected ...) and we "reset" the right PHY according 
 *  to the situation in order to put system in proper state.
 */

static int aneg_counter = 0; 
static int aneg_status = 0; 

static int fs_enet_debug = -1; /* -1 == use FS_ENET_DEF_MSG_ENABLE as value */
module_param(fs_enet_debug, int, 0);
MODULE_PARM_DESC(fs_enet_debug,
		 "Freescale bitmapped debugging message enable value");

#ifdef CONFIG_NET_POLL_CONTROLLER
static void fs_enet_netpoll(struct net_device *dev);
#endif

#define LINK_MONITOR_RETRY (2*HZ)
#define MODE_MANU 1
#define MODE_AUTO 2

static void fs_set_multicast_list(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);

	(*fep->ops->set_multicast_list)(dev);
}

static void skb_align(struct sk_buff *skb, int align)
{
	int off = ((unsigned long)skb->data) & (align - 1);

	if (off)
		skb_reserve(skb, align - off);
}

/* NAPI receive function */
static int fs_enet_rx_napi(struct napi_struct *napi, int budget)
{
	struct fs_enet_private *fep = container_of(napi, struct fs_enet_private, napi);
	struct net_device *dev = fep->ndev;
	const struct fs_platform_info *fpi = fep->fpi;
	cbd_t __iomem *bdp;
	struct sk_buff *skb, *skbn, *skbt;
	int received = 0;
	u16 pkt_len, sc;
	int curidx;

	/*
	 * First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

	/* clear RX status bits for napi*/
	(*fep->ops->napi_clear_rx_event)(dev);

	while (((sc = CBDR_SC(bdp)) & BD_ENET_RX_EMPTY) == 0) {
		curidx = bdp - fep->rx_bd_base;

		/*
		 * Since we have allocated space to hold a complete frame,
		 * the last indicator should be set.
		 */
		if ((sc & BD_ENET_RX_LAST) == 0)
			dev_warn(fep->dev, "rcv is not +last\n");

		/*
		 * Check for errors.
		 */
		if (sc & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_CL |
			  BD_ENET_RX_NO | BD_ENET_RX_CR | BD_ENET_RX_OV)) {
			fep->stats.rx_errors++;
			/* Frame too long or too short. */
			if (sc & (BD_ENET_RX_LG | BD_ENET_RX_SH))
				fep->stats.rx_length_errors++;
			/* Frame alignment */
			if (sc & (BD_ENET_RX_NO | BD_ENET_RX_CL))
				fep->stats.rx_frame_errors++;
			/* CRC Error */
			if (sc & BD_ENET_RX_CR)
				fep->stats.rx_crc_errors++;
			/* FIFO overrun */
			if (sc & BD_ENET_RX_OV)
				fep->stats.rx_crc_errors++;

			skb = fep->rx_skbuff[curidx];

			dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
				DMA_FROM_DEVICE);

			skbn = skb;

		} else {
			skb = fep->rx_skbuff[curidx];

			dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
				DMA_FROM_DEVICE);

			/*
			 * Process the incoming frame.
			 */
			fep->stats.rx_packets++;
			pkt_len = CBDR_DATLEN(bdp) - 4;	/* remove CRC */
			fep->stats.rx_bytes += pkt_len + 4;

			if (pkt_len <= fpi->rx_copybreak) {
				/* +2 to make IP header L1 cache aligned */
				skbn = netdev_alloc_skb(dev, pkt_len + 2);
				if (skbn != NULL) {
					skb_reserve(skbn, 2);	/* align IP header */
					skb_copy_from_linear_data(skb,
						      skbn->data, pkt_len);
					/* swap */
					skbt = skb;
					skb = skbn;
					skbn = skbt;
				}
			} else {
				skbn = netdev_alloc_skb(dev, ENET_RX_FRSIZE);

				if (skbn)
					skb_align(skbn, ENET_RX_ALIGN);
			}

			if (skbn != NULL) {
				skb_put(skb, pkt_len);	/* Make room */
				skb->protocol = eth_type_trans(skb, dev);
				received++;
				netif_receive_skb(skb);
			} else {
				fep->stats.rx_dropped++;
				skbn = skb;
			}
		}

		fep->rx_skbuff[curidx] = skbn;
		CBDW_BUFADDR(bdp, dma_map_single(fep->dev, skbn->data,
			     L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
			     DMA_FROM_DEVICE));
		CBDW_DATLEN(bdp, 0);
		CBDW_SC(bdp, (sc & ~BD_ENET_RX_STATS) | BD_ENET_RX_EMPTY);

		/*
		 * Update BD pointer to next entry.
		 */
		if ((sc & BD_ENET_RX_WRAP) == 0)
			bdp++;
		else
			bdp = fep->rx_bd_base;

		(*fep->ops->rx_bd_done)(dev);

		if (received >= budget)
			break;
	}

	fep->cur_rx = bdp;

	if (received < budget) {
		/* done */
		napi_complete(napi);
		(*fep->ops->napi_enable_rx)(dev);
	}
	return received;
}

/* non NAPI receive function */
static int fs_enet_rx_non_napi(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	const struct fs_platform_info *fpi = fep->fpi;
	cbd_t __iomem *bdp;
	struct sk_buff *skb, *skbn, *skbt;
	int received = 0;
	u16 pkt_len, sc;
	int curidx;
	/*
	 * First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

	while (((sc = CBDR_SC(bdp)) & BD_ENET_RX_EMPTY) == 0) {

		curidx = bdp - fep->rx_bd_base;

		/*
		 * Since we have allocated space to hold a complete frame,
		 * the last indicator should be set.
		 */
		if ((sc & BD_ENET_RX_LAST) == 0)
			dev_warn(fep->dev, "rcv is not +last\n");

		/*
		 * Check for errors.
		 */
		if (sc & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_CL |
			  BD_ENET_RX_NO | BD_ENET_RX_CR | BD_ENET_RX_OV)) {
			fep->stats.rx_errors++;
			/* Frame too long or too short. */
			if (sc & (BD_ENET_RX_LG | BD_ENET_RX_SH))
				fep->stats.rx_length_errors++;
			/* Frame alignment */
			if (sc & (BD_ENET_RX_NO | BD_ENET_RX_CL))
				fep->stats.rx_frame_errors++;
			/* CRC Error */
			if (sc & BD_ENET_RX_CR)
				fep->stats.rx_crc_errors++;
			/* FIFO overrun */
			if (sc & BD_ENET_RX_OV)
				fep->stats.rx_crc_errors++;

			skb = fep->rx_skbuff[curidx];

			dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
				DMA_FROM_DEVICE);

			skbn = skb;

		} else {

			skb = fep->rx_skbuff[curidx];

			dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
				DMA_FROM_DEVICE);

			/*
			 * Process the incoming frame.
			 */
			fep->stats.rx_packets++;
			pkt_len = CBDR_DATLEN(bdp) - 4;	/* remove CRC */
			fep->stats.rx_bytes += pkt_len + 4;

			if (pkt_len <= fpi->rx_copybreak) {
				/* +2 to make IP header L1 cache aligned */
				skbn = netdev_alloc_skb(dev, pkt_len + 2);
				if (skbn != NULL) {
					skb_reserve(skbn, 2);	/* align IP header */
					skb_copy_from_linear_data(skb,
						      skbn->data, pkt_len);
					/* swap */
					skbt = skb;
					skb = skbn;
					skbn = skbt;
				}
			} else {
				skbn = netdev_alloc_skb(dev, ENET_RX_FRSIZE);

				if (skbn)
					skb_align(skbn, ENET_RX_ALIGN);
			}

			if (skbn != NULL) {
				skb_put(skb, pkt_len);	/* Make room */
				skb->protocol = eth_type_trans(skb, dev);
				received++;
				netif_rx(skb);
			} else {
				fep->stats.rx_dropped++;
				skbn = skb;
			}
		}

		fep->rx_skbuff[curidx] = skbn;
		CBDW_BUFADDR(bdp, dma_map_single(fep->dev, skbn->data,
			     L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
			     DMA_FROM_DEVICE));
		CBDW_DATLEN(bdp, 0);
		CBDW_SC(bdp, (sc & ~BD_ENET_RX_STATS) | BD_ENET_RX_EMPTY);

		/*
		 * Update BD pointer to next entry.
		 */
		if ((sc & BD_ENET_RX_WRAP) == 0)
			bdp++;
		else
			bdp = fep->rx_bd_base;

		(*fep->ops->rx_bd_done)(dev);
	}

	fep->cur_rx = bdp;

	return 0;
}

static void fs_enet_tx(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	cbd_t __iomem *bdp;
	struct sk_buff *skb;
	int dirtyidx, do_wake, do_restart;
	u16 sc;

	spin_lock(&fep->tx_lock);
	bdp = fep->dirty_tx;

	do_wake = do_restart = 0;
	while (((sc = CBDR_SC(bdp)) & BD_ENET_TX_READY) == 0) {
		dirtyidx = bdp - fep->tx_bd_base;

		if (fep->tx_free == fep->tx_ring)
			break;

		skb = fep->tx_skbuff[dirtyidx];

		/*
		 * Check for errors.
		 */
		if (sc & (BD_ENET_TX_HB | BD_ENET_TX_LC |
			  BD_ENET_TX_RL | BD_ENET_TX_UN | BD_ENET_TX_CSL)) {

			if (sc & BD_ENET_TX_HB)	/* No heartbeat */
				fep->stats.tx_heartbeat_errors++;
			if (sc & BD_ENET_TX_LC)	/* Late collision */
				fep->stats.tx_window_errors++;
			if (sc & BD_ENET_TX_RL)	/* Retrans limit */
				fep->stats.tx_aborted_errors++;
			if (sc & BD_ENET_TX_UN)	/* Underrun */
				fep->stats.tx_fifo_errors++;
			if (sc & BD_ENET_TX_CSL)	/* Carrier lost */
				fep->stats.tx_carrier_errors++;

			if (sc & (BD_ENET_TX_LC | BD_ENET_TX_RL | BD_ENET_TX_UN)) {
				fep->stats.tx_errors++;
				do_restart = 1;
			}
		} else
			fep->stats.tx_packets++;

		if (sc & BD_ENET_TX_READY) {
			dev_warn(fep->dev,
				 "HEY! Enet xmit interrupt and TX_READY.\n");
		}

		/*
		 * Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (sc & BD_ENET_TX_DEF)
			fep->stats.collisions++;

		/* unmap */
		dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				skb->len, DMA_TO_DEVICE);

		/*
		 * Free the sk buffer associated with this last transmit.
		 */
		dev_kfree_skb_irq(skb);
		fep->tx_skbuff[dirtyidx] = NULL;

		/*
		 * Update pointer to next buffer descriptor to be transmitted.
		 */
		if ((sc & BD_ENET_TX_WRAP) == 0)
			bdp++;
		else
			bdp = fep->tx_bd_base;

		/*
		 * Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (!fep->tx_free++)
			do_wake = 1;
	}

	fep->dirty_tx = bdp;

	if (do_restart)
		(*fep->ops->tx_restart)(dev);

	spin_unlock(&fep->tx_lock);

	if (do_wake)
		netif_wake_queue(dev);
}

/*
 * The interrupt handler.
 * This is called from the MPC core interrupt.
 */
static irqreturn_t
fs_enet_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct fs_enet_private *fep;
	const struct fs_platform_info *fpi;
	u32 int_events;
	u32 int_clr_events;
	int nr, napi_ok;
	int handled;

	fep = netdev_priv(dev);
	fpi = fep->fpi;

	nr = 0;
	while ((int_events = (*fep->ops->get_int_events)(dev)) != 0) {
		nr++;

		int_clr_events = int_events;
		if (fpi->use_napi)
			int_clr_events &= ~fep->ev_napi_rx;

		(*fep->ops->clear_int_events)(dev, int_clr_events);

		if (int_events & fep->ev_err)
			(*fep->ops->ev_error)(dev, int_events);

		if (int_events & fep->ev_rx) {
			if (!fpi->use_napi)
				fs_enet_rx_non_napi(dev);
			else {
				napi_ok = napi_schedule_prep(&fep->napi);

				(*fep->ops->napi_disable_rx)(dev);
				(*fep->ops->clear_int_events)(dev, fep->ev_napi_rx);

				/* NOTE: it is possible for FCCs in NAPI mode    */
				/* to submit a spurious interrupt while in poll  */
				if (napi_ok)
					__napi_schedule(&fep->napi);
			}
		}

		if (int_events & fep->ev_tx)
			fs_enet_tx(dev);
	}

	handled = nr > 0;
	return IRQ_RETVAL(handled);
}

void fs_init_bds(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	cbd_t __iomem *bdp;
	struct sk_buff *skb;
	int i;

	fs_cleanup_bds(dev);

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->tx_free = fep->tx_ring;
	fep->cur_rx = fep->rx_bd_base;

	/*
	 * Initialize the receive buffer descriptors.
	 */
	for (i = 0, bdp = fep->rx_bd_base; i < fep->rx_ring; i++, bdp++) {
		skb = netdev_alloc_skb(dev, ENET_RX_FRSIZE);
		if (skb == NULL)
			break;

		skb_align(skb, ENET_RX_ALIGN);
		fep->rx_skbuff[i] = skb;
		CBDW_BUFADDR(bdp,
			dma_map_single(fep->dev, skb->data,
				L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
				DMA_FROM_DEVICE));
		CBDW_DATLEN(bdp, 0);	/* zero */
		CBDW_SC(bdp, BD_ENET_RX_EMPTY |
			((i < fep->rx_ring - 1) ? 0 : BD_SC_WRAP));
	}
	/*
	 * if we failed, fillup remainder
	 */
	for (; i < fep->rx_ring; i++, bdp++) {
		fep->rx_skbuff[i] = NULL;
		CBDW_SC(bdp, (i < fep->rx_ring - 1) ? 0 : BD_SC_WRAP);
	}

	/*
	 * ...and the same for transmit.
	 */
	for (i = 0, bdp = fep->tx_bd_base; i < fep->tx_ring; i++, bdp++) {
		fep->tx_skbuff[i] = NULL;
		CBDW_BUFADDR(bdp, 0);
		CBDW_DATLEN(bdp, 0);
		CBDW_SC(bdp, (i < fep->tx_ring - 1) ? 0 : BD_SC_WRAP);
	}
}

void fs_cleanup_bds(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	struct sk_buff *skb;
	cbd_t __iomem *bdp;
	int i;

	/*
	 * Reset SKB transmit buffers.
	 */
	for (i = 0, bdp = fep->tx_bd_base; i < fep->tx_ring; i++, bdp++) {
		if ((skb = fep->tx_skbuff[i]) == NULL)
			continue;

		/* unmap */
		dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
				skb->len, DMA_TO_DEVICE);

		fep->tx_skbuff[i] = NULL;
		dev_kfree_skb(skb);
	}

	/*
	 * Reset SKB receive buffers
	 */
	for (i = 0, bdp = fep->rx_bd_base; i < fep->rx_ring; i++, bdp++) {
		if ((skb = fep->rx_skbuff[i]) == NULL)
			continue;

		/* unmap */
		dma_unmap_single(fep->dev, CBDR_BUFADDR(bdp),
			L1_CACHE_ALIGN(PKT_MAXBUF_SIZE),
			DMA_FROM_DEVICE);

		fep->rx_skbuff[i] = NULL;

		dev_kfree_skb(skb);
	}
}

/**********************************************************************************/

#ifdef CONFIG_FS_ENET_MPC5121_FEC
/*
 * MPC5121 FEC requeries 4-byte alignment for TX data buffer!
 */
static struct sk_buff *tx_skb_align_workaround(struct net_device *dev,
					       struct sk_buff *skb)
{
	struct sk_buff *new_skb;

	/* Alloc new skb */
	new_skb = netdev_alloc_skb(dev, skb->len + 4);
	if (!new_skb)
		return NULL;

	/* Make sure new skb is properly aligned */
	skb_align(new_skb, 4);

	/* Copy data to new skb ... */
	skb_copy_from_linear_data(skb, new_skb->data, skb->len);
	skb_put(new_skb, skb->len);

	/* ... and free an old one */
	dev_kfree_skb_any(skb);

	return new_skb;
}
#endif

static int fs_enet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	cbd_t __iomem *bdp;
	int curidx;
	u16 sc;
	unsigned long flags;

#ifdef CONFIG_FS_ENET_MPC5121_FEC
	if (((unsigned long)skb->data) & 0x3) {
		skb = tx_skb_align_workaround(dev, skb);
		if (!skb) {
			/*
			 * We have lost packet due to memory allocation error
			 * in tx_skb_align_workaround(). Hopefully original
			 * skb is still valid, so try transmit it later.
			 */
			return NETDEV_TX_BUSY;
		}
	}
#endif
	spin_lock_irqsave(&fep->tx_lock, flags);

	/*
	 * Fill in a Tx ring entry
	 */
	bdp = fep->cur_tx;

	if (!fep->tx_free || (CBDR_SC(bdp) & BD_ENET_TX_READY)) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&fep->tx_lock, flags);

		/*
		 * Ooops.  All transmit buffers are full.  Bail out.
		 * This should not happen, since the tx queue should be stopped.
		 */
		dev_warn(fep->dev, "tx queue full!.\n");
		return NETDEV_TX_BUSY;
	}

	curidx = bdp - fep->tx_bd_base;
	/*
	 * Clear all of the status flags.
	 */
	CBDC_SC(bdp, BD_ENET_TX_STATS);

	/*
	 * Save skb pointer.
	 */
	fep->tx_skbuff[curidx] = skb;

	fep->stats.tx_bytes += skb->len;

	/*
	 * Push the data cache so the CPM does not get stale memory data.
	 */
	CBDW_BUFADDR(bdp, dma_map_single(fep->dev,
				skb->data, skb->len, DMA_TO_DEVICE));
	CBDW_DATLEN(bdp, skb->len);

	/*
	 * If this was the last BD in the ring, start at the beginning again.
	 */
	if ((CBDR_SC(bdp) & BD_ENET_TX_WRAP) == 0)
		fep->cur_tx++;
	else
		fep->cur_tx = fep->tx_bd_base;

	if (!--fep->tx_free)
		netif_stop_queue(dev);

	/* Trigger transmission start */
	sc = BD_ENET_TX_READY | BD_ENET_TX_INTR |
	     BD_ENET_TX_LAST | BD_ENET_TX_TC;

	/* note that while FEC does not have this bit
	 * it marks it as available for software use
	 * yay for hw reuse :) */
	if (skb->len <= 60)
		sc |= BD_ENET_TX_PAD;
	CBDS_SC(bdp, sc);

	skb_tx_timestamp(skb);

	(*fep->ops->tx_kickstart)(dev);

	spin_unlock_irqrestore(&fep->tx_lock, flags);

	return NETDEV_TX_OK;
}

static void fs_timeout(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	unsigned long flags;
	int wake = 0;

	fep->stats.tx_errors++;

	spin_lock_irqsave(&fep->lock, flags);

	if (dev->flags & IFF_UP) {
		phy_stop(fep->phydev);
		(*fep->ops->stop)(dev);
		(*fep->ops->restart)(dev);
		phy_start(fep->phydev);
	}

	phy_start(fep->phydev);
	wake = fep->tx_free && !(CBDR_SC(fep->cur_tx) & BD_ENET_TX_READY);
	spin_unlock_irqrestore(&fep->lock, flags);

	if (wake)
		netif_wake_queue(dev);
}

/*-----------------------------------------------------------------------------
 *  generic link-change handler - should be sufficient for most cases
 *-----------------------------------------------------------------------------*/
static void generic_adjust_link(struct  net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	struct phy_device *phydev = fep->phydev;
	int new_state = 0;

	if (phydev->link) {
		/* adjust to duplex mode */
		if (phydev->duplex != fep->oldduplex) {
			new_state = 1;
			fep->oldduplex = phydev->duplex;
		}

		if (phydev->speed != fep->oldspeed) {
			new_state = 1;
			fep->oldspeed = phydev->speed;
		}

		if (!fep->oldlink) {
			new_state = 1;
			fep->oldlink = 1;
		}

		if (new_state)
			fep->ops->restart(dev);
	}
	else if (fep->oldlink) {
		new_state = 1;
		fep->oldlink = 0;
		fep->oldspeed = 0;
		fep->oldduplex = -1;
	}

	if (new_state && netif_msg_link(fep))
		phy_print_status(phydev);
}


static void fs_adjust_link(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&fep->lock, flags);

	if(fep->ops->adjust_link)
		fep->ops->adjust_link(dev);
	else
		generic_adjust_link(dev);

	spin_unlock_irqrestore(&fep->lock, flags);
	
	cancel_delayed_work_sync(&fep->link_queue);
	if (fep->mode == MODE_AUTO) schedule_delayed_work(&fep->link_queue, 0);
}


void fs_sysfs_notify(struct work_struct *work)
{
	struct fs_notify_work *notify =
			container_of(work, struct fs_notify_work, notify_queue);

	sysfs_notify_dirent(notify->sd);
}


void fs_send_gratuitous_arp(struct work_struct *work)
{
	struct fs_enet_private *fep =
			container_of(work, struct fs_enet_private, arp_queue);
	__be32 ip_addr;
	struct sk_buff *skb;

	ip_addr = inet_select_addr(fep->ndev, 0, 0);
	skb = arp_create(ARPOP_REPLY, ETH_P_ARP, ip_addr, fep->ndev, ip_addr, NULL,
			fep->ndev->dev_addr, NULL);
	if (skb == NULL)
		printk("arp_create failure -> gratuitous arp not sent\n");
	else
		arp_xmit(skb);
}


void fs_link_switch(struct fs_enet_private *fep)
{
	struct phy_device *phydev = fep->phydev;
	unsigned long flags;
	int value;

	/* If the PHY must be powered down to disable it (mcr3000 1G) */
	if (fep->disable_phy == PHY_POWER_DOWN)
	{
		if (phydev->drv->suspend) phydev->drv->suspend(phydev);

		if (phydev == fep->phydevs[0]) {
			if (fep->phydevs[1]) {
				phydev = fep->phydevs[1];
				dev_err(fep->dev, "Switch to PHYB\n");
			}
		}
		else {
			phydev = fep->phydevs[0];
			dev_err(fep->dev, "Switch to PHYA\n");
		}

		if (fep->phydevs[1]) {
			/* Active phy has changed -> notify user space */
			schedule_work(&fep->notify_work[ACTIVE_LINK].notify_queue);
		}

		spin_lock_irqsave(&fep->lock, flags);
		fep->change_time = jiffies;
		fep->phydev = phydev;
		spin_unlock_irqrestore(&fep->lock, flags);

		if (phydev->drv->resume) phydev->drv->resume(phydev);

		return;
	}
	
	/* Do not suspend the PHY, but isolate it. We can't get a powered 
	   down PHY's link status */
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) | BMCR_ISOLATE));

	if (phydev == fep->phydevs[0]) {
		if (fep->phydevs[1]) {
			/* MCR3000_2G Front side eth connector */
			if (fep->use_PHY5) {
				/* Switch off PHY 1 */
				value = phy_read(phydev, MII_BMCR);
				phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
				/* Unisolate PHY 5 */
				phydev=fep->phydevs[1];
				value = phy_read(phydev, MII_BMCR);
				phy_write(phydev, MII_BMCR, 
					((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
			} 

			if (fep->gpio != -1) ldb_gpio_set_value(fep->gpio, 1);

			phydev = fep->phydevs[1];
			dev_err(fep->dev, "Switch to PHY B\n");
			/* In the open function, autoneg was disabled for PHY B.
			   It must be enabled when PHY B is activated for the 
			   first time. */
			phydev->autoneg = AUTONEG_ENABLE;
		}
	}
	else {
		/* MCR3000_2G Front side eth connector */
		if (fep->use_PHY5) {
			/* isolate PHY 5 */
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
			/* Switch on PHY 1 */
			phydev=fep->phydevs[0];
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, 
				((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
		}
		if (fep->gpio != -1) ldb_gpio_set_value(fep->gpio, 0);
		phydev = fep->phydevs[0];
		dev_err(fep->dev, "Switch to PHY A\n");
	}

	if (fep->phydevs[1]) {
		/* Active phy has changed -> notify user space */
		schedule_work(&fep->notify_work[ACTIVE_LINK].notify_queue);
	}

	spin_lock_irqsave(&fep->lock, flags);
	fep->phydev = phydev;
	fep->change_time = jiffies;
	spin_unlock_irqrestore(&fep->lock, flags);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
	if (phydev->drv->config_aneg)
		phydev->drv->config_aneg(phydev);

	if (fep->phydev->link)
	{
		netif_carrier_on(fep->phydev->attached_dev);

		/* Send gratuitous ARP */
		schedule_work(&fep->arp_queue);
	}
}

void autoneg_handler(struct work_struct *work) {
	struct delayed_work *dwork = to_delayed_work(work);
	struct fs_enet_private *fep =
			container_of(dwork, struct fs_enet_private, link_queue);
	struct phy_device *phydev = fep->phydev;
	int value;

	/* PHY 0 link becomes up */
	if (fep->phydevs[0]->link && fep->phydevs[0]->link != fep->phy_oldlinks[0]) {
		phydev = fep->phydevs[0];
		value = phy_read(phydev, MII_BMSR);
		if (! (value & BMSR_ANEGCOMPLETE))  {
			phydev->autoneg = AUTONEG_ENABLE;
			if (phydev->drv->config_aneg)
				phydev->drv->config_aneg(phydev);
			schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
		} 
	}

	/* PHY 1 link becomes up */
	if (fep->phydevs[1]->link && fep->phydevs[1]->link != fep->phy_oldlinks[1]) {
		phydev = fep->phydevs[1];
		value = phy_read(phydev, MII_BMSR);
		if (! (value & BMSR_ANEGCOMPLETE))  {
			phydev->autoneg = AUTONEG_ENABLE;
			if (phydev->drv->config_aneg)
				phydev->drv->config_aneg(phydev);
			schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
		}
	}
}


// #define DOUBLE_ATTACH_DEBUG 1

void fs_link_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct fs_enet_private *fep =
			container_of(dwork, struct fs_enet_private, link_queue);
	struct phy_device *phydev = fep->phydev;
	struct phy_device *changed_phydevs[2] = {NULL, NULL};
	int nb_changed_phydevs = 0;

	#ifdef DOUBLE_ATTACH_DEBUG
	int value;	
	printk("Current PHY : %s\n", fep->phydev==fep->phydevs[0] ? "PHY 0": "PHY 1");
	printk("PHY 0 (addr=%d) : %s ", fep->phydevs[0]->addr, fep->phydevs[0]->link ? "Up": "Down");
	value = phy_read(fep->phydevs[0], MII_BMSR);
	if (value & 0x0004)
		printk ("(Up in MII_BMSR) ");
	else
		printk ("(Down in MII_BMSR) ");
	value = phy_read(fep->phydevs[0], MII_BMCR);
	if (value & 0x0400) 
		printk ("isolated\n");
	else
		printk ("not isolated\n");
	
	if (fep->phydevs[1]) {
		printk("PHY 1 (addr=%d) : %s ", fep->phydevs[1]->addr, fep->phydevs[1]->link ? "Up": "Down");
		value = phy_read(fep->phydevs[1], MII_BMSR);
		if (value & 0x0004) 
			printk ("(Up in MII_BMSR) ");
		else
			printk ("(Down in MII_BMSR) ");
		value = phy_read(fep->phydevs[1], MII_BMCR);
		if (value & 0x0400) 
			printk ("isolated\n");
		else
			printk ("not isolated\n");
	}
	#endif

	/* If there's only one PHY -> Nothing to do */
	if (! fep->phydevs[1]) {
		if (!phydev->link && fep->mode == MODE_AUTO) 
			schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
		return;
	}

	/* If the PHY must be powered down to disable it (mcr3000 1G) */
	if (fep->disable_phy == PHY_POWER_DOWN)
	{
		if (!phydev->link && fep->mode == MODE_AUTO) {
			if (fep->phydevs[1] && 
			    (jiffies - fep->change_time >= LINK_MONITOR_RETRY))
				fs_link_switch(fep);
			schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
		}
		return;
	}

	/* If we are not in AUTO mode, don't do anything */
	if (fep->mode != MODE_AUTO) return;

	autoneg_handler(work);

	/**
	 *  Even if both PHY are connected at the same time when starting 
	 *  network, they are not set to link UP at the same time
	 *  we use aneg_counter here to identify if we are in the case
	 *  where both PHY are connected when we start or not. 
	 *  if yes aneg_status is left to 0, if not, it's set to 2
	 */
	if (!fep->phydevs[1]->link && fep->phydevs[0]->link && aneg_status == 0) {
		aneg_counter++;
		if (aneg_counter > 4) { aneg_status = 2; }
	}

	/* If elapsed time since last change is too small, wait for a while */
	if (jiffies - fep->change_time < LINK_MONITOR_RETRY) {
		schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
		return;
	}

	/* Which phydev(s) has changed ? */
	if (fep->phydevs[0]->link != fep->phy_oldlinks[0]) {
		changed_phydevs[nb_changed_phydevs++] = fep->phydevs[0];
		fep->phy_oldlinks[0] = fep->phydevs[0]->link;
		/* phyA link status has changed -> notify user space */
		schedule_work(&fep->notify_work[PHY0_LINK].notify_queue);
	} 
	if (fep->phydevs[1]->link != fep->phy_oldlinks[1]) {
		changed_phydevs[nb_changed_phydevs++] = fep->phydevs[1];
		fep->phy_oldlinks[1] = fep->phydevs[1]->link;
		/* phyB link status has changed -> notify user space */
		schedule_work(&fep->notify_work[PHY1_LINK].notify_queue);
	}

	/* If nothing has changed, exit */
	if (! nb_changed_phydevs) {
		/* Check the consistency of the carrier before exiting */
		if (fep->phydev->link && ! netif_carrier_ok(fep->phydev->attached_dev))
			netif_carrier_on(fep->phydev->attached_dev);
		return;
	}


	/* If both PHYs obtained a new link, PHY2 must become the active link */
	if (nb_changed_phydevs==2) {
		if (fep->phydev != fep->phydevs[0])
			fs_link_switch(fep);
		if (! netif_carrier_ok(fep->phydev->attached_dev))
			netif_carrier_on(fep->phydev->attached_dev);
		return;
	}
		

	/** Depending on aneg_status value, a different path of action is 
	 * choosen.
	 *
	 * 1 - aneg_status = 0, both links up.
	 * 	simple "reset" of both PHY with a switch to pu
	 * 	system in proper state
	 *
	 * 2 - aneg_status = 2, when second link (PHY1) become UP
	 * 	we can't "reset" from fs_link_monitor since, during 
	 *	our reset, there would be 1-2 seconds where packets
	 *	would be lost/or duplicated.
	 *	We have to handle part of the reset in phy_state_machine() 
	 *	function, and finish it here (with aneg_status set to 3 and 4)
	 *
	 *	/!\ in this case, it take time for system to return to a
	 *	clean state, but during this time, network should work fine
	 *	during our operation
	 *  
	 */
	if (aneg_status == 0 && fep->phydevs[1]->link && fep->phydevs[0]->link) {
		phy_stop(fep->phydevs[1]);
		fs_link_switch(fep);
		phy_start(fep->phydevs[1]);
		phy_stop(fep->phydevs[0]);
		phy_start(fep->phydevs[0]);
		aneg_status = 10;
	}
	if (aneg_status == 2 && fep->phydevs[1]->link && fep->phydevs[0]->link) {
		fep->phydevs[0]->state = PHY_DOUBLE_ATTACHEMENT;
		aneg_status = 3;
	}
	if (aneg_status == 3 &&	fep->phydevs[0]->state == PHY_HALTED)
	{
		aneg_status = 4;	
	}
	if (aneg_status == 4 && fep->phydevs[0]->state == PHY_RUNNING)
	{
		phy_stop(fep->phydevs[0]);
		phy_start(fep->phydevs[0]);
		aneg_status = 0;
	} 
	
	/* If the active PHY has a link and carrier is off, 
	   call netif_carrier_on */
	if (phydev->link) {
		if (! netif_carrier_ok(fep->phydev->attached_dev))
			netif_carrier_on(fep->phydev->attached_dev);
		return;
	}

	/* If we reach this point, the active PHY has lost its link */
	/* If the other PHY has a link -> switch */
	if ((phydev == fep->phydevs[0] && fep->phydevs[1]->link) ||
	    (phydev == fep->phydevs[1] && fep->phydevs[0]->link))
	{
		fs_link_switch(fep);
		/** in case where PHY1 is UP and PHY0 become UP latter,
		 *  we are not in the same case as before (PHY0 and PHY1
		 *  are not symetric) and we end up here.
		 *
		 *  in this case, simply "reset" PHY1 while starting network
		 *  will fix the issue
		 *
		 */ 
		if (aneg_status == 0 && (fep->phydevs[1]->link || fep->phydevs[0]->link)) {
			phy_stop(fep->phydevs[1]);
			phy_start(fep->phydevs[1]);
			aneg_status = 10;
		}
	} else {
		fep->change_time = jiffies;
		schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
	}
	
}
		
static int fs_init_phy(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	struct phy_device *phydev;
	phy_interface_t iface;

	fep->oldlink = 0;
	fep->oldspeed = 0;
	fep->oldduplex = -1;
	fep->change_time = jiffies;
	fep->mode = MODE_AUTO;

	iface = fep->fpi->use_rmii ?
		PHY_INTERFACE_MODE_RMII : PHY_INTERFACE_MODE_MII;

	phydev = of_phy_connect(dev, fep->fpi->phy_node, &fs_adjust_link, 0,
				iface);
	if (!phydev) {
		phydev = of_phy_connect_fixed_link(dev, &fs_adjust_link,
						   iface);
	}
	if (!phydev) {
		dev_err(&dev->dev, "Could not attach to PHY\n");
		return -ENODEV;
	}

	fep->phydev = phydev;
	fep->phydevs[0] = phydev;
	fep->phy_oldlinks[0] = 0;
	
	fep->phydevs[1] = of_phy_connect(dev, fep->fpi->phy_node2, 
				&fs_adjust_link, 0, iface);
	fep->phy_oldlinks[1] = 0;

	if (fep->phydevs[1] && fep->disable_phy == PHY_POWER_DOWN) {
		if (fep->phydevs[1]->drv->suspend) 
			fep->phydevs[1]->drv->suspend(fep->phydevs[1]);
	}

	return 0;
}

static int fs_enet_open(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	int r;
	int err;
	int value;
	int idx;

	/* to initialize the fep->cur_rx,... */
	/* not doing this, will cause a crash in fs_enet_rx_napi */
	fs_init_bds(fep->ndev);

	if (fep->fpi->use_napi)
		napi_enable(&fep->napi);

	/* Install our interrupt handler. */
	r = request_irq(fep->interrupt, fs_enet_interrupt, IRQF_SHARED,
			"fs_enet-mac", dev);
	if (r != 0) {
		dev_err(fep->dev, "Could not allocate FS_ENET IRQ!");
		if (fep->fpi->use_napi)
			napi_disable(&fep->napi);
		return -EINVAL;
	}

	err = fs_init_phy(dev);
	if (err) {
		free_irq(fep->interrupt, dev);
		if (fep->fpi->use_napi)
			napi_disable(&fep->napi);
		return err;
	}
	phy_start(fep->phydevs[0]);

	if (fep->phydevs[1]) {
		phy_start(fep->phydevs[1]);
		/* If the PHY must be isolated to disable it (MIAe) */
		if (fep->disable_phy == PHY_ISOLATE) {
			value = phy_read(fep->phydevs[1], MII_BMCR);
			phy_write(fep->phydevs[1], MII_BMCR, 
				((value & ~BMCR_PDOWN) | BMCR_ISOLATE));
			if (fep->gpio != -1) {
				ldb_gpio_set_value(fep->gpio, 0);
			}
			/* autoneg must be disabled at this point. Otherwise, 
			the driver will remove the ISOLATE bit in the command
			register and break networking */
			fep->phydevs[1]->autoneg = AUTONEG_DISABLE;
		}
	}

	//  if we are here, we reset aneg_status and aneg_counter
	aneg_status = 0;
	aneg_counter = 0;

	INIT_DELAYED_WORK(&fep->link_queue, fs_link_monitor);
	INIT_WORK(&fep->arp_queue, fs_send_gratuitous_arp);
	for (idx=0; idx<3; idx++)
		INIT_WORK(&fep->notify_work[idx].notify_queue, fs_sysfs_notify);
	schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);

	netif_start_queue(dev);

	return 0;
}

static int fs_enet_close(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	unsigned long flags;

	netif_stop_queue(dev);
	netif_carrier_off(dev);
	if (fep->fpi->use_napi)
		napi_disable(&fep->napi);
	cancel_delayed_work_sync(&fep->link_queue);
	phy_stop(fep->phydevs[0]);
	if (fep->phydevs[1]) {
		phy_stop(fep->phydevs[1]);
	}

	spin_lock_irqsave(&fep->lock, flags);
	spin_lock(&fep->tx_lock);
	(*fep->ops->stop)(dev);
	spin_unlock(&fep->tx_lock);
	spin_unlock_irqrestore(&fep->lock, flags);

	/* release any irqs */

	phy_disconnect(fep->phydevs[0]);
	if (fep->phydevs[1]) {
		phy_disconnect(fep->phydevs[1]);
	}
	free_irq(fep->interrupt, dev);

	return 0;
}

static struct net_device_stats *fs_enet_get_stats(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	return &fep->stats;
}

/*************************************************************************/

static void fs_get_drvinfo(struct net_device *dev,
			    struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, DRV_MODULE_NAME, sizeof(info->driver));
	strlcpy(info->version, DRV_MODULE_VERSION, sizeof(info->version));
}

static int fs_get_regs_len(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);

	return (*fep->ops->get_regs_len)(dev);
}

static void fs_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			 void *p)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	unsigned long flags;
	int r, len;

	len = regs->len;

	spin_lock_irqsave(&fep->lock, flags);
	r = (*fep->ops->get_regs)(dev, p, &len);
	spin_unlock_irqrestore(&fep->lock, flags);

	if (r == 0)
		regs->version = 0;
}

static int fs_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct fs_enet_private *fep = netdev_priv(dev);

	if (!fep->phydev)
		return -ENODEV;

	return phy_ethtool_gset(fep->phydev, cmd);
}

static int fs_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	int ret;
	int value;

	if (!fep->phydev)
		return -ENODEV;

	if (fep->phydevs[1] && cmd->phy_address == fep->phydevs[1]->addr) 
		ret = phy_ethtool_sset(fep->phydevs[1], cmd);
	else 
		ret = phy_ethtool_sset(fep->phydevs[0], cmd);

	/* Only one PHY or no PIO E 18 -> exit */
	if (fep->gpio == -1 || ! fep->phydevs[1]) 
		return ret;

	/* If PHY B is isolated or in power down, Switch to PHY A */
	value = phy_read(fep->phydevs[1], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (fep->phydev == fep->phydevs[1]) {
			dev_err(fep->dev, "Switch to PHY A\n");
			fep->phydev = fep->phydevs[0];
			if (fep->gpio != -1) {
				ldb_gpio_set_value(fep->gpio, 0);
			}
		}
		return ret;
	}

	/* If we reach this point, PHY B is eligible as active PHY */

	/* If PHY A is isolated or in power down, Switch to PHY B */
	value = phy_read(fep->phydevs[0], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (fep->phydev == fep->phydevs[0]) {
			dev_err(fep->dev, "Switch to PHY B\n");
			fep->phydev = fep->phydevs[1];
			if (fep->gpio != -1) {
				ldb_gpio_set_value(fep->gpio, 1);
			}
		}
		return ret;
	}

	/* If we reach this point, both PHYs are Powered up and not isolated. */
	/* This is an error. The network will certainly not work as expected. */

	return ret;
}

static int fs_nway_reset(struct net_device *dev)
{
	return 0;
}

static u32 fs_get_msglevel(struct net_device *dev)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	return fep->msg_enable;
}

static void fs_set_msglevel(struct net_device *dev, u32 value)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	fep->msg_enable = value;
}

static const struct ethtool_ops fs_ethtool_ops = {
	.get_drvinfo = fs_get_drvinfo,
	.get_regs_len = fs_get_regs_len,
	.get_settings = fs_get_settings,
	.set_settings = fs_set_settings,
	.nway_reset = fs_nway_reset,
	.get_link = ethtool_op_get_link,
	.get_msglevel = fs_get_msglevel,
	.set_msglevel = fs_set_msglevel,
	.get_regs = fs_get_regs,
	.get_ts_info = ethtool_op_get_ts_info,
};

static int fs_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct fs_enet_private *fep = netdev_priv(dev);
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&rq->ifr_data;
	int ret, value;
 
 	if (!netif_running(dev))
 		return -EINVAL;
 
	if (fep->phydevs[0]->addr == mii->phy_id)
		ret = phy_mii_ioctl(fep->phydevs[0], rq, cmd);
	else if (fep->phydevs[1]->addr == mii->phy_id)
		ret = phy_mii_ioctl(fep->phydevs[1], rq, cmd);
	else
		return -EINVAL;

	/* Only one PHY or no PIO E 18 -> exit */
	if (fep->gpio == -1 || ! fep->phydevs[1]) 
		return ret;

	/* If PHY B is isolated or in power down, Switch to PHY A */
	value = phy_read(fep->phydevs[1], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (fep->phydev == fep->phydevs[1]) {
			dev_err(fep->dev, "Switch to PHY A\n");
			fep->phydev = fep->phydevs[0];
			if (fep->gpio != -1) {
				ldb_gpio_set_value(fep->gpio, 0);
			}
		}
		return ret;
	}

	/* If we reach this point, PHY B is eligible as active PHY */

	/* If PHY A is isolated or in power down, Switch to PHY B */
	value = phy_read(fep->phydevs[0], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (fep->phydev == fep->phydevs[0]) {
			dev_err(fep->dev, "Switch to PHY B\n");
			fep->phydev = fep->phydevs[1];
			if (fep->gpio != -1) {
				ldb_gpio_set_value(fep->gpio, 1);
			}
		}
		return ret;
	}

	/* If we reach this point, both PHYs are Powered up and not isolated. */
	/* This is an error, the network will certainly not work as expected. */

	return ret;
}

extern int fs_mii_connect(struct net_device *dev);
extern void fs_mii_disconnect(struct net_device *dev);

/**************************************************************************************/
/* sysfs hook function */
static ssize_t fs_attr_active_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);

	return sprintf(buf, "%d\n",fep->phydev == fep->phydevs[1]?1:0);
}

static ssize_t fs_attr_active_link_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);
	int active = simple_strtol(buf, NULL, 10);

	if (active != 1) active = 0;
	if (!fep->phydevs[active]) {
		dev_warn(dev, "PHY on address %d does not exist\n", active);
		return count;
	}
	if (fep->phydevs[active] != fep->phydev) {
		fs_link_switch(fep);
		cancel_delayed_work_sync(&fep->link_queue);
		if (!fep->phydev->link && fep->mode == MODE_AUTO) schedule_delayed_work(&fep->link_queue, LINK_MONITOR_RETRY);
	}
	
	return count;
}
	
static DEVICE_ATTR(active_link, S_IRUGO | S_IWUSR, fs_attr_active_link_show, fs_attr_active_link_store);

static ssize_t fs_attr_phy0_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ctrl;
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);

	ctrl = phy_read(fep->phydevs[0], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : fep->phydevs[0]->link ? 2 : 1);
}

static DEVICE_ATTR(phy0_link, S_IRUGO, fs_attr_phy0_link_show, NULL);

static ssize_t fs_attr_phy1_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ctrl;
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);
	
	ctrl = phy_read(fep->phydevs[1], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : fep->phydevs[1]->link ? 2 : 1);
}

static DEVICE_ATTR(phy1_link, S_IRUGO, fs_attr_phy1_link_show, NULL);

static ssize_t fs_attr_phy0_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);
	struct phy_device *phydev = fep->phydevs[0];

	int mode = simple_strtol(buf, NULL, 10);
	
	if (mode && fep->phydev->drv->resume) phydev->drv->resume(phydev);
	if (!mode && fep->phydev->drv->suspend) phydev->drv->suspend(phydev);
	
	return count;
}

static DEVICE_ATTR(phy0_mode, S_IWUSR, NULL, fs_attr_phy0_mode_store);

static ssize_t fs_attr_phy1_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);
	struct phy_device *phydev = fep->phydevs[1];

	int mode = simple_strtol(buf, NULL, 10);
	
	if (mode && fep->phydev->drv->resume) phydev->drv->resume(phydev);
	if (!mode && fep->phydev->drv->suspend) phydev->drv->suspend(phydev);
	
	return count;
}

static DEVICE_ATTR(phy1_mode, S_IWUSR, NULL, fs_attr_phy1_mode_store);

static ssize_t fs_attr_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);

	return sprintf(buf, "%d\n",fep->mode);
}

static ssize_t fs_attr_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct fs_enet_private *fep = netdev_priv(ndev);
	int mode = simple_strtol(buf, NULL, 10);
	
	if (fep->mode != mode) {
		fep->mode = mode;
		if (mode == MODE_AUTO) {
			cancel_delayed_work_sync(&fep->link_queue);
			schedule_delayed_work(&fep->link_queue, 0);
		}
	}
	
	return count;
}
	
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, fs_attr_mode_show, fs_attr_mode_store);

/**************************************************************************************/

#ifdef CONFIG_FS_ENET_HAS_FEC
#define IS_FEC(match) ((match)->data == &fs_fec_ops)
#else
#define IS_FEC(match) 0
#endif

static const struct net_device_ops fs_enet_netdev_ops = {
	.ndo_open		= fs_enet_open,
	.ndo_stop		= fs_enet_close,
	.ndo_get_stats		= fs_enet_get_stats,
	.ndo_start_xmit		= fs_enet_start_xmit,
	.ndo_tx_timeout		= fs_timeout,
	.ndo_set_rx_mode	= fs_set_multicast_list,
	.ndo_do_ioctl		= fs_ioctl,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= fs_enet_netpoll,
#endif
};

static struct of_device_id fs_enet_match[];
static int fs_enet_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct net_device *ndev;
	struct fs_enet_private *fep;
	struct fs_platform_info *fpi;
	const u32 *data;
	struct clk *clk;
	int err;
	const u8 *mac_addr;
	const char *phy_connection_type;
	int privsize, len, ret = -ENODEV;
	int ngpios = of_gpio_count(ofdev->dev.of_node);
	int gpio = -1;
	char *Disable_PHY;
	char *use_PHY5;

	match = of_match_device(fs_enet_match, &ofdev->dev);
	if (!match)
		return -EINVAL;

	if (ngpios == 1) 
		gpio = ldb_gpio_init(ofdev->dev.of_node, &ofdev->dev, 0, 1);

	fpi = kzalloc(sizeof(*fpi), GFP_KERNEL);
	if (!fpi)
		return -ENOMEM;

	if (!IS_FEC(match)) {
		data = of_get_property(ofdev->dev.of_node, "fsl,cpm-command", &len);
		if (!data || len != 4)
			goto out_free_fpi;

		fpi->cp_command = *data;
	}

	fpi->rx_ring = 32;
	fpi->tx_ring = 32;
	fpi->rx_copybreak = 240;
	fpi->use_napi = 1;
	fpi->napi_weight = 17;
	fpi->phy_node = of_parse_phandle(ofdev->dev.of_node, "phy-handle", 0);
	if ((!fpi->phy_node) && (!of_get_property(ofdev->dev.of_node, "fixed-link",
						  NULL)))
		goto out_free_fpi;
	fpi->phy_node2 = of_parse_phandle(ofdev->dev.of_node, "phy-handle", 1);

	if (of_device_is_compatible(ofdev->dev.of_node, "fsl,mpc5125-fec")) {
		phy_connection_type = of_get_property(ofdev->dev.of_node,
						"phy-connection-type", NULL);
		if (phy_connection_type && !strcmp("rmii", phy_connection_type))
			fpi->use_rmii = 1;
	}

	/* make clock lookup non-fatal (the driver is shared among platforms),
	 * but require enable to succeed when a clock was specified/found,
	 * keep a reference to the clock upon successful acquisition
	 */
	clk = devm_clk_get(&ofdev->dev, "per");
	if (!IS_ERR(clk)) {
		err = clk_prepare_enable(clk);
		if (err) {
			ret = err;
			goto out_free_fpi;
		}
		fpi->clk_per = clk;
	}

	privsize = sizeof(*fep) +
		   sizeof(struct sk_buff **) *
		   (fpi->rx_ring + fpi->tx_ring);

	ndev = alloc_etherdev(privsize);
	if (!ndev) {
		ret = -ENOMEM;
		goto out_put;
	}

	SET_NETDEV_DEV(ndev, &ofdev->dev);
	platform_set_drvdata(ofdev, ndev);

	fep = netdev_priv(ndev);
	fep->dev = &ofdev->dev;
	fep->ndev = ndev;
	fep->fpi = fpi;
	fep->ops = match->data;
	fep->gpio = gpio;

	fep->disable_phy = 0;
	Disable_PHY = (char *)of_get_property(ofdev->dev.of_node, 
		"PHY-disable", NULL);
	if (Disable_PHY) {
		printk("PHY-disable = %s\n", Disable_PHY);
		if (! strcmp(Disable_PHY, "power-down"))
			fep->disable_phy = PHY_POWER_DOWN;
		else if (! strcmp(Disable_PHY, "isolate"))
			fep->disable_phy = PHY_ISOLATE;
	}

	fep->use_PHY5 = 0;
	use_PHY5 = (char *)of_get_property(ofdev->dev.of_node, 
		"use-PHY5", NULL);
	if (use_PHY5) 
		fep->use_PHY5=1;

	ret = fep->ops->setup_data(ndev);
	if (ret)
		goto out_free_dev;

	fep->rx_skbuff = (struct sk_buff **)&fep[1];
	fep->tx_skbuff = fep->rx_skbuff + fpi->rx_ring;

	spin_lock_init(&fep->lock);
	spin_lock_init(&fep->tx_lock);

	mac_addr = of_get_mac_address(ofdev->dev.of_node);
	if (mac_addr)
		memcpy(ndev->dev_addr, mac_addr, 6);

	ret = fep->ops->allocate_bd(ndev);
	if (ret)
		goto out_cleanup_data;

	fep->rx_bd_base = fep->ring_base;
	fep->tx_bd_base = fep->rx_bd_base + fpi->rx_ring;

	fep->tx_ring = fpi->tx_ring;
	fep->rx_ring = fpi->rx_ring;

	ndev->netdev_ops = &fs_enet_netdev_ops;
	ndev->watchdog_timeo = 2 * HZ;
	if (fpi->use_napi)
		netif_napi_add(ndev, &fep->napi, fs_enet_rx_napi,
			       fpi->napi_weight);

	ndev->ethtool_ops = &fs_ethtool_ops;

	init_timer(&fep->phy_timer_list);

	netif_carrier_off(ndev);

	ret = register_netdev(ndev);
	if (ret)
		goto out_free_bd;

	pr_info("%s: fs_enet: %pM\n", ndev->name, ndev->dev_addr);

	ret = 0;
	ret |= device_create_file(fep->dev, &dev_attr_active_link);
	ret |= device_create_file(fep->dev, &dev_attr_phy0_link);
	if (fpi->phy_node2) ret |= device_create_file(fep->dev, &dev_attr_phy1_link);
	ret |= device_create_file(fep->dev, &dev_attr_phy0_mode);
	if (fpi->phy_node2) ret |= device_create_file(fep->dev, &dev_attr_phy1_mode);
	ret |= device_create_file(fep->dev, &dev_attr_mode);
	if (ret)
		goto out_remove_file;

	fep->notify_work[PHY0_LINK].sd = 
		sysfs_get_dirent(fep->dev->kobj.sd, NULL, "phy0_link");
	fep->notify_work[PHY1_LINK].sd = 
		sysfs_get_dirent(fep->dev->kobj.sd, NULL, "phy1_link");
	fep->notify_work[ACTIVE_LINK].sd = 
		sysfs_get_dirent(fep->dev->kobj.sd, NULL, "active_link");

	return 0;

out_remove_file:
	device_remove_file(fep->dev, &dev_attr_active_link);
	device_remove_file(fep->dev, &dev_attr_phy0_link);
	device_remove_file(fep->dev, &dev_attr_phy1_link);
	device_remove_file(fep->dev, &dev_attr_phy0_mode);
	device_remove_file(fep->dev, &dev_attr_phy1_mode);
	device_remove_file(fep->dev, &dev_attr_mode);
	unregister_netdev(ndev);
out_free_bd:
	fep->ops->free_bd(ndev);
out_cleanup_data:
	fep->ops->cleanup_data(ndev);
out_free_dev:
	free_netdev(ndev);
out_put:
	of_node_put(fpi->phy_node);
	of_node_put(fpi->phy_node2);
	if (fpi->clk_per)
		clk_disable_unprepare(fpi->clk_per);
out_free_fpi:
	kfree(fpi);
	return ret;
}

static int fs_enet_remove(struct platform_device *ofdev)
{
	struct net_device *ndev = platform_get_drvdata(ofdev);
	struct fs_enet_private *fep = netdev_priv(ndev);

	device_remove_file(fep->dev, &dev_attr_active_link);
	device_remove_file(fep->dev, &dev_attr_phy0_link);
	device_remove_file(fep->dev, &dev_attr_phy1_link);
	device_remove_file(fep->dev, &dev_attr_phy0_mode);
	device_remove_file(fep->dev, &dev_attr_phy1_mode);
	device_remove_file(fep->dev, &dev_attr_mode);
	
	unregister_netdev(ndev);

	fep->ops->free_bd(ndev);
	fep->ops->cleanup_data(ndev);
	dev_set_drvdata(fep->dev, NULL);
	of_node_put(fep->fpi->phy_node);
	of_node_put(fep->fpi->phy_node2);
	if (fep->fpi->clk_per)
		clk_disable_unprepare(fep->fpi->clk_per);
	free_netdev(ndev);
	return 0;
}

static struct of_device_id fs_enet_match[] = {
#ifdef CONFIG_FS_ENET_HAS_SCC
	{
		.compatible = "fsl,cpm1-scc-enet",
		.data = (void *)&fs_scc_ops,
	},
	{
		.compatible = "fsl,cpm2-scc-enet",
		.data = (void *)&fs_scc_ops,
	},
#endif
#ifdef CONFIG_FS_ENET_HAS_FCC
	{
		.compatible = "fsl,cpm2-fcc-enet",
		.data = (void *)&fs_fcc_ops,
	},
#endif
#ifdef CONFIG_FS_ENET_HAS_FEC
#ifdef CONFIG_FS_ENET_MPC5121_FEC
	{
		.compatible = "fsl,mpc5121-fec",
		.data = (void *)&fs_fec_ops,
	},
	{
		.compatible = "fsl,mpc5125-fec",
		.data = (void *)&fs_fec_ops,
	},
#else
	{
		.compatible = "fsl,pq1-fec-enet",
		.data = (void *)&fs_fec_ops,
	},
#endif
#endif
	{}
};
MODULE_DEVICE_TABLE(of, fs_enet_match);

static struct platform_driver fs_enet_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "fs_enet",
		.of_match_table = fs_enet_match,
	},
	.probe = fs_enet_probe,
	.remove = fs_enet_remove,
};

#ifdef CONFIG_NET_POLL_CONTROLLER
static void fs_enet_netpoll(struct net_device *dev)
{
       disable_irq(dev->irq);
       fs_enet_interrupt(dev->irq, dev);
       enable_irq(dev->irq);
}
#endif

module_platform_driver(fs_enet_driver);
