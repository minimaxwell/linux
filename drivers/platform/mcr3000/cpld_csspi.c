/*
 * cpld_csspi.c - MCR3000 cpld_csspi interface
 *
 * Authors: Christophe LEROY
 *	    Patrick VASSEUR
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
 * Controlleur GPIO pour les ChipSelect SPI 
 */

static spinlock_t cpld_csspi_lock;
static struct of_mm_gpio_chip cpld_csspi_mm_gc;

static int cpld_csspi_get(struct gpio_chip *gc, unsigned int gpio)
{
	return gpio == ((in_be16(cpld_csspi_mm_gc.regs) >> 5) & 7) ? 1:0;
}

static void cpld_csspi_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	unsigned long flags;
	unsigned short reg;

	spin_lock_irqsave(&cpld_csspi_lock, flags);

	reg = in_be16(cpld_csspi_mm_gc.regs);
	reg &= ~(7<<5);
	if (val) reg |= (gpio&7) << 5;
	out_be16(cpld_csspi_mm_gc.regs, reg);

	spin_unlock_irqrestore(&cpld_csspi_lock, flags);
}

static int cpld_csspi_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return 0;
}

static int cpld_csspi_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	cpld_csspi_set(gc, gpio, val);
	return 0;
}

static void cpld_csspi_save_regs(struct of_mm_gpio_chip *mm_gc)
{
}

static int __devinit cpld_csspi_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct of_mm_gpio_chip *mm_gc;
	struct of_gpio_chip *of_gc;
	struct gpio_chip *gc;
	struct device *dev = &ofdev->dev;
	struct device_node *np = ofdev->node;

	dev_info(dev,"driver for MCR3000 CPLD ChipSelects initialised\n");
	
	spin_lock_init(&cpld_csspi_lock);

	mm_gc = &cpld_csspi_mm_gc;
	of_gc = &mm_gc->of_gc;
	gc = &of_gc->gc;

	mm_gc->save_regs = cpld_csspi_save_regs;
	of_gc->gpio_cells = 2;
	gc->ngpio = 8;
	gc->direction_input = cpld_csspi_dir_in;
	gc->direction_output = cpld_csspi_dir_out;
	gc->get = cpld_csspi_get;
	gc->set = cpld_csspi_set;

	return of_mm_gpiochip_add(np, mm_gc);
}

static int __devexit cpld_csspi_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	
	dev_info(dev,"driver for MCR3000 CPLD ChipSelects removed.\n");
	return 0;
}

static const struct of_device_id cpld_csspi_match[] = {
	{
		.compatible = "s3k,mcr3000-cpld-csspi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, cpld_csspi_match);

static struct of_platform_driver cpld_csspi_driver = {
	.match_table	= cpld_csspi_match,
	.probe		= cpld_csspi_probe,
	.remove		= __devexit_p(cpld_csspi_remove),
	.driver		= {
		.name	= "cpld-csspi",
		.owner	= THIS_MODULE,
	},
};

static int __init cpld_csspi_init(void)
{
	return of_register_platform_driver(&cpld_csspi_driver);
}
subsys_initcall(cpld_csspi_init);

MODULE_AUTHOR("Christophe LEROY");
MODULE_DESCRIPTION("Driver for CPLD ChipSelects on MCR3000 ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-cpld");
