/*
 * DMA engine driver for marvell 37xx SoCs.
 * Copyright (C) 2017 Maxime Chevallier <maxime.chevallier@smile.fr> FIXME
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/memory.h>
#include <linux/dmapool.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include "dmaengine.h"
#include "virt-dma.h"

/* Descriptor pool config */
#define MV_DMA_SLOT_SIZE		64
#define MV_DMA_THRESHOLD		1

#define MV_DMA_MIN_BYTE_COUNT		SZ_128
#define MV_DMA_MAX_BYTE_COUNT		(SZ_16M - 1)

/* Interrupt cause */
#define MV_DMA_INTR_EOD		BIT(0)
#define MV_DMA_INTR_EOC		BIT(1)
#define MV_DMA_INTR_STOPPED	BIT(2)
#define MV_DMA_INTR_PAUSED	BIT(3)
#define MV_DMA_INTR_ADDR_DECODE	BIT(4)
#define MV_DMA_INTR_ACC_PROT	BIT(5)
#define MV_DMA_INTR_WR_PROT	BIT(6)
#define MV_DMA_INTR_OWN_PROT	BIT(7)
#define MV_DMA_INTR_PARITY_ERR	BIT(8)
#define MV_DMA_INTR_BAR_ERR	BIT(9)

#define MV_DMA_INTR_ERRS	(MV_DMA_INTR_ADDR_DECODE | \
				MV_DMA_INTR_ACC_PROT | \
				MV_DMA_INTR_WR_PROT | \
				MV_DMA_INTR_OWN_PROT | \
				MV_DMA_INTR_PARITY_ERR | \
				MV_DMA_INTR_BAR_ERR)

#define MV_DMA_INTR_MASK	(MV_DMA_INTR_ERRS | \
				 MV_DMA_INTR_EOC | \
				 MV_DMA_INTR_STOPPED)

/* Values for the MV_DMA_CFG_ADDR register */
#define MV_DMA_CFG_ADDR_CFG_DDR		0
#define MV_DMA_CFG_ADDR_CFG_PCIE	1
#define MV_DMA_CFG_ADDR_CFG_SPI		2
#define MV_DMA_CFG_ADDR_CFG_UART2	3
#define MV_DMA_CFG_ADDR_32BIT		BIT(4)
#define MV_DMA_CFG_ADDR_FIXED_ADDR	BIT(5)

/* Values for the MV_DMA_CFG register */
#define MV_DMA_OPERATION_MODE_XOR	0
#define MV_DMA_OPERATION_MODE_CRC32	1
#define MV_DMA_OPERATION_MODE_DMA	2
#define MV_DMA_OPERATION_MODE_IN_DESC   7
#define MV_DMA_DESCRIPTOR_SWAP		BIT(14)
#define MV_DMA_DESC_SUCCESS		0x40000000

/* Values for the MV_DMA_ACTIVATION register */
#define MV_DMA_XESTATUS		(0x03 << 4)
#define MV_DMA_XERESTART	BIT(3)
#define MV_DMA_XEPAUSE		BIT(2)
#define MV_DMA_XESTOP		BIT(1)
#define MV_DMA_XESTART		BIT(0)

#define MV_DMA_XESTATUS_NOT_ACTIVE	0
#define MV_DMA_XESTATUS_ACTIVE		1
#define MV_DMA_XESTATUS_PAUSED		2

#define MV_DMA_DESC_OPERATION_XOR       (0 << 24)
#define MV_DMA_DESC_OPERATION_CRC32C    (1 << 24)
#define MV_DMA_DESC_OPERATION_DMA    (2 << 24)

/* High range per-channel registers */
#define MV_DMA_H_CH_NEXT_DESC		0x00
#define MV_DMA_H_CH_CURR_DES 		0x10
#define MV_DMA_H_CH_BYTE_COUNT		0x20
#define MV_DMA_H_CH_WINDOW_CTRL		0x40
#define MV_DMA_H_CH_ADDR_OVERRIDE_CTRL	0xA0
#define MV_DMA_H_CH_DEST_POINTER	0xB0
#define MV_DMA_H_CH_BLOCK_SIZE		0xC0

/* High range per-window registers */
#define MV_DMA_H_WIN_BASE_ADDR		0x50
#define MV_DMA_H_WIN_SIZE_MASK		0x70
/* Only for window 0 to 3 */
#define MV_DMA_H_WIN_HIGH_ADDR_REMAP	0x90

/* High range global registers */
#define MV_DMA_H_INIT_VALUE_LOW		0xE0
#define MV_DMA_H_INIT_VALUE_HIGH	0xE4

/* Low range per-channel registers */
#define MV_DMA_L_CH_CFG_ADDR		0x04
#define MV_DMA_L_CH_CFG			0x10
#define MV_DMA_L_CH_ACTIVATION		0x20
#define MV_DMA_L_CH_OUTSTANDING_RD	0x80

