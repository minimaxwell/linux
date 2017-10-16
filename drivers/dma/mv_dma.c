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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/memory.h>
#include <linux/io.h>
#include <linux/interrupt.h>

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

#define MV_DMA_DESC_OPERATION_XOR       (0 << 24)
#define MV_DMA_DESC_OPERATION_CRC32C    (1 << 24)
#define MV_DMA_DESC_OPERATION_MEMCPY    (2 << 24)

#define MV_DMA_DESC_DMA_OWNED		BIT(31)
#define MV_DMA_DESC_EOD_INT_EN		BIT(31)

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

#define MV_DMA_INTR_MASK_VALUE	(MV_DMA_INT_END_OF_DESC | MV_DMA_INT_END_OF_CHAIN | \
				 MV_DMA_INT_STOPPED     | MV_DMA_INTR_ERRORS)


/* FIXME : 4bytes only for SPI and UART2 */
#define MV_DMA_BUSWIDTH	( BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | \
			  BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
			  BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) )

/* All known DMA / XOR controllers have at most 2 channels */
#define MV_DMA_MAX_CHANNELS 2

/*
 * This structure describes XOR descriptor size 64bytes. The
 * mv_phy_src_idx() macro must be used when indexing the values of the
 * phy_src_addr[] array. This is due to the fact that the 'descriptor
 * swap' feature, used on big endian systems, swaps descriptors data
 * within blocks of 8 bytes. So two consecutive values of the
 * phy_src_addr[] array are actually swapped in big-endian, which
 * explains the different mv_phy_src_idx() implementation.
 */
#if defined(__LITTLE_ENDIAN)
struct mv_xor_desc {
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
struct mv_xor_desc {
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

struct mv_dma_chan {
	struct dma_chan chan;
	unsigned int addr_cfg;
	struct mv_dma_device *mv_dma_dev;
	int mv_chan_id;
	int irq;
};

struct mv_dma_device {
	struct dma_device	dma_dev;
	struct platform_device	*pdev;
	void __iomem	*base;
	void __iomem	*high_base;
	unsigned int 	nr_channels;
	struct mv_dma_chan channels[MV_DMA_MAX_CHANNELS];
};

static struct mv_dma_chan *to_mv_dma_chan(struct dma_chan *chan) {
	return container_of(chan, struct mv_dma_chan, chan);
}

static struct mv_dma_device *to_mv_dma_device(struct dma_device *dma_dev) {
	return container_of(dma_dev, struct mv_dma_device, dma_dev);
}

/* generic register access helper functions */
static inline void mv_dma_write(void __iomem *base, u32 val, u32 reg)
{
	writel_relaxed(val, base + reg);
}

static inline u32 mv_dma_read(void __iomem *base, u32 reg)
{
	return readl_relaxed(base + reg);
}

static inline void mv_dma_write_chan_low(struct mv_dma_chan *chan, u32 val, u32 reg)
{
	mv_dma_write(chan->mv_dma_dev->base, val, reg + (4 * chan->mv_chan_id));
}

static inline u32 mv_dma_read_chan_low(struct mv_dma_chan *chan, u32 reg)
{
	return mv_dma_read(chan->mv_dma_dev->base, reg + (4 * chan->mv_chan_id));
}

static inline void mv_dma_write_chan_high(struct mv_dma_chan *chan, u32 val, u32 reg)
{
	mv_dma_write(chan->mv_dma_dev->high_base, val,
			reg + (4 * chan->mv_chan_id));
}

static inline u32 mv_dma_read_chan_high(struct mv_dma_chan *chan, u32 reg)
{
	return mv_dma_read(chan->mv_dma_dev->high_base, reg + (4 * chan->mv_chan_id));
}

static inline void mv_dma_write_window(void __iomem *base, u32 window,
								  u32 val,
								  u32 reg)
{
	mv_dma_write(base, val, reg + (4 * window));
}

static inline u32 mv_dma_read_window(void __iomem *base, u32 window, u32 reg)
{
	return mv_dma_read(base, reg + (4 * window));
}

static inline void mv_dma_clear_bits(void __iomem *base, u32 mask, u32 reg)
{
	u32 val = readl(base + reg);
	reg &= ~mask;
	writel(val, reg + base);
}

static inline void mv_dma_set_bits(void __iomem *base, u32 mask, u32 reg)
{
	u32 val = readl(base + reg);
	reg |= mask;
	writel(val, reg + base);
}
/* Specific register access helper functions */

/* Get the interrupt cause for this particular channel. The result is right
 * shifted. */
static u32 mv_dma_interrupt_cause(struct mv_dma_chan *chan)
{
	u32 val = mv_dma_read(chan->mv_dma_dev->base, MV_DMA_L_INTR_CAUSE);
	return val >> (16 * chan->mv_chan_id);
}

static void mv_dma_interrupt_clear(struct mv_dma_chan *chan, u32 mask)
{
	mv_dma_clear_bits(chan->mv_dma_dev->base, MV_DMA_L_INTR_CAUSE,
					mask << (16 * chan->mv_chan_id));
}

static int mv_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct device *dev = chan->device->dev;
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static void mv_dma_free_chan_resources(struct dma_chan *chan)
{
	struct device *dev = chan->device->dev;
	dev_info(dev, "%s\n", __func__);
	return;
}

static void mv_dma_issue_pending(struct dma_chan *chan)
{
	struct device *dev = chan->device->dev;
	dev_info(dev, "%s\n", __func__);
	return;
}

static struct dma_async_tx_descriptor *mv_dma_prep_interleaved_dma(
					struct dma_chan *chan,
					struct dma_interleaved_template *xt,
					unsigned long flags)
{
	struct device *dev = chan->device->dev;
	dev_info(dev, "%s\n", __func__);
	return NULL;
}

static enum dma_status mv_dma_tx_status(struct dma_chan *chan,
					    dma_cookie_t cookie,
					    struct dma_tx_state *txstate)
{
	struct device *dev = chan->device->dev;
	dev_info(dev, "%s\n", __func__);
	return DMA_ERROR;
}

/* Returns the burst_param to be configured into the DMA controller. */
static u32 mv_dma_get_burst_param(struct mv_dma_chan *chan, u32 burst_size)
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
		dev_info(chan->chan.device->dev, "Invalid burst size requested "
				": %u, falling back to 8 bytes\n", burst_size);
		return 0;
	}
}

