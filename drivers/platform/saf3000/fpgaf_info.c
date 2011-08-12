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
#include <saf3000/saf3000.h>

/*
 * driver FPGAF INFO
 */
 
struct fpgaf {
	u16 ident;
	u16 version;
	u16 res1[6];
	u16 reset;
	u16 res2[7];
	u16 it_mask;
	u16 it_pend;
	u16 it_ack;
	u16 it_ctr;
	u16 res3[4];
	u16 alrm_in;
	u16 alrm_out;
	u16 res4[6];
	u16 fonc_gen;
	u16 addr;
	u16 res5[6];
	u16 pll_ctr;
	u16 pll_status;
	u16 pll_src;
	u16 res6[5];
	u16 net_ref;
	u16 etat_ref;
	u16 res7[6];
	u16 syn_h110;
	u16 res8[7];
	u16 test;
};

struct fpgaf_info_data {
	struct fpgaf __iomem *fpgaf;
	struct device *dev;
	struct device *infos;
};

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))

static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 version = in_be16(&fpgaf->version);
	
	return snprintf(buf, PAGE_SIZE, "%X.%X.%X.%X\n",EXTRACT(version,12,4), EXTRACT(version,8,4), 
			EXTRACT(version,4,4), EXTRACT(version,0,4));
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static ssize_t fs_attr_board_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);

	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,0,8));
}
static DEVICE_ATTR(board, S_IRUGO, fs_attr_board_show, NULL);

static ssize_t fs_attr_rack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);
	
	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,4,4));
}
static DEVICE_ATTR(rack, S_IRUGO, fs_attr_rack_show, NULL);

static ssize_t fs_attr_slot_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);
	
	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,0,4));
}
static DEVICE_ATTR(slot, S_IRUGO, fs_attr_slot_show, NULL);

static int __devinit fpgaf_info_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct fpgaf_info_data *data;
	struct class *class;
	struct device *infos;
	struct fpgaf *fpgaf;
	int ret;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	fpgaf = of_iomap(np, 0);
	if (fpgaf == NULL) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->fpgaf = fpgaf;
	
	class = saf3000_class_get();
		
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "infos");
	dev_set_drvdata(infos, data);
	data->infos = infos;
	
	if ((ret=device_create_file(dev, &dev_attr_version))
			|| (ret=device_create_file(infos, &dev_attr_board))
			|| (ret=device_create_file(infos, &dev_attr_rack))
			|| (ret=device_create_file(infos, &dev_attr_slot))) {
		goto err_unfile;
	}
	dev_info(dev,"driver MCR3000_2G FPGAF INFO added.\n");
	
	return 0;

err_unfile:
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(infos, &dev_attr_board);
	device_remove_file(infos, &dev_attr_rack);
	device_remove_file(infos, &dev_attr_slot);
	
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->fpgaf), data->fpgaf = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit fpgaf_info_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(infos, &dev_attr_board);
	device_remove_file(infos, &dev_attr_rack);
	device_remove_file(infos, &dev_attr_slot);
	
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->fpgaf), data->fpgaf = NULL;
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