/* Low range global registers */
#define MV_DMA_L_CHAN_ARBITER		0x00
#define MV_DMA_L_INTR_CAUSE		0x30
#define MV_DMA_L_INTR_MASK		0x40
#define MV_DMA_L_ERROR_CAUSE		0x50
#define MV_DMA_L_ERROR_ADDR		0x60

/* HW desc status values */
#define MV_DMA_DESC_STATUS_OK		BIT(30)
#define MV_DMA_DESC_OWNER_XOR		BIT(31)

#define MV_CH_BASE(chan) (chan->mv_dma_dev->base)
#define MV_CH_HBASE(chan) (chan->mv_dma_dev->high_base)

#define MV_CH_REG(chan, reg) (MV_CH_BASE(chan) + reg + 4 * chan->mv_chan_id)
#define MV_CH_HREG(chan, reg) (MV_CH_HBASE(chan) + reg + 4 * chan->mv_chan_id)

#define MV_DEV_REG(dev, reg) (dev->base + reg)
#define MV_DEV_HREG(dev, reg) (dev->high_base + reg)

#define MV_WIN_REG(dev, win, reg) (dev->high_base + reg + 4 * win)

/* FIXME : 4bytes only for SPI and UART2 */
#define MV_DMA_BUSWIDTH	( BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | \
			  BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
			  BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) )

/* All known DMA / XOR controllers have at most 2 channels */
#define MV_DMA_MAX_CHANNELS 2

enum mv_dma_sw_desc_status {
	MV_DMA_SW_DESC_COMPLETED,
	MV_DMA_SW_DESC_PENDING,
	MV_DMA_SW_DESC_ERROR,
};

struct mv_dma_sw_desc {
	struct virt_dma_desc	vdesc;
	/* list of mv_dma_slot */
	struct list_head	slots;
};

/* a mv_dma_slot represent one HW desc */
struct mv_dma_slot {
	struct list_head 	node;
	dma_addr_t		desc_phys_addr;
	void			*desc_virt_addr;
};

/*
 * This structure describes DMA descriptor size 64bytes. The
 * mv_phy_src_idx() macro must be used when indexing the values of the
 * phy_src_addr[] array. This is due to the fact that the 'descriptor
 * swap' feature, used on big endian systems, swaps descriptors data
 * within blocks of 8 bytes. So two consecutive values of the
 * phy_src_addr[] array are actually swapped in big-endian, which
 * explains the different mv_phy_src_idx() implementation.
 */
#if defined(__LITTLE_ENDIAN)
struct mv_dma_desc {
	u32 status;		/* descriptor execution status */
	u32 crc32_result;	/* result of CRC-32 calculation */
	u32 desc_command;	/* type of operation to be carried out */
	u32 phy_next_desc;	/* next descriptor address pointer */
	u32 byte_count;		/* size of src/dst blocks in bytes */
	u32 phy_dest_addr;	/* destination block address */
	u32 phy_src_addr[8];	/* source block addresses */
	u32 reserved0;
	u32 reserved1;
};
#define mv_phy_src_idx(src_idx) (src_idx)
#else
struct mv_dma_desc {
	u32 crc32_result;	/* result of CRC-32 calculation */
	u32 status;		/* descriptor execution status */
	u32 phy_next_desc;	/* next descriptor address pointer */
	u32 desc_command;	/* type of operation to be carried out */
	u32 phy_dest_addr;	/* destination block address */
	u32 byte_count;		/* size of src/dst blocks in bytes */
	u32 phy_src_addr[8];	/* source block addresses */
	u32 reserved1;
	u32 reserved0;
};
#define mv_phy_src_idx(src_idx) (src_idx ^ 1)
#endif

enum mv_chan_status {
	MV_CHAN_NOT_ACTIVE = 0,
	MV_CHAN_ACTIVE,
	MV_CHAN_PAUSED,
};

/* Wrapper for a channel on the DMA engine.
 * @vchan the virtual channel, used by the dma_engine. We use a virt_chan
 * rather than a raw dam_chan because virt_chan does the descriptor
 * housekeeping for us.*/
struct mv_dma_chan {
	struct			virt_dma_chan vchan;
	unsigned int		addr_cfg;

	/* For dev2mem / mem2dev, we need the device address to R/W to. */
	phys_addr_t		src_dev_addr;
	phys_addr_t		dst_dev_addr;

	struct			mv_dma_device *mv_dma_dev;
	int			mv_chan_id;
	int			irq;
	int			slots_allocated;
	/* When this flag is set, the engine must start the pending chain when
	 * it finishes processing the current one. */
	struct tasklet_struct	irq_tasklet;

	/* HW descriptors memory space */
	struct dma_pool		*pool;
	size_t			pool_size;
};

struct mv_dma_device {
	struct dma_device	dma_dev;
	struct platform_device	*pdev;
	void __iomem		*base;
	void __iomem		*high_base;
	unsigned int 		nr_channels;
	struct mv_dma_chan 	channels[MV_DMA_MAX_CHANNELS];
};

static struct mv_dma_chan *to_mv_dma_chan(struct virt_dma_chan *vchan)
{
	return container_of(vchan, struct mv_dma_chan, vchan);
}

