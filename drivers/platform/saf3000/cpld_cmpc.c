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
 
#define COLOUR_NONE	0x0000
#define COLOUR_RED	0x1000
#define COLOUR_GREEN	0x8000
#define COLOUR_ORANGE	(COLOUR_RED | COLOUR_GREEN)
#define COLOUR_MASK	(COLOUR_RED | COLOUR_GREEN | COLOUR_ORANGE)
 
struct cpld {
	u16 reset;
	u16 etat;
	u16 cmde;
	u16 idma;
};

struct cpld_cmpc_data {
	struct cpld __iomem *cpld;
	struct device *dev;
	unsigned short colour, colour_blink;
	int blink;
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

static inline int colour_val(const char *buf)
{
	int ret = -1;
	if (sysfs_streq(buf,"none")) ret = COLOUR_NONE;
	else if (sysfs_streq(buf,"red")) ret = COLOUR_RED;
	else if (sysfs_streq(buf,"orange")) ret = COLOUR_ORANGE;
	else if (sysfs_streq(buf,"green")) ret = COLOUR_GREEN;
	return ret;
}

static inline const char *colour_text(int colour)
{
	char *ret="";
	if (colour == COLOUR_NONE) ret = "none";
	else if (colour == COLOUR_RED) ret = "red";
	else if (colour == COLOUR_ORANGE) ret = "orange";
	else if (colour == COLOUR_GREEN) ret = "green";
	return ret;
}

static ssize_t fs_attr_colour_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_cmpc_data *data = dev_get_drvdata(dev->parent);
	int colour = data->colour;
	int l=0;

	if (colour == COLOUR_RED)
		l += snprintf(buf+l, PAGE_SIZE-l, "[red] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "red ");

	if (colour == COLOUR_ORANGE)
		l += snprintf(buf+l, PAGE_SIZE-l, "[orange] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "orange ");

	if (colour == COLOUR_GREEN)
		l += snprintf(buf+l, PAGE_SIZE-l, "[green] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "green ");
	
	l += snprintf(buf+l, PAGE_SIZE-l, "\n");
	
	return l;
}

static ssize_t fs_attr_colour_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct cpld_cmpc_data *data = dev_get_drvdata(dev->parent);
	struct cpld *cpld = data->cpld;
	int colour = colour_val(buf);
	
	if (colour != -1 && colour != COLOUR_NONE) {
		data->colour = colour;
		if (in_be16(&cpld->cmde) & COLOUR_MASK) /* si led allumee on applique la nouvelle couleur */
			clrsetbits_be16(&cpld->cmde, colour ^ COLOUR_MASK, colour);
	}
	return count;
}
static DEVICE_ATTR(colour, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_colour_show, fs_attr_colour_store);

static ssize_t fs_attr_colour_blink_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cpld_cmpc_data *data = dev_get_drvdata(dev->parent);
	int colour = data->colour_blink;
	int l=0;

	if (colour == COLOUR_NONE)
		l += snprintf(buf+l, PAGE_SIZE-l, "[none] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "none ");

	if (colour == COLOUR_RED)
		l += snprintf(buf+l, PAGE_SIZE-l, "[red] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "red ");

	if (colour == COLOUR_ORANGE)
		l += snprintf(buf+l, PAGE_SIZE-l, "[orange] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "orange ");

	if (colour == COLOUR_GREEN)
		l += snprintf(buf+l, PAGE_SIZE-l, "[green] ");
	else 
		l += snprintf(buf+l, PAGE_SIZE-l, "green ");
	
	l += snprintf(buf+l, PAGE_SIZE-l, "\n");
	
	return l;
}

static ssize_t fs_attr_colour_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct cpld_cmpc_data *data = dev_get_drvdata(dev->parent);
	int colour = colour_val(buf);
	
	if (colour != -1) {
		data->colour_blink = colour;
	}
	return count;
}
static DEVICE_ATTR(colour_blink, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_colour_blink_show, fs_attr_colour_blink_store);

static void cpld_cmpc_led_pwr_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct device *dev = cdev->dev->parent;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	struct cpld *cpld = data->cpld;
	
	if (brightness) {
		int colour = data->colour;
		clrsetbits_be16(&cpld->cmde, colour ^ COLOUR_MASK, colour);
	}
	else if (data->blink) {
		int colour = data->colour_blink;
		clrsetbits_be16(&cpld->cmde, colour ^ COLOUR_MASK, colour);
	}
	else {
		clrbits16(&cpld->cmde, COLOUR_MASK);
	}
}

static enum led_brightness cpld_cmpc_led_pwr_get(struct led_classdev *cdev)
{
	struct device *dev = cdev->dev->parent;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	struct cpld *cpld = data->cpld;
	enum led_brightness ret;
	
	if (data->blink && (in_be16(&cpld->cmde) & COLOUR_MASK) == data->colour_blink)
		ret = LED_OFF;
	else if (in_be16(&cpld->cmde) & COLOUR_MASK)
		ret = LED_FULL;
	else 
		ret = LED_OFF;
	return ret;
}

static int cpld_cmpc_led_blink(struct led_classdev *cdev, unsigned long *delay_on, unsigned long *delay_off)
{
	struct device *dev = cdev->dev->parent;
	struct cpld_cmpc_data *data = dev_get_drvdata(dev);
	
	if (*delay_on || *delay_off) data->blink = 1;
	else data->blink = 0;
	
	return -EINVAL;
}

static struct led_classdev cpld_led_pwr = {
	.name = "mcr:multi:pwr",
	.brightness_set = cpld_cmpc_led_pwr_set,
	.brightness_get = cpld_cmpc_led_pwr_get,
	.default_trigger = "default_on",
	.blink_set = cpld_cmpc_led_blink,
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
	data->colour = COLOUR_RED;
	data->colour_blink = COLOUR_NONE;

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
	if ((ret=device_create_file(cpld_led_pwr.dev, &dev_attr_colour)) ||
			(ret=device_create_file(cpld_led_pwr.dev, &dev_attr_colour_blink))) {
		goto err_unregister;
	}
	dev_info(dev,"driver MCR3000_2G CPLD CMPC added.\n");
	
	return 0;

err_unregister:
	device_remove_file(cpld_led_pwr.dev, &dev_attr_colour);
	device_remove_file(cpld_led_pwr.dev, &dev_attr_colour_blink);
	led_classdev_unregister(&cpld_led_pwr);
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
	
	device_remove_file(cpld_led_pwr.dev, &dev_attr_colour);
	device_remove_file(cpld_led_pwr.dev, &dev_attr_colour_blink);
	
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
