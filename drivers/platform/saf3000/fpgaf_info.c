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
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/leds.h>
#include <sysdev/fsl_soc.h>
#include <saf3000/saf3000.h>
#include <saf3000/fpgaf.h>

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h> 


/*
 * driver FPGAF INFO
 */
 
struct fpgaf_info_data {
	struct fpgaf __iomem *fpgaf;
	u16 *mezz;
	struct device *dev;
	struct device *infos;
	struct device *hwmon_dev;
	int AL_IN_clear[2];
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

static ssize_t show_name(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	return sprintf(buf, "fpga-f\n");
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);


static ssize_t show_fan(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpga_f = data->fpgaf;
	
	if (fpga_f->alrm_in & 0x08)
		 return sprintf(buf, "0\n");
	else
		 return sprintf(buf, "1000\n");
}

static DEVICE_ATTR(fan1_input, S_IRUGO, show_fan, NULL);

static ssize_t show_fan_alarm(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpga_f = data->fpgaf;
	
	if (fpga_f->alrm_in & 0x08)
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
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpga_f = data->fpgaf;

	switch(nr) {
	case 0:
		if (fpga_f->alrm_in & 0x40)
			return sprintf(buf, "5000\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 1:
		if (fpga_f->alrm_in & 0x80)
			return sprintf(buf, "12000\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 2:
		if (fpga_f->alrm_in & 0x10)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "48000\n");
		break;
	case 3:
		if (fpga_f->alrm_in & 0x20)
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

static ssize_t show_alarm(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpga_f = data->fpgaf;

	switch(nr) {
	case 0:
		if (fpga_f->alrm_in & 0x40)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "1\n");
		break;
	case 1:
		if (fpga_f->alrm_in & 0x80)
			return sprintf(buf, "0\n");
		else
			return sprintf(buf, "1\n");
		break;
	case 2:
		if (fpga_f->alrm_in & 0x10)
			return sprintf(buf, "1\n");
		else
			return sprintf(buf, "0\n");
		break;
	case 3:
		if (fpga_f->alrm_in & 0x20)
			return sprintf(buf, "1\n");
		else
			return sprintf(buf, "0\n");
		break;
	}

	return -EINVAL;
}


static SENSOR_DEVICE_ATTR(in0_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_alarm, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_alarm, S_IRUGO, show_alarm, NULL, 3);

static ssize_t show_label(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;

	switch(nr) {
	case 0:
		return sprintf(buf, "Alimentation 5 Volts\n");
		break;
	case 1:
		return sprintf(buf, "Alimentation CBRAS 12 Volts\n");
		break;
	case 2:
		return sprintf(buf, "Alimentation externe 1 48 Volts\n");
		break;
	case 3:
		return sprintf(buf, "Alimentation externe 2 48 Volts\n");
		break;
	}

	return -EINVAL;
}


static SENSOR_DEVICE_ATTR(in0_label, S_IRUGO, show_label, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_label, S_IRUGO, show_label, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_label, S_IRUGO, show_label, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_label, S_IRUGO, show_label, NULL, 3);

static ssize_t show_intrusion(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(attr);
	int nr = sensor_attr->index;
	struct fpgaf_info_data *data = dev_get_drvdata(dev);
	struct fpgaf *fpga_f = data->fpgaf;

	switch(nr) {
	case 0:
		if (fpga_f->alrm_in & 0x01)
			return sprintf(buf, "%d\n", 
				data->AL_IN_clear[0] ? 0 : 1);
		else
			return sprintf(buf, "0\n");
		break;
	case 1:
		if (fpga_f->alrm_in & 0x02)
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
	struct fpgaf_info_data *data = dev_get_drvdata(dev);

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


static struct attribute *fpgaf_attributes[] = {
	&dev_attr_name.attr,
	&dev_attr_fan1_input.attr,
	&dev_attr_fan1_alarm.attr,
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,
	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
	&sensor_dev_attr_intrusion0_alarm.dev_attr.attr,
	&sensor_dev_attr_intrusion1_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group fpgaf_group = {
	.attrs = fpgaf_attributes,
};


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
	int err, ret;

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

        /* Register sysfs hooks */
        data->hwmon_dev = hwmon_device_register(dev);
        if (IS_ERR(data->hwmon_dev)) {
                err = PTR_ERR(data->hwmon_dev);
                goto err_hwmon;
	}

	err = sysfs_create_group(&dev->kobj, &fpgaf_group);
	if (err)
                goto err_hwmon;
	
	data->AL_IN_clear[0] = data->AL_IN_clear[1] = 0;

	return 0;

err_hwmon:
	led_classdev_unregister(&fpgaf_led_alrm2);
	led_classdev_unregister(&fpgaf_led_alrm1);
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

	hwmon_device_unregister(data->hwmon_dev);
	
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

static struct platform_driver fpgaf_info_driver = {
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
