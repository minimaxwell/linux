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

#define MV_DMA_BUSWIDTH	(BIT(DMA_SLAVE_BUSWIDTH_4_BYTES))

struct mv_dma_device {
	struct dma_device	dma_dev;
	void __iomem	*base;
	void __iomem	*high_base;
};

static int mv_dma_alloc_chan_resources(struct dma_chan *chan)
{
	return 0;
}

static void mv_dma_free_chan_resources(struct dma_chan *chan)
{
	return;
}

static void mv_dma_issue_pending(struct dma_chan *chan)
{
	return;
}

static struct dma_async_tx_descriptor *mv_dma_prep_interleaved_dma(
					struct dma_chan *chan,
					struct dma_interleaved_template *xt,
					unsigned long flags)
{
	return NULL;
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

	dma_dev->device_alloc_chan_resources = mv_dma_alloc_chan_resources;
	dma_dev->device_free_chan_resources = mv_dma_free_chan_resources;
	dma_dev->device_issue_pending = mv_dma_issue_pending;
	dma_dev->device_prep_interleaved_dma = mv_dma_prep_interleaved_dma;

	dma_dev->src_addr_widths = MV_DMA_BUSWIDTH;
	dma_dev->dst_addr_widths = MV_DMA_BUSWIDTH;
	dma_dev->directions = BIT(DMA_DEV_TO_MEM) | BIT(DMA_MEM_TO_DEV);

	dma_dev->dev = &pdev->dev;

	INIT_LIST_HEAD(&dma_dev->channels);

	/* Configure parameters from DT */

	/* Register the device in the framework */
	ret = of_dma_controller_register(pdev->dev.of_node,
		of_dma_xlate_by_chan_id, dma_dev);
	if (ret) {
		dev_info(&pdev->dev, "Unable to register controller to OF\n");
		goto err;
	}

	/* Register the device in DT DMA framework, so that slaves can find us */
	ret = dma_async_device_register(dma_dev);
	if (ret) {
		dev_info(&pdev->dev, "Unable to register controller to MDA "
				"Framework\n");
		goto err;
	}

	/* Configure the window */
	mv_dma_configure_window(mv_dma_dev);

err:
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

