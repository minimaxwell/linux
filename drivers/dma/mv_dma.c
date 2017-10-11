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
#include <linux/platform_device.h>

enum mv_dma_type {
	MV_DMA_ARMADA_37XX,
};

static const struct of_device_id mv_dma_dt_ids[] = {
	{
		.compatible = "marvell,armada-3700-dma",
	 	.data = (void *)MV_DMA_ARMADA_37XX,
	},
	{},
};

static int mv_dma_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s\n", __func__);
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

