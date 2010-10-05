/*
 * opx_gpio.c - MCR3000 GPIO for OPx PINs
 *
 * Authors: Christophe LEROY
 *
 * Copyright (c) 2010  CSSI
 *
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/watchdog.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_spi.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <sysdev/fsl_soc.h>

/*
 * Controlleur GPIO pour les ports OP1 à OP4 du MPC
 */

static spinlock_t opx_gpio_lock;
static struct of_mm_gpio_chip opx_gpio_mm_gc;

#define M8XX_PGCRX_CXOE    0x00000080
#define M8XX_PGCRX_CXRESET 0x00000040

static int opx_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	unsigned int *pgcrx=(unsigned int *)opx_gpio_mm_gc.regs;
	unsigned int reg;
	unsigned int mask;
	unsigned int d31 = gpio&1;
	unsigned int d30 = (gpio>>1)&1;

	reg = in_be32(pgcrx + d30);
	mask = d31^d30 ? M8XX_PGCRX_CXOE:M8XX_PGCRX_CXRESET;
	return reg&mask ? 1:0;
}

static void opx_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	unsigned long flags;
	unsigned int *pgcrx=(unsigned int *)opx_gpio_mm_gc.regs;
	unsigned int reg;
	unsigned int mask;
	unsigned int d31 = gpio&1;
	unsigned int d30 = (gpio>>1)&1;

	spin_lock_irqsave(&opx_gpio_lock, flags);

	reg = in_be32(pgcrx + d30);
	mask = d31^d30 ? M8XX_PGCRX_CXOE:M8XX_PGCRX_CXRESET;
	if (val) reg |= mask;
	else reg &= ~mask;
	out_be32(pgcrx + d30, reg);

	spin_unlock_irqrestore(&opx_gpio_lock, flags);
}

static int opx_gpio_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return -EINVAL;
}

static int opx_gpio_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	opx_gpio_set(gc, gpio, val);
	return 0;
}

static void opx_gpio_save_regs(struct of_mm_gpio_chip *mm_gc)
{
}

static int __devinit opx_gpio_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct of_mm_gpio_chip *mm_gc;
	struct of_gpio_chip *of_gc;
	struct gpio_chip *gc;
	struct device *dev = &ofdev->dev;
	struct device_node *np = ofdev->node;

	dev_info(dev,"Initialisation GPIO OPx\n");
	
	spin_lock_init(&opx_gpio_lock);

	mm_gc = &opx_gpio_mm_gc;
	of_gc = &mm_gc->of_gc;
	gc = &of_gc->gc;

	mm_gc->save_regs = opx_gpio_save_regs;
	of_gc->gpio_cells = 2;
	gc->ngpio = 4;
	gc->direction_input = opx_gpio_dir_in;
	gc->direction_output = opx_gpio_dir_out;
	gc->get = opx_gpio_get;
	gc->set = opx_gpio_set;

	return of_mm_gpiochip_add(np, mm_gc);
}

static int __devexit opx_gpio_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	
	dev_info(dev,"driver for MCR3000 CPLD ChipSelects removed.\n");
	return 0;
}

static const struct of_device_id opx_gpio_match[] = {
	{
		.compatible = "s3k,mcr3000-opx-gpio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, opx_gpio_match);

static struct of_platform_driver opx_gpio_driver = {
	.match_table	= opx_gpio_match,
	.probe		= opx_gpio_probe,
	.remove		= __devexit_p(opx_gpio_remove),
	.driver		= {
		.name	= "opx-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init opx_gpio_init(void)
{
	return of_register_platform_driver(&opx_gpio_driver);
}
subsys_initcall(opx_gpio_init);

MODULE_AUTHOR("Christophe LEROY");
MODULE_DESCRIPTION("Driver for OPx GPIO on MCR3000 ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-opx-gpio");
