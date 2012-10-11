/*
 * mcr3000_hwmon.c - MCR3000 1G HWMON
 *
 * Authors: Jerome CHANTELAUZE
 * Copyright (c) 2012  CSSI
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
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/leds.h>
#include <sysdev/fsl_soc.h>
#include <saf3000/saf3000.h>

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h> 

/*
 * driver CPLD CMPC
 */
 
struct hwmon_cpld_regs {
	u16 cpld_RESET;
	u16 cpld_CMD;
	u16 cpld_EVT;
	u16 cpld_STWD;
};

struct hwmon_fpga_regs {
	u16 fpga_RILED;
};

struct hwmon_data {
	struct hwmon_cpld_regs __iomem *cpld_regs;
	struct hwmon_fpga_regs __iomem *fpga_regs;
	struct device *dev;
	struct device *infos;
	struct device *hwmon_dev;
	int AL_IN_clear[2];
};

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))


static ssize_t show_name(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	return sprintf(buf, "cpld-fpga\n");
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);

static ssize_t show_fan(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct hwmon_fpga_regs *regs = data->fpga_regs;
	
	if (regs->fpga_RILED & 0x40)
		 return sprintf(buf, "0\n");
	else
		 return sprintf(buf, "1000\n");
}


static DEVICE_ATTR(fan1_input, S_IRUGO, show_fan, NULL);

static ssize_t show_fan_alarm(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct hwmon_fpga_regs *regs = data->fpga_regs;
	
	if (regs->fpga_RILED & 0x40)
		 return sprintf(buf, "1\n");
	else
		 return sprintf(buf, "0\n");
}

static DEVICE_ATTR(fan1_alarm, S_IRUGO, show_fan_alarm, NULL);	