static struct mv_dma_sw_desc *to_mv_dma_sw_desc(struct virt_dma_desc *vdesc)
{
	return container_of(vdesc, struct mv_dma_sw_desc, vdesc);
}

static struct device *mv_chan_to_devp(struct mv_dma_chan *mv_chan)
{
	return mv_chan->vchan.chan.device->dev;
}

/* Get the interrupt cause for this particular channel. The result is right
 * shifted. */
static u32 mv_dma_interrupt_cause(struct mv_dma_chan *mv_chan)
{
	u32 val = readl(MV_CH_REG(mv_chan, MV_DMA_L_INTR_CAUSE));
	return val >> (16 * mv_chan->mv_chan_id);
}

static void mv_dma_interrupt_clear(struct mv_dma_chan *mv_chan, u32 mask)
{
	u32 val = readl(MV_CH_REG(mv_chan, MV_DMA_L_INTR_CAUSE));
	val &= ~(mask << 16 * mv_chan->mv_chan_id);
	writel(val, MV_CH_REG(mv_chan, MV_DMA_L_INTR_CAUSE));
}

static void mv_dma_interrupt_enable(struct mv_dma_chan *mv_chan, u32 mask)
{
	u32 val = readl(MV_CH_REG(mv_chan, MV_DMA_L_INTR_MASK));
	val |= (mask << 16 * mv_chan->mv_chan_id);
	writel(val, MV_CH_REG(mv_chan, MV_DMA_L_INTR_MASK));
}

static void mv_dma_interrupt_disable(struct mv_dma_chan *mv_chan, u32 mask)
{
	u32 val = readl(MV_CH_REG(mv_chan, MV_DMA_L_INTR_MASK));
	val &= ~(mask << 16 * mv_chan->mv_chan_id);
	writel(val, MV_CH_REG(mv_chan, MV_DMA_L_INTR_MASK));
}

static u32 mv_dma_chan_status(struct mv_dma_chan *mv_chan) {
	u32 chan_status = readl(MV_CH_REG(mv_chan, MV_DMA_L_CH_ACTIVATION));
	return (chan_status & MV_DMA_XESTATUS) >> 4;
}

static void mv_dma_chan_set_status(struct mv_dma_chan *mv_chan, u32 status)
{
	/* This clears other activation bits. */
	writel(status, MV_CH_REG(mv_chan, MV_DMA_L_CH_ACTIVATION));
}

static u32 mv_dma_slot_get_hw_status(struct mv_dma_slot *mv_slot)
{
	struct mv_dma_desc *hw_desc =
			(struct mv_dma_desc *)mv_slot->desc_virt_addr;
	return hw_desc->status;
}

static void mv_dma_chan_start(struct mv_dma_chan *mv_chan)
{
	mv_dma_interrupt_clear(mv_chan, MV_DMA_INTR_MASK);
	mv_dma_interrupt_enable(mv_chan, MV_DMA_INTR_MASK);

	mv_dma_chan_set_status(mv_chan, MV_DMA_XESTART);
}

static struct mv_dma_slot
*mv_dma_get_first_slot(struct mv_dma_sw_desc *mv_sw_desc)
{
	return list_first_entry(&mv_sw_desc->slots, struct mv_dma_slot, node);
}

/* Check the status of a sw_desc. This will loop through the HW desc to check
 * the status set by the engine. If one of the hw_desc has an error flag, report
 * the whole sw_desc as ERROR.*/
static enum mv_dma_sw_desc_status
mv_dma_get_sw_desc_status(struct mv_dma_sw_desc *mv_sw_desc)
{
	struct mv_dma_slot *mv_slot;
	u32 status;

	list_for_each_entry(mv_slot, &mv_sw_desc->slots, node) {
		status = mv_dma_slot_get_hw_status(mv_slot);

		if (status & MV_DMA_DESC_OWNER_XOR)
			return MV_DMA_SW_DESC_PENDING;

		if (! (status & MV_DMA_DESC_STATUS_OK) )
			return MV_DMA_SW_DESC_ERROR;
	}

	return MV_DMA_SW_DESC_COMPLETED;
}

/* Set the channel's next desc to be the first desc of the 'issued' list*/
static void mv_dma_set_first_desc(struct mv_dma_chan *mv_chan)
{
	/* Get first descriptor from issued list */
	struct virt_dma_desc *vdesc = vchan_next_desc(&mv_chan->vchan);

	struct mv_dma_sw_desc *mv_sw_desc = to_mv_dma_sw_desc(vdesc);

	/* Get first slot from this descriptor. We assume all slots inside the
	 * descriptor to be linked. */
	struct mv_dma_slot *mv_slot = mv_dma_get_first_slot(mv_sw_desc);

	writel(mv_slot->desc_phys_addr,
			MV_CH_HREG(mv_chan, MV_DMA_H_CH_NEXT_DESC));
}

