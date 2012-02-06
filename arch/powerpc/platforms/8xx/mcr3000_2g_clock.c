/*
 * Copyright (C) 2011 CSSI All rights reserved.
 *
 * Author: Christophe Leroy <christophe.leroy@c-s.fr>
 *
 * Implements the clk api defined in include/linux/clk.h
 *
 *    Original based on linux/arch/arm/mach-integrator/clock.c
 *
 *    Copyright (C) 2004 ARM Limited.
 *    Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/io.h>

#include <linux/of_platform.h>
#include <asm/clk_interface.h>

struct clk {
	struct module *owner;
};

struct clk fpga_clk;

static struct clk *fpga_clk_get(struct device *dev, const char *id)
{
	struct clk *clk = ERR_PTR(-ENOENT);

	if (dev == NULL || id == NULL)
		return NULL;

	if (strcmp(id,"fpga") == 0 && try_module_get(fpga_clk.owner)) {
		clk = &fpga_clk; 
	}

	return clk;
}


static void fpga_clk_put(struct clk *clk)
{
	module_put(clk->owner);
}

static int fpga_clk_enable(struct clk *clk)
{
	return 0;
}

static void fpga_clk_disable(struct clk *clk)
{
}

static unsigned long fpga_clk_get_rate(struct clk *clk)
{
	return 0; /* renvoyer la vitesse en HZ */
}

static long fpga_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

static int fpga_clk_set_rate(struct clk *clk, unsigned long rate)
{
	/* prendre et configure la vitesse en HZ */
	return 0;
}

static struct clk_interface fpga_clk_functions = {
	.clk_get		= fpga_clk_get,
	.clk_enable		= fpga_clk_enable,
	.clk_disable		= fpga_clk_disable,
	.clk_get_rate		= fpga_clk_get_rate,
	.clk_put		= fpga_clk_put,
	.clk_round_rate		= fpga_clk_round_rate,
	.clk_set_rate		= fpga_clk_set_rate,
	.clk_set_parent		= NULL,
	.clk_get_parent		= NULL,
};

int __init fpga_clk_init(void)
{
	clk_functions = fpga_clk_functions;
	return 0;
}