static ssize_t show_alim(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct hwmon_cpld_regs *regs = data->cpld_regs;

	switch(nr) {
	case 0:
		if (regs->cpld_STWD & 0x100)
			return sprintf(buf, "1500\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 1:
		if (regs->cpld_STWD & 0x200)
			return sprintf(buf, "2500\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 2:
		if (regs->cpld_STWD & 0x400)
			return sprintf(buf, "5000\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 3:
		if (regs->cpld_EVT & 0x4000)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "48000\n");
		break;
	case 4:
		if (regs->cpld_EVT & 0x8000)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "48000\n");
		break;
	}

	return -EINVAL;
}

#define show_in_offset(offset)				\
static SENSOR_DEVICE_ATTR(in##offset##_input, S_IRUGO,		\
		show_alim, NULL, offset);
show_in_offset(0);
show_in_offset(1);
show_in_offset(2);
show_in_offset(3);
show_in_offset(4);


static ssize_t show_alarm(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct hwmon_cpld_regs *regs = data->cpld_regs;

	switch(nr) {
	case 0:
		if (regs->cpld_STWD & 0x100)
			return sprintf(buf, "1500\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 1:
		if (regs->cpld_STWD & 0x200)
			return sprintf(buf, "2500\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 2:
		if (regs->cpld_STWD & 0x400)
			return sprintf(buf, "5000\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 3:
		if (regs->cpld_EVT & 0x4000)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "48000\n");
		break;
	case 4:
		if (regs->cpld_EVT & 0x8000)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "48000\n");
		break;
	}

	return -EINVAL;
}

static SENSOR_DEVICE_ATTR(in0_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_alarm, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_alarm, S_IRUGO, show_alarm, NULL, 3);
static SENSOR_DEVICE_ATTR(in4_alarm, S_IRUGO, show_alarm, NULL, 4);


static ssize_t show_label(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;

	switch(nr) {
	case 0:
		return sprintf(buf, "Alimentation 1.5 Volts\n");
		break;
	case 1:
		return sprintf(buf, "Alimentation 2.5 Volts\n");
		break;
	case 2:
		return sprintf(buf, "Alimentation 5 Volts\n");
		break;
	case 3:
		return sprintf(buf, "Alimentation externe 1 48 Volts\n");
		break;
	case 4:
		return sprintf(buf, "Alimentation externe 2 48 Volts\n");
		break;
	}

	return -EINVAL;
}


static SENSOR_DEVICE_ATTR(in0_label, S_IRUGO, show_label, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_label, S_IRUGO, show_label, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_label, S_IRUGO, show_label, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_label, S_IRUGO, show_label, NULL, 3);
static SENSOR_DEVICE_ATTR(in4_label, S_IRUGO, show_label, NULL, 3);

static ssize_t show_intrusion(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct hwmon_cpld_regs *regs = data->cpld_regs;

	switch(nr) {
	case 0:
		if (regs->cpld_EVT & 0x1000)
			return sprintf(buf, "%d\n", 
				data->AL_IN_clear[0] ? 0 : 1);
		else
			return sprintf(buf, "0\n");
		break;
	case 1:
		if (regs->cpld_EVT & 0x2000)
			return sprintf(buf, "%d\n", 
				data->AL_IN_clear[1] ? 0 : 1);
		else
			return sprintf(buf, "0\n");
		break;
	}

	return -EINVAL;
}

static ssize_t clear_intrusion(struct device *dev, struct device_attribute *attr,
                const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct hwmon_data *data = dev_get_drvdata(dev);

	switch(nr) {
	case 0:
		data->AL_IN_clear[0] = 
			SENSORS_LIMIT(simple_strtoul(buf, NULL, 10), 0, 1) ? 
			0 : 1;
		return count;
	case 1:
		data->AL_IN_clear[1] = 
			SENSORS_LIMIT(simple_strtoul(buf, NULL, 10), 0, 1) ? 
			0 : 1;
		return count;
	}

	return -EINVAL;
}

static SENSOR_DEVICE_ATTR(intrusion0_alarm, S_IRUGO | S_IWUSR, show_intrusion, 
	clear_intrusion, 0);
static SENSOR_DEVICE_ATTR(intrusion1_alarm, S_IRUGO | S_IWUSR, show_intrusion, 
	clear_intrusion, 1);


static struct attribute *mcr3000_hwmon_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_fan1_input.attr,
	&dev_attr_fan1_alarm.attr,
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
	&sensor_dev_attr_in4_label.dev_attr.attr,
	&sensor_dev_attr_intrusion0_alarm.dev_attr.attr,
	&sensor_dev_attr_intrusion1_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group mcr3000_hwmon_group = {
	.attrs = mcr3000_hwmon_attributes,
};



static const struct of_device_id mcr3000_hwmon_match[];
static int __devinit mcr3000_hwmon_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct hwmon_data *data;
	struct hwmon_cpld_regs *cpld_regs;
	struct hwmon_fpga_regs *fpga_regs;
	int ret;
	struct class *class;
	struct device *infos = NULL;
	int err;
int idx, mask;

	match = of_match_device(mcr3000_hwmon_match, &ofdev->dev);
	if (!match)
		return -EINVAL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	cpld_regs = of_iomap(np, 0);
	if (cpld_regs == NULL) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->cpld_regs = cpld_regs;

	fpga_regs = of_iomap(np, 1);
	if (fpga_regs == NULL) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->fpga_regs = fpga_regs;

	class = saf3000_class_get();
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "mcr3000-hwmon");
	dev_set_drvdata(infos, data);
	data->infos = infos;
	

mask=0x8000;
printk("evt=");
for (idx=0; idx<=15; idx++) {
printk("%d", data->cpld_regs->cpld_EVT & mask ? 1 : 0);
if (idx==7) printk(" ");
mask=mask >> 1;
}
printk("\n");
mask=0x8000;
printk("etat=");
for (idx=0; idx<=15; idx++) {
printk("%d", data->cpld_regs->cpld_STWD & mask ? 1 : 0);
if (idx==7) printk(" ");
mask=mask >> 1;
}
printk("\n");
mask=0x8000;
printk("fpga RILED=");
for (idx=0; idx<=15; idx++) {
printk("%d", data->fpga_regs->fpga_RILED & mask ? 1 : 0);
if (idx==7) printk(" ");
mask=mask >> 1;
}
printk("\n");

        /* Register sysfs hooks */
        data->hwmon_dev = hwmon_device_register(dev);
        if (IS_ERR(data->hwmon_dev)) {
                err = PTR_ERR(data->hwmon_dev);
                goto err_unfile;
	}

	err = sysfs_create_group(&dev->kobj, &mcr3000_hwmon_group);
	if (err)
                goto err_unfile;

	dev_info(dev,"driver MCR3000_1G hawmon added.\n");
	
	data->AL_IN_clear[0] = data->AL_IN_clear[1] = 0;

	return 0;

err_unfile:
	device_unregister(infos);
	iounmap(data->cpld_regs), data->cpld_regs = NULL;
	iounmap(data->fpga_regs), data->fpga_regs = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit mcr3000_hwmon_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct hwmon_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	hwmon_device_unregister(data->hwmon_dev);

	device_unregister(infos);
	iounmap(data->cpld_regs), data->cpld_regs = NULL;
	iounmap(data->fpga_regs), data->fpga_regs = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
	
	dev_info(dev,"driver MCR3000_1G hwmon removed.\n");
	return 0;
}

static const struct of_device_id mcr3000_hwmon_match[] = {
	{
		.compatible = "s3k,mcr3000-hwmon",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mcr3000_hwmon_match);

static struct platform_driver mcr3000_hwmon_driver = {
	.probe		= mcr3000_hwmon_probe,
	.remove		= __devexit_p(mcr3000_hwmon_remove),
	.driver		= {
		.name	= "mcr3000-hwmon",
		.owner	= THIS_MODULE,
		.of_match_table	= mcr3000_hwmon_match,
	},
};

static int __init mcr3000_hwmon_init(void)
{
	return platform_driver_register(&mcr3000_hwmon_driver);
}
subsys_initcall(mcr3000_hwmon_init);

MODULE_AUTHOR("J.CHANTELAUZE");
MODULE_DESCRIPTION("Driver for CPLD CMPC on MCR3000 1G ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-hwmon");