static void mv_dma_set_next_desc(struct mv_dma_slot *mv_slot,
				struct mv_dma_slot *mv_next_slot)
{
	struct mv_dma_desc *hw_desc = 
		(struct mv_dma_desc *) mv_slot->desc_virt_addr;

	/* We expect the next descriptor address in the HW desc to be NULL */
	BUG_ON(hw_desc->phy_next_desc);
	hw_desc->phy_next_desc = mv_next_slot->desc_phys_addr;
}

static bool mv_dma_chan_is_busy(struct mv_dma_chan *mv_chan)
{
	return (mv_dma_chan_status(mv_chan) == MV_CHAN_ACTIVE);
}

static void mv_dma_start_new_chain(struct mv_dma_chan *mv_chan)
{
	/* bug on engine busy */
	BUG_ON(mv_dma_chan_is_busy(mv_chan));

	/* Register the first descriptor of the submitted chain to the
	 * engine*/
	mv_dma_set_first_desc(mv_chan);

	/* Start the engine */
	mv_dma_chan_start(mv_chan);
}

static void mv_dma_issue_pending(struct dma_chan *chan)
{
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(to_virt_chan(chan));
	struct device *dev = mv_chan_to_devp(mv_chan);;

	dev_info(dev, "%s\n", __func__);

	spin_lock_bh(&mv_chan->vchan.lock);

	/* Put the 'submitted' chain at the end of the 'issued' chain */
	vchan_issue_pending(&mv_chan->vchan);

	/* If the chan is busy, it will automatically process the new
	 * transactions as it finishes the current chain. If not, we have to
	 * restart the engine. */
	if (!mv_dma_chan_is_busy(mv_chan))
		mv_dma_start_new_chain(mv_chan);
	
	spin_unlock_bh(&mv_chan->vchan.lock);

	return;
}

static struct mv_dma_slot *mv_dma_alloc_slot(struct mv_dma_chan *mv_chan)
{
	struct mv_dma_slot *mv_slot = NULL;

	mv_slot = kzalloc(sizeof(*mv_slot), GFP_KERNEL);
	if (!mv_slot)
		return NULL;

	mv_slot->desc_virt_addr = dma_pool_alloc(mv_chan->pool, GFP_NOWAIT, 
						&mv_slot->desc_phys_addr);

	if (!mv_slot->desc_virt_addr)
		goto dma_alloc_err;

	return mv_slot;

dma_alloc_err:

	kfree(mv_slot);
	return NULL;
}

static struct mv_dma_sw_desc *mv_dma_alloc_desc(struct mv_dma_chan *mv_chan)
{
	struct mv_dma_sw_desc *mv_sw_desc = NULL;

	mv_sw_desc = kzalloc(sizeof(*mv_sw_desc), GFP_KERNEL);
	if (!mv_sw_desc)
		return NULL;

	INIT_LIST_HEAD(&mv_sw_desc->slots);

	return mv_sw_desc;
}

/* Setup the controller to the correct address types, depending on the required
 * sources and destination peripherals. Supported transfers are :
 * - DDR to DDR
 * - DDR to PCIE
 * - DDR to SPI
 * - DDR to UART2
 * - PCIE to DDR
 * - SPI to DDR
 * - UART2 to DDR
 * Each of these configuration requires a dedicated channel, so only two of
 * them can run concurrently. */
static int mv_dma_chan_config_addr(struct mv_dma_chan *mv_chan,
				enum dma_transfer_direction dir)
{
	struct device *dev = mv_chan_to_devp(mv_chan);

	u16 dev_addr_cfg = 0;
	u16 src_addr_cfg = 0;
	u16 dst_addr_cfg = 0;
	u32 addr_cfg;

	/* For mem2dev or dev2mem, determine the dev address configuration */
	switch(mv_chan->addr_cfg) {
	case MV_DMA_CFG_ADDR_CFG_DDR :
		if (dir != DMA_MEM_TO_MEM)
			goto err_config;
		break;
	case MV_DMA_CFG_ADDR_CFG_PCIE :
		/* PCIe to DDR : incremental, 64 bits */
		dev_addr_cfg = MV_DMA_CFG_ADDR_CFG_PCIE;
		break;
	case MV_DMA_CFG_ADDR_CFG_SPI :
		/* SPI to DDR : fixed source, 32 bits */
		dev_addr_cfg = MV_DMA_CFG_ADDR_CFG_SPI |
				MV_DMA_CFG_ADDR_FIXED_ADDR |
				MV_DMA_CFG_ADDR_32BIT;
		break;
	case MV_DMA_CFG_ADDR_CFG_UART2 :
		/* UART2 to DDR : fixed source, 32 bits */
		dev_addr_cfg = MV_DMA_CFG_ADDR_CFG_UART2 |
				MV_DMA_CFG_ADDR_FIXED_ADDR |
				MV_DMA_CFG_ADDR_32BIT;
		break;
	default:
		goto err_config;
	}

