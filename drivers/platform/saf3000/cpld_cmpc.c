/*
 * cpld_cmpc.c - MCR3000 2G CPLD
 *
 * Authors: Christophe LEROY
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
#include <linux/leds.h>
#include <sysdev/fsl_soc.h>
#include <saf3000/saf3000.h>

/*
 * driver CPLD CMPC
 */
 
struct cpld {
	u16 reset;
	u16 etat;
	u16 cmde;
	u16 idma;
};

struct cpld_cmpc_data {
	struct cpld __iomem *cpld;
	struct device *dev;
};

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))

static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	struct cpld *cpld = data->cpld;
	u16 etat = in_be16(&cpld->etat);
	
	return snprintf(buf, PAGE_SIZE, "%X.%X\n",EXTRACT(etat,12,4), EXTRACT(etat,8,4));
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static void cpld_cmpc_led_pwr_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct device *dev = cdev->dev->parent;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	struct cpld *cpld = data->cpld;
	
	if (brightness) { /* normalement, piloter les 4 couleurs (eteind, vert, rouge, jaune) 
				mais le CPLD ne le permet pas pour l'instant */
		setbits16(&cpld->cmde, 0x8000);
	}
	else {
		clrbits16(&cpld->cmde, 0x8000);
	}
}

static enum led_brightness cpld_cmpc_led_pwr_get(struct led_classdev *cdev)
{
	struct device *dev = cdev->dev->parent;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	struct cpld *cpld = data->cpld;
	
	return in_be16(&cpld->cmde) & 0x8000 ? LED_FULL : LED_OFF;
}

static struct led_classdev cpld_led_pwr = {
	.name = "mcr:multi:pwr",
	.brightness_set = cpld_cmpc_led_pwr_set,
	.brightness_get = cpld_cmpc_led_pwr_get,
};

static int __devinit cpld_cmpc_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct cpld_cmpc_data *data;
	struct cpld *cpld;
	int ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	cpld = of_iomap(np, 0);
	if (cpld == NULL) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->cpld = cpld;
	
	if ((ret=device_create_file(dev, &dev_attr_version))) {
		goto err_unfile;
	}
	led_classdev_register(dev, &cpld_led_pwr);
	dev_info(dev,"driver MCR3000_2G CPLD CMPC added.\n");
	
	return 0;

err_unfile:
	device_remove_file(dev, &dev_attr_version);
	
	iounmap(data->cpld), data->cpld = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit cpld_cmpc_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	
	led_classdev_unregister(&cpld_led_pwr);
	
	device_remove_file(dev, &dev_attr_version);
	
	iounmap(data->cpld), data->cpld = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
	
	dev_info(dev,"driver MCR3000_2G CPLD CMPC removed.\n");
	return 0;
}

static const struct of_device_id cpld_cmpc_match[] = {
	{
		.compatible = "s3k,mcr3000-cpld-cmpc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, cpld_cmpc_match);

static struct of_platform_driver cpld_cmpc_driver = {
	.probe		= cpld_cmpc_probe,
	.remove		= __devexit_p(cpld_cmpc_remove),
	.driver		= {
			  	.name		= "cpld-cmpc",
			  	.owner		= THIS_MODULE,
			  	.of_match_table	= cpld_cmpc_match,
			  },
};

static int __init cpld_cmpc_init(void)
{
	return of_register_platform_driver(&cpld_cmpc_driver);
}
subsys_initcall(cpld_cmpc_init);

MODULE_AUTHOR("F.TRINH THAI & C.LEROY");
MODULE_DESCRIPTION("Driver for CPLD CMPC on MCR3000 2G ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-cpld-cmpc");
