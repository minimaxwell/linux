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

#define WINDOW_BASE(w)		(0x50 + ((w) << 2))
#define WINDOW_SIZE(w)		(0x70 + ((w) << 2))
#define WINDOW_REMAP_HIGH(w)	(0x90 + ((w) << 2))
#define WINDOW_BAR_ENABLE(chan)	(0x40 + ((chan) << 2))
#define WINDOW_OVERRIDE_CTRL(chan)	(0xA0 + ((chan) << 2))

#define MV_DMA_BUSWIDTH	( BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | \
			  BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
			  BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) )

/* All known DMA / XOR controllers have at most 2 channels */
#define MV_DMA_MAX_CHANNELS 2

struct mv_dma_chan {
	struct dma_chan chan;
	unsigned int addr_cfg;
};

struct mv_dma_device {
	struct dma_device	dma_dev;
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

static int mv_dma_slave_config(struct dma_chan *chan,
			     struct dma_slave_config *config)
{
	struct device *dev = chan->device->dev;
	struct mv_dma_chan *mv_chan = to_mv_dma_chan(chan);

	dev_info(dev, "%s, dir=%d, addr_cfg=%d\n", __func__,
			config->direction, mv_chan->addr_cfg );

	/* TODO : 
	 * - Set DA_CFG_CHx depending on direction and addr_cfg
	 * - Set SA_CFG_CHx depending on direction and adr_cfg
	 * - Set data write format to 32bit or 64 bits depending on dir and cfg
	 * - Set DA/SA to point to SPI FIFO / UART2 FIFO (we'll see about PCIe later)
	 * - Set the burts limit
	 * The rest will be handled in prep callback*/

	return 0;
}
/* Right from mv_xor driver. For now, copy-paste, but might disapear if we
 * decide to merge this with mv_xor*/
static void mv_dma_configure_window(struct mv_dma_device *mv_dma_dev)
{
	void __iomem *base = mv_dma_dev->high_base;
	u32 win_enable = 0;
	int i;

	for (i = 0; i < 8; i++) {
		writel(0, base + WINDOW_BASE(i));
		writel(0, base + WINDOW_SIZE(i));
		if (i < 4)
			writel(0, base + WINDOW_REMAP_HIGH(i));
	}
	/*
	 * For Armada3700 open default 4GB Mbus window. The dram
	 * related configuration are done at AXIS level.
	 */
	writel(0xffff0000, base + WINDOW_SIZE(0));
	win_enable |= 1;
	win_enable |= 3 << 16;

	writel(win_enable, base + WINDOW_BAR_ENABLE(0));
	writel(win_enable, base + WINDOW_BAR_ENABLE(1));
	writel(0, base + WINDOW_OVERRIDE_CTRL(0));
	writel(0, base + WINDOW_OVERRIDE_CTRL(1));
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
		list_add_tail(&mv_chan->chan.device_node, &dma_dev->channels);
	}

	return 0;
}

struct dma_chan *mv_dma_of_xlate_chan(struct of_phandle_args *dma_spec,
						 struct of_dma *ofdma)
{
	struct mv_dma_device *mv_dma_dev = ofdma->of_dma_data;
	struct dma_device *dma_dev = &mv_dma_dev->dma_dev;
	struct mv_dma_chan *mv_chan;
	struct dma_chan *chan, *candidate = NULL;

	if (!mv_dma_dev || dma_spec->args_count != 2)
		return NULL;

	list_for_each_entry(chan, &dma_dev->channels, device_node)
		if (chan->chan_id == dma_spec->args[0]) {
			candidate = chan;
			break;
		}

	if (!candidate)
		return NULL;

	mv_chan = to_mv_dma_chan(candidate);
	if (!mv_chan)
		return NULL;

	mv_chan->addr_cfg = dma_spec->args[1];

	return dma_get_slave_channel(candidate);
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