	/* Determine the correct src_addr and dst_addr configs based on
	 * transfer direction */
	switch (dir) {
	case DMA_DEV_TO_MEM :
		src_addr_cfg = dev_addr_cfg;
		dst_addr_cfg = MV_DMA_CFG_ADDR_CFG_DDR;
		break;
	case DMA_MEM_TO_DEV :
		src_addr_cfg = MV_DMA_CFG_ADDR_CFG_DDR;
		dst_addr_cfg = dev_addr_cfg;
		break;
	case DMA_MEM_TO_MEM :
		/* DDR to DDR */
		src_addr_cfg = MV_DMA_CFG_ADDR_CFG_DDR;
		dst_addr_cfg = MV_DMA_CFG_ADDR_CFG_DDR;
		break;
	default :
		dev_err(dev, "Unsupported DMA transfer mode %d\n", dir);
		return -EINVAL;
	}

	/* Write address config to the controller */
	addr_cfg = ((dst_addr_cfg << 16) | src_addr_cfg);
	writel(addr_cfg, MV_CH_REG(mv_chan, MV_DMA_L_CH_CFG_ADDR));

	return 0;
err_config:
	dev_err(dev, "Invalid device config (%d) regarding the chosen "
			"direction (%d)", mv_chan->addr_cfg, dir);

	return -EINVAL;
}

/* Get an async_tx desc for a scatter-gather operation.
 * For now, we impose that the direction is the same as requested during the
 * chan_config operation. If not, we fail.
 * */
static struct dma_async_tx_descriptor *mv_dma_prep_slave_sg(
					struct dma_chan *chan,
					struct scatterlist *sgl,
					unsigned int sg_len,
					enum dma_transfer_direction dir,
					unsigned long flags, void *context)
{
	struct device *dev = chan->device->dev;
	struct mv_dma_sw_desc *mv_sw_desc;
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(to_virt_chan(chan));
	dma_addr_t slot_addr;
	size_t slot_len;
	struct mv_dma_slot *mv_slot, *mv_prev_slot = NULL;
	struct mv_dma_desc *hw_desc;
	struct dma_async_tx_descriptor *tx;

	/* Buffer variables to iterate over scatterlist */
	struct scatterlist *sg;
	int i;

	/* Configure the address type to use the correct dev2mem parameters */
	if (mv_dma_chan_config_addr(mv_chan, dir))
		return NULL;

	/* Create the sw desc, that contains the vdesc and the slot list */
	mv_sw_desc = mv_dma_alloc_desc(mv_chan);

	/* Init the vdesc */
	tx = vchan_tx_prep(to_virt_chan(chan), &mv_sw_desc->vdesc, flags);

	/* Create the slots. Assume the SG list has correct size :
	 * Each block's size is > burst limit, and < 16MB - 1*/
	for_each_sg(sgl, sg, sg_len, i) {
		slot_addr = sg_dma_address(sg);
		slot_len = sg_dma_len(sg);

		/* Alloc the slot */
		mv_slot = mv_dma_alloc_slot(mv_chan);

		/* Configure the slot */
		hw_desc = (struct mv_dma_desc *) mv_slot->desc_virt_addr;

		hw_desc->status = MV_DMA_DESC_OWNER_XOR;
		hw_desc->byte_count = slot_len;

		if (dir == DMA_MEM_TO_DEV) {
			hw_desc->phy_src_addr[0] = slot_addr;
			hw_desc->phy_dest_addr = mv_chan->dst_dev_addr;
		} else {
			hw_desc->phy_src_addr[0] = mv_chan->src_dev_addr;
			hw_desc->phy_dest_addr = slot_addr;
		}

		/* DMA only for now. */
		hw_desc->desc_command = MV_DMA_DESC_OPERATION_DMA;

		/* Link it to the previous one if needed */
		if (mv_prev_slot)
			mv_dma_set_next_desc(mv_prev_slot, mv_slot);
		else
			hw_desc->phy_next_desc = 0;

		list_add_tail(&mv_slot->node, &mv_sw_desc->slots);
		mv_prev_slot = mv_slot;
	}

	dev_info(dev, "%s : %d SG elements\n", __func__, i);
	return tx;
}

static enum dma_status mv_dma_tx_status(struct dma_chan *chan,
					    dma_cookie_t cookie,
					    struct dma_tx_state *txstate)
{
	return dma_cookie_status(chan, cookie, txstate);
}

/* This tasklet is called when the chain is done being processed.
 * It is responsible for :
 * 	- Putting every processed sw_desc from the 'issued' list into the
 * 	  'completed' list
 * 	- Calling the vchan_complete function to notify the completion of these
 * 	  descriptors
 * 	- Scheduling the next descriptors in the 'issued' list to run
 * */
