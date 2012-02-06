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
#include <linux/leds.h>
#include <sysdev/fsl_soc.h>
#include <saf3000/saf3000.h>
#include <saf3000/fpgaf.h>

/*
 * driver FPGAF INFO
 */
 
struct fpgaf_info_data {
	struct fpgaf __iomem *fpgaf;
	u16 *mezz;
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

/* adresse lue sur fond panier 0..255 */
static ssize_t fs_attr_addr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);

	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,0,8));
}
static DEVICE_ATTR(addr, S_IRUGO, fs_attr_addr_show, NULL);

/* numero de carte 0..127 */
static ssize_t fs_attr_board_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);

	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,0,7));
}
static DEVICE_ATTR(board, S_IRUGO, fs_attr_board_show, NULL);

/* type de chassis 0..1 */
static ssize_t fs_attr_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);
	
	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,7,1));
}
static DEVICE_ATTR(type, S_IRUGO, fs_attr_type_show, NULL);

/* numero de chassis 0..7 */
static ssize_t fs_attr_rack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);
	
	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,4,3));
}
static DEVICE_ATTR(rack, S_IRUGO, fs_attr_rack_show, NULL);

/* numero d'emplacement dans chassis 0..15 */
static ssize_t fs_attr_slot_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 addr = in_be16(&fpgaf->addr);
	
	return snprintf(buf, PAGE_SIZE, "%d\n",EXTRACT(addr,0,4));
}
static DEVICE_ATTR(slot, S_IRUGO, fs_attr_slot_show, NULL);

static ssize_t fs_attr_mezz_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	u16 *mezz = data->mezz;
	u16 fonc_gen = in_be16(&fpgaf->fonc_gen);
	u16 ident;
	char *carte;
	int l;
	
	if (fonc_gen & 0x8) { /* Mezzanine presente */
		DEFINE_SPINLOCK(lock);
		unsigned long flags;
		int ident2;
		
		/* La carte C4E1 n'a pas de bus de donnees donc on fait une bidouille en utilisant la 
		remanence du bus de donnees pour discriminer la carte C4E1 */
		
		spin_lock_irqsave(&lock, flags);
		out_be16(mezz, 0xaa55);
		ident = in_be16(mezz);
		out_be16(mezz, 0x55aa);
		ident2 = in_be16(mezz);
		spin_unlock_irqrestore(&lock, flags);
		
		if (ident != ident2) { /* C4E1 */
			ident = (1<<5) | 0;
		}
		else {
			ident >>= 8;
		}
	}
	else {
		ident = 0;
	}
	switch (ident) {
	case 0x0:
		carte = "None";
		break;
	case 0x20:
		carte = "C4E1";
		break;
	case 0x21:
		carte = "CAG";
		break;
	default:
		carte = NULL;
		break;
	}
	if (carte) {
		l = snprintf(buf, PAGE_SIZE, "%s\n", carte);
	}
	else {
		l = snprintf(buf, PAGE_SIZE, "Unknown id %d\n", ident);
	}
	return l;
}
static DEVICE_ATTR(mezz, S_IRUGO, fs_attr_mezz_show, NULL);

static ssize_t fs_attr_alrm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	char *mode;
	
	if (in_be16(&fpgaf->alrm_out) & 0x10) {
		mode = "manual";
	}
	else {
		mode = "auto";
	}
	return snprintf(buf, PAGE_SIZE, "%s\n",mode);
}

static ssize_t fs_attr_alrm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	
	if (sysfs_streq(buf, "manual")) {
		setbits16(&fpgaf->alrm_out, 0x10);
	}
	else if (sysfs_streq(buf, "auto")) {
		clrbits16(&fpgaf->alrm_out, 0x10);
	}
	return count;
}
	
static DEVICE_ATTR(alrm, S_IRUGO | S_IWUSR, fs_attr_alrm_show, fs_attr_alrm_store);

static void fpgaf_led_alrm1_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct device *dev = cdev->dev->parent;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	
	if (brightness) {
		clrbits16(&fpgaf->alrm_out, 0x4);
	}
	else {
		setbits16(&fpgaf->alrm_out, 0x4);
	}
}

static enum led_brightness fpgaf_led_alrm1_get(struct led_classdev *cdev)
{
	struct device *dev = cdev->dev->parent;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	
	return in_be16(&fpgaf->alrm_out) & 0x4 ? LED_OFF : LED_FULL;
}

