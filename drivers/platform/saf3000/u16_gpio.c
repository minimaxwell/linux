/*
 * Simple Memory-Mapped GPIOs
 *
 * Copyright (c) MontaVista Software, Inc. 2008.
 *
 * Author: Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <asm/prom.h>

struct u16_gpio_chip {
	struct of_mm_gpio_chip mm_gc;
	spinlock_t lock;

	/* shadowed data register to clear/set bits safely */
	u16 data;
	
	/* IRQ associated with Pins when relevant */
	int irq[16];
};

static struct u16_gpio_chip *to_u16_gpio_chip(struct of_mm_gpio_chip *mm_gc)
{
	return container_of(mm_gc, struct u16_gpio_chip, mm_gc);
}

static u16 u16_pin2mask(unsigned int pin)
{
	return 1 << (15 - pin);
}

static int u16_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct of_mm_gpio_chip *mm_gc = to_of_mm_gpio_chip(gc);

	return !!(in_be16(mm_gc->regs) & u16_pin2mask(gpio));
}

static void u16_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	struct of_mm_gpio_chip *mm_gc = to_of_mm_gpio_chip(gc);
	struct u16_gpio_chip *u16_gc = to_u16_gpio_chip(mm_gc);
	unsigned long flags;

	spin_lock_irqsave(&u16_gc->lock, flags);

	if (val)
		u16_gc->data |= u16_pin2mask(gpio);
	else
		u16_gc->data &= ~u16_pin2mask(gpio);

	out_be16(mm_gc->regs, u16_gc->data);

	spin_unlock_irqrestore(&u16_gc->lock, flags);
}

static int u16_gpio_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return 0;
}

static int u16_gpio_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	u16_gpio_set(gc, gpio, val);
	return 0;
}

static void u16_gpio_save_regs(struct of_mm_gpio_chip *mm_gc)
{
	struct u16_gpio_chip *u16_gc = to_u16_gpio_chip(mm_gc);

	u16_gc->data = in_be16(mm_gc->regs);
}

static int u16_gpio_to_irq(struct gpio_chip *gc, unsigned int gpio)
{
	struct of_mm_gpio_chip *mm_gc = to_of_mm_gpio_chip(gc);
	struct u16_gpio_chip *u16_gc = to_u16_gpio_chip(mm_gc);

	return u16_gc->irq[gpio] ? u16_gc->irq[gpio] : -ENXIO;
}

static int __init u16_gpiochip_add(struct device_node *np)
{
	int ret;
	struct u16_gpio_chip *u16_gc;
	struct of_mm_gpio_chip *mm_gc;
	struct gpio_chip *gc;
	int i;

	u16_gc = kzalloc(sizeof(*u16_gc), GFP_KERNEL);
	if (!u16_gc)
		return -ENOMEM;

	spin_lock_init(&u16_gc->lock);

	for (i=0; i<16 ;i++) {
		u16_gc->irq[i] = irq_of_parse_and_map(np, i);
	}

	mm_gc = &u16_gc->mm_gc;
	gc = &mm_gc->gc;

	mm_gc->save_regs = u16_gpio_save_regs;
	gc->ngpio = 16;
	gc->direction_input = u16_gpio_dir_in;
	gc->direction_output = u16_gpio_dir_out;
	gc->get = u16_gpio_get;
	gc->set = u16_gpio_set;
	gc->to_irq = u16_gpio_to_irq;

	ret = of_mm_gpiochip_add(np, mm_gc);
	if (ret)
		goto err;
	return 0;
err:
	kfree(u16_gc);
	return ret;
}

void __init u16_gpiochip_init(const char *compatible)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, compatible) {
		int ret;
		struct resource r;

		ret = of_address_to_resource(np, 0, &r);
		if (ret)
			goto err;

		switch (resource_size(&r)) {
		case 2:
			ret = u16_gpiochip_add(np);
			if (ret)
				goto err;
			break;
		default:
			ret = -ENOSYS;
			goto err;
		}
		continue;
err:
		pr_err("%s: registration failed, status %d\n",
		       np->full_name, ret);
	}
}
