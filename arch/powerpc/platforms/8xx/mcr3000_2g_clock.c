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

/* les différentes vitesses de l'interface UART */
typedef enum
{
	E_FPGA_BD_300 = 1,
	E_FPGA_BD_600,
	E_FPGA_BD_1200,
	E_FPGA_BD_2400,
	E_FPGA_BD_4800,
	E_FPGA_BD_9600,
	E_FPGA_BD_14400,
	E_FPGA_BD_19200,
	E_FPGA_BD_38400,
	E_FPGA_BD_57600,
	E_FPGA_BD_115200,
	E_FPGA__BD_MAX
} tFpgaListeBaud;

struct clk {
	struct module *owner;
	short *fpga_brg;
	int fpga_load;
	int init_brg;
};

struct clk fpga_clk;

static struct clk *fpga_clk_get(struct device *dev, const char *id)
{
	struct clk *clk = ERR_PTR(-ENOENT);

	if (dev == NULL && id == NULL)
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
	unsigned int _Info = 0, rate;
	
	if (fpga_clk.fpga_brg) {
		_Info = in_be16(fpga_clk.fpga_brg);
	}
	
	switch (_Info) {
		case E_FPGA_BD_300: rate = 300; break;
		case E_FPGA_BD_600: rate = 600; break;
		case E_FPGA_BD_1200: rate = 1200; break;
		case E_FPGA_BD_2400: rate = 2400; break;
		case E_FPGA_BD_4800: rate = 4800; break;
		case E_FPGA_BD_9600: rate = 9600; break;
		case E_FPGA_BD_14400: rate = 14400; break;
		case E_FPGA_BD_19200: rate = 19200; break;
		case E_FPGA_BD_38400: rate = 38400; break;
		case E_FPGA_BD_57600: rate = 57600; break;
		case E_FPGA_BD_115200: rate = 115200; break;
		default: rate = 0; break;
	}

	return rate; /* renvoyer la vitesse en HZ */
}

static long fpga_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

static int fpga_clk_set_rate(struct clk *clk, unsigned long rate)
{
	unsigned int _Info;

	if (fpga_clk.fpga_load) {	
		/* prendre et configure la vitesse en HZ */
		switch (rate) {
			case 300: _Info = E_FPGA_BD_300; break;
			case 600: _Info = E_FPGA_BD_600; break;
			case 1200: _Info = E_FPGA_BD_1200; break;
			case 2400: _Info = E_FPGA_BD_2400; break;
			case 4800: _Info = E_FPGA_BD_4800; break;
			case 9600: _Info = E_FPGA_BD_9600; break;
			case 14400: _Info = E_FPGA_BD_14400; break;
			case 19200: _Info = E_FPGA_BD_19200; break;
			case 38400: _Info = E_FPGA_BD_38400; break;
			case 57600: _Info = E_FPGA_BD_57600; break;
			case 115200: _Info = E_FPGA_BD_115200; break;
			default: _Info = 0; break;
		}
	
		if (_Info && fpga_clk.fpga_brg) {
			out_be16(fpga_clk.fpga_brg, _Info);
		}
	}
	else
		fpga_clk.init_brg = rate;
		
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
	struct device_node *np = NULL;
	struct resource res;
	int ret;

	clk_functions = fpga_clk_functions;
	fpga_clk.fpga_load = 0;
	fpga_clk.init_brg = 0;
	fpga_clk.fpga_brg = NULL;
	
	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-fpga");
	if (np == NULL) {
		printk(KERN_ERR "FPGA CLK init: can not find mcr3000-fpga node\n");
	}
	else {
		ret = of_address_to_resource(np, 3, &res);
		if (ret) {
			printk(KERN_ERR "FPGA CLK init: can not remap adresse FPGA\n");
		}
		else {
			fpga_clk.fpga_brg = ioremap(res.start, res.end - res.start + 1);
		}
	}

	return 0;
}

void fpga_clk_set_brg(void)
{
	fpga_clk.fpga_load = 1;
	if (fpga_clk.init_brg) {
		fpga_clk_set_rate(NULL, fpga_clk.init_brg);
		fpga_clk.init_brg = 0;
	}
}
EXPORT_SYMBOL(fpga_clk_set_brg);