static struct led_classdev fpgaf_led_alrm1 = {
	.name = "mcr:red:alrm1",
	.brightness_set = fpgaf_led_alrm1_set,
	.brightness_get = fpgaf_led_alrm1_get,
};

static void fpgaf_led_alrm2_set(struct led_classdev *cdev, enum led_brightness brightness)
{
	struct device *dev = cdev->dev->parent;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	
	if (brightness) {
		clrbits16(&fpgaf->alrm_out, 0x8);
	}
	else {
		setbits16(&fpgaf->alrm_out, 0x8);
	}
}

static enum led_brightness fpgaf_led_alrm2_get(struct led_classdev *cdev)
{
	struct device *dev = cdev->dev->parent;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpgaf = data->fpgaf;
	
	return in_be16(&fpgaf->alrm_out) & 0x8 ? LED_OFF : LED_FULL;
}

static struct led_classdev fpgaf_led_alrm2 = {
	.name = "mcr:yellow:alrm2",
	.brightness_set = fpgaf_led_alrm2_set,
	.brightness_get = fpgaf_led_alrm2_get,
};

static const struct of_device_id fpgaf_info_match[];
static int __devinit fpgaf_info_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct fpgaf_info_data *data;
	struct class *class;
	struct device *infos;
	struct fpgaf *fpgaf;
	u16 *mezz;
	int ret;

	match = of_match_device(fpgaf_info_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	fpgaf = of_iomap(np, 0);
	if (fpgaf == NULL) {
		dev_err(dev,"of_iomap FPGAF failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->fpgaf = fpgaf;
	
	mezz = of_iomap(np, 1);
	if (mezz == NULL) {
		dev_err(dev,"of_iomap MEZZ failed\n");
		ret = -ENOMEM;
		goto err_unmap0;
	}
	data->mezz = mezz;
	
	class = saf3000_class_get();
		
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "infos");
	dev_set_drvdata(infos, data);
	data->infos = infos;
	
	if ((ret=device_create_file(dev, &dev_attr_version))
			|| (ret=device_create_file(infos, &dev_attr_addr))
			|| (ret=device_create_file(infos, &dev_attr_board))
			|| (ret=device_create_file(infos, &dev_attr_type))
			|| (ret=device_create_file(infos, &dev_attr_rack))
			|| (ret=device_create_file(infos, &dev_attr_slot))
			|| (ret=device_create_file(infos, &dev_attr_mezz))
			|| (ret=device_create_file(infos, &dev_attr_alrm))) {
		goto err_unfile;
	}

	led_classdev_register(dev, &fpgaf_led_alrm1);
	led_classdev_register(dev, &fpgaf_led_alrm2);
	dev_info(dev,"driver MCR3000_2G FPGAF INFO added.\n");
	
	return 0;

err_unfile:
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(infos, &dev_attr_addr);
	device_remove_file(infos, &dev_attr_board);
	device_remove_file(infos, &dev_attr_type);
	device_remove_file(infos, &dev_attr_rack);
	device_remove_file(infos, &dev_attr_slot);
	device_remove_file(infos, &dev_attr_mezz);
	device_remove_file(infos, &dev_attr_alrm);
	
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->mezz), data->mezz = NULL;
err_unmap0:
	iounmap(data->fpgaf), data->fpgaf = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit fpgaf_info_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	led_classdev_unregister(&fpgaf_led_alrm2);
	led_classdev_unregister(&fpgaf_led_alrm1);
	
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(infos, &dev_attr_addr);
	device_remove_file(infos, &dev_attr_board);
	device_remove_file(infos, &dev_attr_type);
	device_remove_file(infos, &dev_attr_rack);
	device_remove_file(infos, &dev_attr_slot);
	device_remove_file(infos, &dev_attr_mezz);
	device_remove_file(infos, &dev_attr_alrm);
	
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->mezz), data->mezz = NULL;
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
	return platform_driver_register(&fpgaf_info_driver);
}
subsys_initcall(fpgaf_info_init);

MODULE_AUTHOR("F.TRINH THAI & C.LEROY");
MODULE_DESCRIPTION("Driver for FPGAF INFO on MCR3000 2G ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-fpgaf-info");