/* Setup the channel config in the controller to use the correct burst limit,
 * and transfer type.
 * We use descriptor-based transfer types.
 * */
static void mv_dma_chan_config(struct mv_dma_chan *chan,
			     struct dma_slave_config *config)
{
	u32 chan_config = 0;

	u32 burst_cfg = mv_dma_get_burst_param(chan, config->src_maxburst);
	chan_config |= (burst_cfg << 4);
	burst_cfg = mv_dma_get_burst_param(chan, config->dst_maxburst);
	chan_config |= (burst_cfg << 8);

	chan_config |= MV_DMA_OPERATION_MODE_IN_DESC;

	mv_dma_write_chan_low(chan, chan_config, MV_DMA_L_CH_CFG);
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
			struct dma_slave_config *config)
{
	struct device *dev = mv_chan->chan.device->dev;
	u16 dev_addr_cfg = 0;
	u16 src_addr_cfg = 0;
	u16 dst_addr_cfg = 0;

	/* For mem2dev or dev2mem, determine the dev address configuration */
	switch(mv_chan->addr_cfg) {
	case MV_DMA_CFG_ADDR_CFG_DDR :
		if (config->direction != DMA_MEM_TO_MEM)
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
	switch (config->direction) {
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
		dev_err(dev, "Unsupported DMA transfer mode %d\n",
						config->direction);
		return -EINVAL;
	}

	/* Write address config to the controller */
	mv_dma_write_chan_low(mv_chan, ((dst_addr_cfg << 16) | src_addr_cfg),
						MV_DMA_L_CH_CFG_ADDR);

	return 0;
err_config:
	dev_err(dev, "Invalid device config (%d) regarding the chosen "
			"direction (%d)", mv_chan->addr_cfg, config->direction);

	return -EINVAL;
}

static int mv_dma_slave_config(struct dma_chan *chan,
			     struct dma_slave_config *config)
{
	struct device *dev = chan->device->dev;
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(chan);

	dev_info(dev, "%s, dir=%d, addr_cfg=%d\n", __func__,
			config->direction, mv_chan->addr_cfg );

	if (mv_dma_chan_config_addr(mv_chan, config))
		return -EINVAL;

	mv_dma_chan_config(mv_chan, config);

	return 0;

}

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

	/*
	tasklet_schedule(&chan->irq_tasklet);
	mv_chan_clear_eoc_cause(chan);
	*/

	/* FIXME Clear everything ? */
	mv_dma_interrupt_clear(chan, intr_cause);