static void mv_dma_tasklet(unsigned long data)
{
	struct mv_dma_chan *mv_chan = (struct mv_dma_chan *) data;
	struct virt_dma_desc *vd, *tmp;
	struct mv_dma_sw_desc *mv_sw_desc;
	enum mv_dma_sw_desc_status status;

	/* The EOC from the engine means that it is done processing the chain
	 * contained in the current sw_desc.
	 *
	 * Support for hotchaining could be done fairly easily, with a 'hotchained'
	 * flag in a sw_desc. */

	spin_lock_bh(&mv_chan->vchan.lock);

	list_for_each_entry_safe(vd, tmp, &mv_chan->vchan.desc_issued, node) {
		mv_sw_desc = to_mv_dma_sw_desc(vd);

		status = mv_dma_get_sw_desc_status(mv_sw_desc);

		/* This descriptor was not processed by the engine, it should
		 * stay at the head of the issued queue. */
		if (status == MV_DMA_SW_DESC_PENDING)
			break;

		/* The descriptor must be removed from the issued queue */
		list_del(&vd->node);

		/* TODO Gracefully deal with errors  */
		if (status == MV_DMA_SW_DESC_ERROR)
			dev_warn(mv_chan_to_devp(mv_chan),
					"Error while processing a descriptor\n");

		dev_info(mv_chan_to_devp(mv_chan), "Vdesc processed successfully\n");
		vchan_cookie_complete(vd);
	}

	/* If there are still descriptors to be processed, restart the engine */
	if (!list_empty(&mv_chan->vchan.desc_issued))
		mv_dma_start_new_chain(mv_chan);

	spin_unlock_bh(&mv_chan->vchan.lock);
}

/* IRQ can be issued on two occasions :
 * - The end of chain is reached, in that case all issued descriptors were
 *   consumed by the engine, which is now in the 'stopped' state.
 * - An error occured
 * */
static irqreturn_t mv_dma_interrupt_handler(int irq, void *data)
{
	struct mv_dma_chan *chan = data;

	u32 intr_cause = mv_dma_interrupt_cause(chan);

	dev_dbg(chan->mv_dma_dev->dma_dev.dev, "intr cause %x\n", intr_cause);
	if (intr_cause & MV_DMA_INTR_ERRS){
		/* FIXME Take action on error intr */
		/* mv_chan_err_interrupt_handler(chan, intr_cause); */
		dev_warn(chan->mv_dma_dev->dma_dev.dev, "intr error : %x\n", intr_cause);
	}

	tasklet_schedule(&chan->irq_tasklet);

	/* FIXME Clear everything ? */
	mv_dma_interrupt_clear(chan, intr_cause);

	return IRQ_HANDLED;
}

static void mv_dma_free_sw_desc(struct virt_dma_desc *vdesc)
{
	struct mv_dma_slot *mv_slot, *tmp;
	struct mv_dma_sw_desc *mv_sw_desc = to_mv_dma_sw_desc(vdesc);
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(
					to_virt_chan(vdesc->tx.chan));

	/* Free each slot */
	list_for_each_entry_safe(mv_slot, tmp, &mv_sw_desc->slots, node) {
		list_del(&mv_slot->node);

		dma_pool_free(mv_chan->pool, mv_slot->desc_virt_addr,
						mv_slot->desc_phys_addr);
		kfree(mv_slot);
	}

	/* Free the descriptor */
	kfree(mv_sw_desc);
}

static int mv_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(to_virt_chan(chan));
	mv_dma_interrupt_enable(mv_chan, MV_DMA_INTR_MASK);
	return 0;
}

static void mv_dma_free_chan_resources(struct dma_chan *chan)
{
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(to_virt_chan(chan));
	mv_dma_interrupt_disable(mv_chan, MV_DMA_INTR_MASK);
	return;
}

/* Returns the burst_param to be configured into the DMA controller. */
static u32 mv_dma_get_burst_param(struct mv_dma_chan *mv_chan, u32 burst_size)
{
	switch(burst_size) {
	case 8: return 0;
	case 16: return 1;
	case 32: return 2;
	case 64: return 3;
	case 128: return 4;
	/* Fallback to 8 bytes, which is compatible with every possible device
	 * configuration, but might by non-optimal since this will solicitate
	 * more the DMA engine.*/
	default:
		dev_info(mv_chan_to_devp(mv_chan), "Invalid burst size requested "
				": %u, falling back to 8 bytes\n", burst_size);
		return 0;
	}
}

/* Setup the channel config in the controller to use the correct burst limit,
 * and transfer type.
 * We use descriptor-based transfer types.
 * */
static void mv_dma_chan_config(struct mv_dma_chan *mv_chan,
				struct dma_slave_config *config)
{
	u32 chan_config = 0;

	u32 burst_cfg = mv_dma_get_burst_param(mv_chan, config->src_maxburst);
	chan_config |= (burst_cfg << 4);
	burst_cfg = mv_dma_get_burst_param(mv_chan, config->dst_maxburst);
	chan_config |= (burst_cfg << 8);

	chan_config |= MV_DMA_OPERATION_MODE_IN_DESC;

#if defined(__BIG_ENDIAN)
	chan_config |= MV_DMA_DESCRIPTOR_SWAP;
#else
	chan_config &= ~MV_DMA_DESCRIPTOR_SWAP;
#endif

	writel(chan_config, MV_CH_REG(mv_chan, MV_DMA_L_CH_CFG));
}

