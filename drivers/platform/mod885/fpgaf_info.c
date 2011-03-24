/*
 * fpgaf_info.c - MCR3000 2G FPGAF information
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
#include <sysdev/fsl_soc.h>

/*
 * driver FPGAF INFO
 */

struct fpgaf_info_data {
	u8 __iomem *version,*mcrid;
	struct device *dev;
};

static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	u8 version = *data->version;
	
	return sprintf(buf,"%X.%X\n",(version>>4)&0xf, version&0xf);
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static ssize_t fs_attr_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return sprintf(buf,"%d\n",id);
}
static DEVICE_ATTR(num, S_IRUGO, fs_attr_num_show, NULL);

static ssize_t fs_attr_chassis_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return sprintf(buf,"%d\n",(id>>4)&0xf);
}
static DEVICE_ATTR(chassis, S_IRUGO, fs_attr_chassis_show, NULL);

static ssize_t fs_attr_carte_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return sprintf(buf,"%d\n",(id>>0)&0xf);
}
static DEVICE_ATTR(carte, S_IRUGO, fs_attr_carte_show, NULL);

static int __devinit fpgaf_info_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct fpgaf_info_data *data;
	int ret;

	data = kmalloc(GFP_KERNEL,sizeof(*data));
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	data->version = of_iomap(np, 0);
	if (!data->version) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->mcrid = of_iomap(np, 1);
	
	if ((ret=device_create_file(dev, &dev_attr_version))) {
		goto err_unfile;
	}
		
	if (data->mcrid && ((ret=device_create_file(dev, &dev_attr_num))
			|| (ret=device_create_file(dev, &dev_attr_chassis))
			|| (ret=device_create_file(dev, &dev_attr_carte)))) {
		goto err_unfile;
	}
	dev_info(dev,"driver MCR3000_2G FPGAF INFO added.\n");
	return 0;

err_unfile:
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(dev, &dev_attr_num);
	device_remove_file(dev, &dev_attr_chassis);
	device_remove_file(dev, &dev_attr_carte);
	
	iounmap(data->version);
	data->version = NULL;
	if (data->mcrid) {
		iounmap(data->mcrid);
	}
	data->mcrid = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit fpgaf_info_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(dev, &dev_attr_num);
	device_remove_file(dev, &dev_attr_chassis);
	device_remove_file(dev, &dev_attr_carte);
	iounmap(data->version);
	data->version = NULL;
	if (data->mcrid) {
		iounmap(data->mcrid);
	}
	data->mcrid = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
	
	dev_info(dev,"driver MCR3000_2G FPGAF INFO removed.\n");
	return 0;
}

static const struct of_device_id fpgaf_info_match[] = {
	{
		.compatible = "s3k,mcr3000-fpgaf-info",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fpgaf_info_match);

static struct of_platform_driver fpgaf_info_driver = {
	.probe		= fpgaf_info_probe,
	.remove		= __devexit_p(fpgaf_info_remove),
	.driver		= {
			  	.name		= "fpgaf-info",
			  	.owner		= THIS_MODULE,
			  	.of_match_table	= fpgaf_info_match,
			  },
};

static int __init fpgaf_info_init(void)
{
	return of_register_platform_driver(&fpgaf_info_driver);
}
subsys_initcall(fpgaf_info_init);

MODULE_AUTHOR("F.TRINH THAI & C.LEROY");
MODULE_DESCRIPTION("Driver for FPGAF INFO on MCR3000 2G ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-fpgaf-info");