	mv_dma_interrupt_clear(chan, intr_cause);
	return IRQ_HANDLED;
}

/* Right from mv_xor driver. For now, copy-paste, but might disapear if we
 * decide to merge this with mv_xor. This needs refacto.*/
static void mv_dma_configure_window(struct mv_dma_device *mv_dma_dev)
{
	/* FIXME */
	void __iomem *base = mv_dma_dev->high_base;
	u32 win_enable = 0;
	int i;

	for (i = 0; i < 8; i++) {
		mv_dma_write_window(base, i, 0, MV_DMA_H_WIN_BASE_ADDR);
		mv_dma_write_window(base, i, 0, MV_DMA_H_WIN_SIZE_MASK);
		if (i < 4)
			mv_dma_write_window(mv_dma_dev, i, 0, MV_DMA_H_WIN_HIGH_ADDR_REMAP);
	}
	/*
	 * For Armada3700 open default 4GB Mbus window. The dram
	 * related configuration are done at AXIS level.
	 */
	mv_dma_write_window(base, 0, 0xffff0000, MV_DMA_H_WIN_SIZE_MASK);

	win_enable |= 1;
	win_enable |= 3 << 16;

	mv_dma_write_chan_high(&mv_dma_dev->channels[0], win_enable, MV_DMA_H_CH_WINDOW_CTRL);
	mv_dma_write_chan_high(&mv_dma_dev->channels[1], win_enable, MV_DMA_H_CH_WINDOW_CTRL);
	mv_dma_write_chan_high(&mv_dma_dev->channels[0], 0, MV_DMA_H_CH_ADDR_OVERRIDE_CTRL);
	mv_dma_write_chan_high(&mv_dma_dev->channels[1], 0, MV_DMA_H_CH_ADDR_OVERRIDE_CTRL);
}

static const struct of_device_id mv_dma_dt_ids[] = {
	{
		.compatible = "marvell,armada-3700-dma",
	},
	{},
};

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

static int mv_dma_init_channels(struct mv_dma_device *mv_dma_dev)
{
	int chan_id, ret;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct mv_dma_chan *mv_chan;

	ret = mv_dma_of_parse_channels(mv_dma_dev);
	if (ret)
		return ret;

	for (chan_id = 0; chan_id < mv_dma_dev->nr_channels; chan_id++) {
		mv_chan = &mv_dma_dev->channels[chan_id];
		mv_chan->chan.device = dma_dev;
		mv_chan->mv_dma_dev = mv_dma_dev;
		mv_chan->mv_chan_id = chan_id;
		mv_chan->irq = platform_get_irq(mv_dma_dev->pdev, chan_id);
		if (!mv_chan->irq) {
			dev_err(dma_dev->dev, "Error getting IRQ for chan %d\n",
								chan_id);
		} else {
			if (request_irq(mv_chan->irq, mv_dma_interrupt_handler,
						0, "DMA-irq", mv_chan)) {
				dev_err(dma_dev->dev, "Error requesting IRQ "
						"for chan %d\n", chan_id);
			} else {
				dev_info(dma_dev->dev, "Sucessfully requested "
						"IRQ for chan %d\n", chan_id);
			}
		}

		list_add_tail(&mv_chan->chan.device_node, &dma_dev->channels);
	}

	return 0;
}

struct dma_chan *mv_dma_of_xlate_chan(struct of_phandle_args *dma_spec,
						 struct of_dma *ofdma)
{
	struct mv_dma_device *mv_dma_dev = ofdma->of_dma_data;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct mv_dma_chan *mv_chan, *candidate = NULL;
	struct dma_chan *chan;

	if (!mv_dma_dev || dma_spec->args_count != 2)
		return NULL;

	list_for_each_entry(chan, &dma_dev->channels, device_node) {
		mv_chan = to_mv_dma_chan(chan);
		if (mv_chan->mv_chan_id == dma_spec->args[0]) {
			candidate = mv_chan;
			break;
		}
	}

	if (!candidate)
		return NULL;

	mv_chan->addr_cfg = dma_spec->args[1];
	if (mv_chan->addr_cfg > 3) {
		dev_err(dma_dev->dev, "Invalid 2nd cell value in DT : %d\n",
				mv_chan->addr_cfg);
		return NULL;
	}

	return dma_get_slave_channel(&candidate->chan);
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
	dma_dev->device_prep_interleaved_dma = mv_dma_prep_interleaved_dma;
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

