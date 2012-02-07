/*
 * fpga_cs.c - MCR3000 fpga_cs interface
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

struct fpga_cs_data {
	struct of_mm_gpio_chip mm_gc;
	spinlock_t fpga_cs_lock;
	int prog, init;
	int prog_state;
};

static int fpga_cs_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct of_mm_gpio_chip *mm_gc = to_of_mm_gpio_chip(gc);
	struct fpga_cs_data *data=container_of(mm_gc, struct fpga_cs_data, mm_gc);

	if (gpio == 0) return data->prog_state;
	else if (gpio == 1) return gpio_get_value(data->init);
	else return -EINVAL;
}

static void fpga_cs_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	struct of_mm_gpio_chip *mm_gc = to_of_mm_gpio_chip(gc);
	struct fpga_cs_data *data=container_of(mm_gc, struct fpga_cs_data, mm_gc);
	unsigned long flags;

	spin_lock_irqsave(&data->fpga_cs_lock, flags);
	
	if (gpio == 0) {
		data->prog_state = val;
		if (val == 0) {
			int i;
			
			gpio_set_value(data->prog, 0);
			i=0;
			while (gpio_get_value(data->init)) {
				if (i++>100000) {; /* attente */
					pr_err("timeout1 %s\n",__FUNCTION__);
					break;
				}
			}
			gpio_set_value(data->prog, 1);
			i=0;
			while (!gpio_get_value(data->init)) {
				if (i++>100000) {; /* attente */
					pr_err("timeout2 %s\n",__FUNCTION__);
					break;
				}
			}
		}
	}
	
	spin_unlock_irqrestore(&data->fpga_cs_lock, flags);
}

static int fpga_cs_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return 0;
}

static int fpga_cs_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	fpga_cs_set(gc, gpio, val);
	return 0;
}

static void fpga_cs_save_regs(struct of_mm_gpio_chip *mm_gc)
{
}

static const struct of_device_id fpga_cs_match[];
static int __devinit fpga_cs_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct of_mm_gpio_chip *mm_gc;
	struct gpio_chip *gc;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	int ngpios = of_gpio_count(np);
	int ret;
	struct fpga_cs_data *data;

	match = of_match_device(fpga_cs_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	dev_info(dev,"driver for MCR3000 FPGA Programming ChipSelect initialised\n");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	
	if (ngpios<2) {
		dev_err(dev,"missing GPIO definition in device tree\n");
		ret = -EINVAL;
		goto err_free;
	}

	data->prog = of_get_gpio(np, 0);
	ret = gpio_request_one(data->prog, GPIOF_OUT_INIT_LOW, dev_driver_string(dev));
	if (ret) {
		dev_err(dev, "can't request gpio PROG: err %d\n", ret);
		goto err_free;
	}
	
	data->init = of_get_gpio(np, 1);
	ret = gpio_request_one(data->init, GPIOF_IN, dev_driver_string(dev));
	if (ret) {
		dev_err(dev, "can't request gpio INIT: err %d\n", ret);
		goto err_gpio;
	}
	
	spin_lock_init(&data->fpga_cs_lock);

	mm_gc = &data->mm_gc;
	gc = &mm_gc->gc;

	mm_gc->save_regs = fpga_cs_save_regs;
	gc->ngpio = 2;
	gc->direction_input = fpga_cs_dir_in;
	gc->direction_output = fpga_cs_dir_out;
	gc->get = fpga_cs_get;
	gc->set = fpga_cs_set;

	return of_mm_gpiochip_add(np, mm_gc);

err_gpio:
	gpio_free(data->prog);
err_free:
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit fpga_cs_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpga_cs_data *data = dev_get_drvdata(dev);
	
	if (gpio_is_valid(data->prog)) gpio_free(data->prog);
	if (gpio_is_valid(data->init)) gpio_free(data->init);
	dev_set_drvdata(dev, NULL);
	if (data) kfree(data);
	dev_info(dev,"driver for MCR3000 FPGA Programmation ChipSelect removed.\n");
	return 0;
}

static const struct of_device_id fpga_cs_match[] = {
	{
		.compatible = "s3k,mcr3000-fpga-cs",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fpga_cs_match);

static struct platform_driver fpga_cs_driver = {
	.probe		= fpga_cs_probe,
	.remove		= __devexit_p(fpga_cs_remove),
	.driver		= {
		.name	= "fpga-cs",
		.owner	= THIS_MODULE,
		.of_match_table	= fpga_cs_match,
	},
};

static int __init fpga_cs_init(void)
{
	return platform_driver_register(&fpga_cs_driver);
}
subsys_initcall(fpga_cs_init);

MODULE_AUTHOR("Christophe LEROY");
MODULE_DESCRIPTION("Driver for simulating SPI CS for FPGA programming ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-fpga");