static int mv_dma_slave_config(struct dma_chan *chan,
			     struct dma_slave_config *config)
{
	struct device *dev = chan->device->dev;
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(to_virt_chan(chan));

	dev_info(dev, "%s, dir=%d, addr_cfg=%d\n", __func__,
			config->direction, mv_chan->addr_cfg );

	mv_dma_chan_config(mv_chan, config);

	mv_chan->src_dev_addr = config->src_addr;
	mv_chan->dst_dev_addr = config->dst_addr;
	return 0;
}

/* Right from mv_xor driver. For now, copy-paste, but might disapear if we
 * decide to merge this with mv_xor. This needs refacto.*/
static void mv_dma_configure_window(struct mv_dma_device *mv_dma_dev)
{
	struct mv_dma_chan *mv_chan;
	u32 win_enable = 0;
	int i;

	for (i = 0; i < 8; i++) {
		writel(0, MV_WIN_REG(mv_dma_dev, i, MV_DMA_H_WIN_BASE_ADDR));
		writel(0, MV_WIN_REG(mv_dma_dev, i, MV_DMA_H_WIN_SIZE_MASK));
		if (i < 4)
			writel(0, MV_WIN_REG(mv_dma_dev, i, MV_DMA_H_WIN_HIGH_ADDR_REMAP));
	}
	/*
	 * For Armada3700 open default 4GB Mbus window. The dram
	 * related configuration are done at AXIS level.
	 */
	writel(0xffff0000, MV_WIN_REG(mv_dma_dev, 0, MV_DMA_H_WIN_SIZE_MASK));

	win_enable |= 1;
	win_enable |= 3 << 16;

	for (i = 0; i < mv_dma_dev->nr_channels; i++) {
		mv_chan = &mv_dma_dev->channels[i];
		writel(win_enable, MV_CH_HREG(mv_chan, MV_DMA_H_CH_WINDOW_CTRL));
		writel(0, MV_CH_HREG(mv_chan, MV_DMA_H_CH_ADDR_OVERRIDE_CTRL));
	}
}

static const struct of_device_id mv_dma_dt_ids[] = {
	{
		.compatible = "marvell,armada-3700-dma",
	},
	{},
};

/* Parse the number of supported channels from DT */
static int mv_dma_of_parse_channels(struct mv_dma_device *mv_dma_dev)
{
	int ret;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct device *dev = dma_dev->dev;

	ret = of_property_read_u32(dev->of_node, "dma-channels",
					&mv_dma_dev->nr_channels);
	if (ret) {
		dev_err(dev, "failed to read dma-channels\n");
		return ret;
	}

	if (mv_dma_dev->nr_channels > MV_DMA_MAX_CHANNELS) {
		dev_err(dev, "Invalid number of channels in DT, "
				"declared %d channels, max number is %d\n",
				mv_dma_dev->nr_channels, MV_DMA_MAX_CHANNELS);
		return ret;
	}

	return 0;
}

static int mv_dma_alloc_channel(struct mv_dma_device *mv_dma_dev, int chan_id)
{
	struct mv_dma_chan *mv_chan;
	struct device *dev = mv_dma_dev->dma_dev.dev;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;

	mv_chan = &mv_dma_dev->channels[chan_id];

	vchan_init(&mv_chan->vchan, dma_dev);

	mv_chan->mv_dma_dev = mv_dma_dev;
	mv_chan->mv_chan_id = chan_id;
	mv_chan->vchan.desc_free = mv_dma_free_sw_desc;

	tasklet_init(&mv_chan->irq_tasklet, mv_dma_tasklet, (unsigned long)
		     mv_chan);

	mv_chan->pool = dma_pool_create("DMA pool", dev, 
					sizeof(struct mv_dma_desc),
					MV_DMA_SLOT_SIZE, 0);
	if (!mv_chan->pool)
		return -ENOMEM;

	mv_chan->irq = platform_get_irq(mv_dma_dev->pdev, chan_id);
	if (!mv_chan->irq) {
		dev_err(dev, "Error getting IRQ for chan %d\n",
							chan_id);
		goto err_irq;
	}

	if (request_irq(mv_chan->irq, mv_dma_interrupt_handler,
				0, "DMA-irq", mv_chan)) {
		dev_err(dev, "Error requesting IRQ for chan %d\n", chan_id);
		goto err_irq;
	}

	dev_info(dev, "Channel %d alloc successfull (%pK)\n", chan_id, mv_chan);

	return 0;

err_irq:

	dma_pool_destroy(mv_chan->pool);
	return -EINVAL;
}

static int mv_dma_init_channels(struct mv_dma_device *mv_dma_dev)
{
	int chan_id, ret;

	/* Get number of declared channels in DT */
	ret = mv_dma_of_parse_channels(mv_dma_dev);
	if (ret)
		return ret;

	for (chan_id = 0; chan_id < mv_dma_dev->nr_channels; chan_id++) {
		/* Initialize a channel */
		ret = mv_dma_alloc_channel(mv_dma_dev, chan_id);
		if (ret)
			return ret;
	}

	return 0;
}

