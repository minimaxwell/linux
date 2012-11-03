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
#include <linux/spi/spi.h>
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


#define NBMAX 64

struct gpio_data {
	struct device	*dev;
	struct device	*gpios_infos;
	struct device	*codecs_infos;
	int		gpios_gpio[NBMAX];
	int		gpios_ngpios;
	unsigned short	type_far;
	unsigned short	type_fav;
};

static ssize_t fs_attr_infos_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_data *data = dev_get_drvdata(dev);
	struct device_node *np = data->dev->of_node;
	int len_info = 0, len = 0, i;
	const char *info = of_get_property(np, "codec", &len_info);
	
	len += snprintf(buf + len, PAGE_SIZE - len, "Codec/Canal/Test/Fonction\n");
	for (i = 0; ((i < 12) && (len_info > 0)); i++) {
		int lg = strlen(info) + 1;
		len += snprintf(buf + len, PAGE_SIZE - len, "  %d   in %d   %2d  %s\n", (i / 4) + 1, (i % 4) + 1, i + 1, info);
		info += lg;
		len_info -= lg;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\nCodec/Canal/Test/Fonction\n");
	for (i = 0; ((i < 12) && (len_info > 0)); i++) {
		int lg = strlen(info) + 1;
		len += snprintf(buf + len, PAGE_SIZE - len, "  %d   out %d  %2d  %s\n", (i / 4) + 1, (i % 4) + 1, i + 1, info);
		info += lg;
		len_info -= lg;
	}
	return len;
}
static DEVICE_ATTR(infos, S_IRUGO, fs_attr_infos_show, NULL);

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
	struct device *infos;
	short *fpgam;

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
	
	fpgam = of_iomap(np, 0);
	if (fpgam == NULL) {
		dev_err(dev, "of_iomap FPGAM failed\n");
		ret = -ENOMEM;
		goto err_map;
	}
	data->type_far = (*fpgam >> 5) & 0x07;
	data->type_fav = (*(fpgam + 1) >> 13) & 0x07;
	dev_info(dev,"Ident face AR : %d, ident face AV : %d\n", data->type_far, data->type_fav);
	iounmap(fpgam);

	if (data->type_far == 1)
		names = of_get_property(np, "names_nvcs", &len_names);
	else
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
		
		if (sysfs_streq(names, "none")) {
			names += len;
			len_names -= len;
			 continue;
		}
		gpio = of_get_gpio_flags(np, i, &flags);
		ret = gpio_request_one(gpio, flags, dev_driver_string(dev));
		if (ret) {
			dev_err(dev, "can't request gpio n %d => %d: %d\n", i, gpio, ret);
			names += len;
			len_names -= len;
			continue;
		}

		ret = gpio_export(gpio, 0);
		if (ret) {
			dev_err(dev,"can't export gpio %d: %d\n", gpio, ret);
			names += len;
			len_names -= len;
			continue;
		}

		gpio_sysfs_set_active_low(gpio, 1);
		ret = gpio_export_link(data->gpios_infos, names, gpio);
//		dev_info(dev,"Retour gpio_export_link du GPIO %d (id = %d) = %d\n", i, gpio, ret);

		dev_info(dev,"Name gpio = %s\n", names);
		
		data->gpios_gpio[i] = gpio;

		names += len;
		len_names -= len;
	}

	infos = device_create(class, dev, MKDEV(0, 0), NULL, "codec");
	dev_set_drvdata(infos, data);
	data->codecs_infos = infos;
	ret = device_create_file(infos, &dev_attr_infos);
	
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
	device_remove_file(data->codecs_infos, &dev_attr_infos);
	device_unregister(data->codecs_infos);
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
