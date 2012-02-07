/*
 * gpio_miae.c - MIAE 2G GPIO
 *
 * Authors: Patrick VASSEUR
 * Copyright (c) 2012  CSSI
 *
 */

#include <linux/fs.h>
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
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_spi.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <saf3000/saf3000.h>
#include <asm/prom.h>
#include <asm/irq.h>


/*
 * 1.0 - 06/01/2012 - creation driver pour MIAE
 */
#define GPIO_VERSION		"1.0"
#define GPIO_AUTHOR		"VASSEUR Patrick - Janvier 2012"


#define NBMAX 32

struct gpio_data {
	struct device	*dev;
	struct device	*gpios_infos;
	int		gpios_gpio[NBMAX];
	char		*name_gpio[NBMAX];
	int		gpios_ngpios;
};

static const struct of_device_id gpios_match[];
static int __devinit gpios_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	int ret;
	struct class *class;
	int i;
	const char *names;
	int len_names = 0;
	struct gpio_data *data;

	match = of_match_device(gpios_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	dev_info(dev, "GPIOs driver probe.\n");
	
	data = kzalloc(sizeof *data, GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	data->gpios_ngpios = of_gpio_count(np);
	
	names = of_get_property(np, "names", &len_names);
	
	if (data->gpios_ngpios < 1) {
		dev_err(dev, "missing GPIO definition in device tree\n");
		ret = -EINVAL;
		goto err_map;
	}
	if (data->gpios_ngpios > NBMAX) data->gpios_ngpios = NBMAX;

	class = saf3000_class_get();
	data->gpios_infos = device_create(class, dev, MKDEV(0, 0), NULL, "gpio");

	for (i = 0; ((i < data->gpios_ngpios) && (len_names > 0)); i++) {
		enum of_gpio_flags flags;
		int gpio;
		int len = strlen(names) + 1;
		
		gpio = of_get_gpio_flags(np, i, &flags);
		ret = gpio_request_one(gpio, flags, dev_driver_string(dev));
		if (ret) {
			dev_err(dev, "can't request gpio n %d => %d: %d\n", i, gpio, ret);
			continue;
		}

		ret = gpio_export(gpio, 0);
		if (ret) {
			dev_err(dev,"can't export gpio %d: %d\n", gpio, ret);
			continue;
		}

		gpio_sysfs_set_active_low(gpio, 1);
		ret = gpio_export_link(data->gpios_infos, names, gpio);
//		dev_info(dev,"Retour gpio_export_link du GPIO %d (id = %d) = %d\n", i, gpio, ret);

		data->name_gpio[i] = kzalloc(len, GFP_KERNEL);
		if (data->name_gpio[i] == NULL) {
			dev_err(dev,"can't attribute name gpio %d\n", i);
			continue;
		}
		memcpy(data->name_gpio[i], names, len);
		dev_info(dev,"Name gpio = %s\n", data->name_gpio[i]);
		
		data->gpios_gpio[i] = gpio;
		
		names += len;
		len_names -= len;
	}
	
	dev_info(dev,"GPIOs driver added.\n");
	return 0;

err_map:
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit gpios_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct gpio_data *data = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < data->gpios_ngpios; i++) {
		gpio_free(data->gpios_gpio[i]);
	}
	device_unregister(data->gpios_infos);
	dev_set_drvdata(dev, NULL);
	kfree(data);
		
	dev_info(dev,"GPIOs driver removed.\n");
	return 0;
}

static const struct of_device_id gpios_match[] = {
	{
		.compatible = "s3k,gpios-appli",
	},
	{},
};
MODULE_DEVICE_TABLE(of, gpios_match);

static struct platform_driver gpios_driver = {
	.probe		= gpios_probe,
	.remove		= __devexit_p(gpios_remove),
	.driver		= {
		.name	= "gpios",
		.owner	= THIS_MODULE,
		.of_match_table	= gpios_match,
	},
};

static int __init gpios_init(void)
{
	return platform_driver_register(&gpios_driver);
}
module_init(gpios_init);

static void __exit gpios_exit(void)
{
	platform_driver_unregister(&gpios_driver);
}
module_exit(gpios_exit);

MODULE_AUTHOR(GPIO_AUTHOR);
MODULE_VERSION(GPIO_VERSION);
MODULE_DESCRIPTION("Driver for GPIO on MIA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-fpga-m");