/* Called to request channels directly from DT.
 * Devices requesting a slave channel are expected to provide 1 parameter
 * in the 'dmas' entry, which is the device type :
 *   	- 0 : DDR device type, for mem2mem slaves
 *   	- 1 : PCIe
 *   	- 2 : SPI
 *   	- 3 : UART2
 * Since the engine does not support dev2dev accesses, we always assume that
 * the other end of the transfer is DDR.
 *
 * Also, dev2mem channels are half duplex for SPI and UART2.
 *
 * The transfer direction is set when the device configures the channel.
 * */
struct dma_chan *mv_dma_of_xlate_chan(struct of_phandle_args *dma_spec,
						 struct of_dma *ofdma)
{
	struct mv_dma_device *mv_dma_dev = ofdma->of_dma_data;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct mv_dma_chan *mv_chan;
	struct dma_chan *chan;

	if (!mv_dma_dev || dma_spec->args_count != 1)
		return NULL;

	/* It doesn't matter which channel we pick */
	chan = dma_get_any_slave_channel(dma_dev);
	if (!chan)
		return NULL;

	mv_chan = to_mv_dma_chan(to_virt_chan(chan));

	mv_chan->addr_cfg = dma_spec->args[0];
	if (mv_chan->addr_cfg > 3) {
		dev_err(dma_dev->dev, "Invalid cell value in DT : %d\n",
				mv_chan->addr_cfg);
		return NULL;
	}

	return chan;
}

/* Register a mv_dma_device to the underlying frameworks */
static int mv_dma_device_register(struct mv_dma_device *mv_dma_dev)
{
	int ret;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct device *dev = dma_dev->dev;

	/* Register the device in the framework */
	ret = dma_async_device_register(dma_dev);
	if (ret) {
		dev_info(dev, "Unable to register controller to MDA "
				"Framework\n");
		return ret;
	}

	/* Register the device in DT DMA framework, so that slaves can find us */
	ret = of_dma_controller_register(dev->of_node,
				mv_dma_of_xlate_chan, mv_dma_dev);
	if (ret) {
		dev_info(dev, "Unable to register controller to OF\n");
		return ret;
	}
	
	return 0;
}

/* Perform all the controller init */
static int mv_dma_init_controller(struct mv_dma_device *mv_dma_dev)
{
	mv_dma_configure_window(mv_dma_dev);

	return 0;
}

static int mv_dma_probe(struct platform_device *pdev)
{
	struct mv_dma_device *mv_dma_dev;
	struct dma_device *dma_dev;
	struct resource *res;
	int ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	/* Allocate useful resources */
	mv_dma_dev = devm_kzalloc(&pdev->dev, sizeof(*mv_dma_dev), GFP_KERNEL);
	if (!mv_dma_dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	mv_dma_dev->base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!mv_dma_dev->base)
		return -EBUSY;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	mv_dma_dev->high_base = devm_ioremap(&pdev->dev, res->start,
					     resource_size(res));
	if (!mv_dma_dev->high_base)
		return -EBUSY;

	platform_set_drvdata(pdev, mv_dma_dev);
	mv_dma_dev->pdev = pdev;

	dma_dev = &mv_dma_dev->dma_dev;

	/* Initialize struct dma_device */
	dma_cap_set(DMA_SLAVE, dma_dev->cap_mask);

	/* Register all callbacks */
	dma_dev->device_alloc_chan_resources = mv_dma_alloc_chan_resources;
	dma_dev->device_free_chan_resources = mv_dma_free_chan_resources;
	dma_dev->device_issue_pending = mv_dma_issue_pending;
	dma_dev->device_prep_slave_sg = mv_dma_prep_slave_sg;
	dma_dev->device_tx_status = mv_dma_tx_status;
	dma_dev->device_config = mv_dma_slave_config;

	dma_dev->src_addr_widths = MV_DMA_BUSWIDTH;
	dma_dev->dst_addr_widths = MV_DMA_BUSWIDTH;
	dma_dev->directions = BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV);

	dma_dev->dev = &pdev->dev;

	INIT_LIST_HEAD(&dma_dev->channels);

	/* create channels */
	ret = mv_dma_init_channels(mv_dma_dev);
	if (ret)
		goto err;

	/* Register to all underlying frameworks */
	ret = mv_dma_device_register(mv_dma_dev);
	if (ret)
		goto err;

	/* Now that everything is setup and register, configure the controller */
	ret = mv_dma_init_controller(mv_dma_dev);
	if(ret)
		goto err;

	dev_info(&pdev->dev, "%s complete\n", __func__);
	return 0;
	/* Configure the window */

err:
	/* TODO : cleanup / deinit */
	return 0;
}

static struct platform_driver mv_dma_driver = {
	.probe = mv_dma_probe,
	.driver = {
		.name = "mv_dma",
		.of_match_table = of_match_ptr(mv_dma_dt_ids),
	},
};

builtin_platform_driver(mv_dma_driver);

