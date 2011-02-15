/*
 * fpga_loader.c - MCR3000 FPGA interface
 *
 * Authors: Christophe LEROY
 *
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
#include <ldb/ldb_gpio.h>

#define STATUS_WAITING 0
#define STATUS_LOADING 1
#define STATUS_LOADED 2
#define STATUS_NOTLOADED 3

#define NB_GPIO 3
#define DIR_GPIO {0,0,1}

#define INITFPGA 0
#define DONEFPGA 1
#define RST_FPGA 2

static struct spi_device *fpga_spi=NULL;

struct fpga_data {
	u8 __iomem *version,*mcrid;
	struct device *dev;
	int gpio[NB_GPIO]; /* initfpga, donefpga, rst_fpga */
	int status;
};

static void fpga_fw_load(const struct firmware *fw, void *context)
{
	struct fpga_data *data=(struct fpga_data*)context;
	struct device *dev = data->dev;
	
	if (!fw) {
		dev_err(dev," fw load failed\n");
	}
	else if (!fpga_spi) {
		dev_err(dev,"driver not attached to spi bus yet, loading impossible\n");
		release_firmware(fw);
	}
	else {
		static struct spi_transfer spit;
		static struct spi_message spim;
		
		dev_info(dev,"received fw data %x size %d\n",(unsigned int)fw->data,fw->size);
		data->status = STATUS_LOADING;
	
		ldb_gpio_set_value(data->gpio[RST_FPGA], 1);

		memset(&spit, 0, sizeof(spit));

		/* on ne sait pas pourquoi, impossible d'utiliser directement fw->data, ca bloque le CPM. On verra plus tard pourquoi */	

		spit.tx_buf=kmalloc(fw->size,GFP_KERNEL);
		if (spit.tx_buf) {
			int ret;
			memcpy((void*)spit.tx_buf, fw->data, fw->size);
			spit.len=fw->size;

			spi_message_init(&spim);
			spi_message_add_tail(&spit, &spim);

			ret = spi_sync(fpga_spi, &spim);
			if (ret != 0) dev_err(dev,"pb spi_sync\n");

			kfree(spit.tx_buf);

			if (ldb_gpio_get_value(data->gpio[INITFPGA])) {
				dev_err(dev,"fw load failed, data not correct\n");
			}
			else if (ldb_gpio_get_value(data->gpio[DONEFPGA])) {
				dev_err(dev,"fw load failed, data not complete\n");
			}
			else {
				u8 version, mcrid;
				dev_info(dev,"fw load ok\n");
				ldb_gpio_set_value(data->gpio[RST_FPGA], 0);
				data->status = STATUS_LOADED;
				version = *data->version;
				dev_info(dev,"fw version %X.%X\n",(version>>4)&0xf, version&0xf);
				if (data->mcrid) {
					mcrid = *data->mcrid;
					dev_info(dev,"adresse carte %X.%X\n",(mcrid>>4)&0xf, mcrid&0xf);
				}
			}
		}
		else {
			dev_err(dev,"kmalloc failed\n");
		}
		release_firmware(fw);
	}
	
	if (data->status != STATUS_LOADED) {
		data->status = STATUS_WAITING;
		if (request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG, "FPGA.bin", dev, GFP_KERNEL, data, fpga_fw_load)) {
			dev_err(dev,"fw async loading problem\n");
			data->status = STATUS_NOTLOADED;
		}
	}
}

static ssize_t fs_attr_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	int ret=0;

	switch (data->status) {
	case STATUS_WAITING: ret=sprintf(buf, "waiting\n"); break;
	case STATUS_LOADING: ret=sprintf(buf, "loading\n"); break;
	case STATUS_LOADED: ret=sprintf(buf, "loaded\n"); break;
	case STATUS_NOTLOADED: ret=sprintf(buf, "not loaded\n"); break;
	}
	return ret;
}

static ssize_t fs_attr_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpga_data *data = dev_get_drvdata(dev);

	if (data->status != STATUS_WAITING && !strncmp("reload",buf,6)) {
		data->status = STATUS_WAITING;
		if (request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG, "FPGA.bin", dev, GFP_KERNEL, data, fpga_fw_load)) {
			dev_err(dev,"fw async loading problem\n");
			data->status = STATUS_NOTLOADED;
		}
	}
	
	return count;
}
static DEVICE_ATTR(status, S_IRUGO | S_IWUSR, fs_attr_status_show, fs_attr_status_store);

static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	u8 version = *data->version;
	
	return data->status == STATUS_LOADED?
			sprintf(buf,"%X.%X\n",(version>>4)&0xf, version&0xf):
			sprintf(buf,"-.-\n");
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static ssize_t fs_attr_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return data->status == STATUS_LOADED?
			sprintf(buf,"%d\n",id):
			sprintf(buf,"-1\n");
}
static DEVICE_ATTR(num, S_IRUGO, fs_attr_num_show, NULL);

static ssize_t fs_attr_chassis_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return data->status == STATUS_LOADED?
			sprintf(buf,"%d\n",(id>>4)&0xf):
			sprintf(buf,"-1\n");
}
static DEVICE_ATTR(chassis, S_IRUGO, fs_attr_chassis_show, NULL);

static ssize_t fs_attr_carte_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	u8 id = *data->mcrid;
	
	return data->status == STATUS_LOADED?
			sprintf(buf,"%d\n",(id>>0)&0xf):
			sprintf(buf,"-1\n");
}
static DEVICE_ATTR(carte, S_IRUGO, fs_attr_carte_show, NULL);

static int __devinit fpga_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	int ret;
	int idx;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	int ngpios = of_gpio_count(np);
	struct fpga_data *data;
	int dir[NB_GPIO]=DIR_GPIO;

	data = kmalloc(GFP_KERNEL,sizeof(*data));
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	
	dev_set_drvdata(dev, data);
	data->dev = dev;

	if (ngpios<NB_GPIO) {
		dev_err(dev,"missing GPIO definition in device tree\n");
		ret = -EINVAL;
		goto err_free;
	}

	for (idx=0; idx<NB_GPIO ; idx++) {
		int gpio = ldb_gpio_init(np, dev, idx, dir[idx]);
		if (gpio == -1) {
			dev_err(dev,"problem with GPIO init\n");
			ret = -EINVAL;
			goto err_gpios;
		}
		data->gpio[idx] = gpio;
	}	
	
	data->version = of_iomap(np, 0);
	if (!data->version) {
		dev_err(dev,"of_iomap failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->mcrid = of_iomap(np, 1);
	
	if ((ret=device_create_file(dev, &dev_attr_status))
			|| (ret=device_create_file(dev, &dev_attr_version)))
		goto err_unfile;
		
	if (data->mcrid && ((ret=device_create_file(dev, &dev_attr_num))
			|| (ret=device_create_file(dev, &dev_attr_chassis))
			|| (ret=device_create_file(dev, &dev_attr_carte))))
		goto err_unfile;

	if (strstr(match->compatible,"base")) {
		data->status = STATUS_WAITING;
		if (request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG, "mcr3000/uFPGA.bin", dev, GFP_KERNEL, data, fpga_fw_load)) {
			dev_err(dev,"fw async loading problem\n");
			goto err_unfile;
		}

		dev_info(dev,"driver for MCR3000 FPGA initialized, waiting for firmware.\n");
	}
	else {
		data->status = STATUS_NOTLOADED;
		dev_info(dev,"driver for MCR3000 FPGA initialized, firmware can be loaded.\n");
	}
	return 0;
	
err_unfile:
	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(dev, &dev_attr_num);
	device_remove_file(dev, &dev_attr_chassis);
	device_remove_file(dev, &dev_attr_carte);
	
	iounmap(data->version);
	data->version = NULL;
	if (data->mcrid) iounmap(data->mcrid);
	data->mcrid = NULL;
err_gpios:
	for (idx=0; idx<NB_GPIO ; idx++) {
		int gpio = data->gpio[idx];
		if (ldb_gpio_is_valid(gpio))
			ldb_gpio_free(gpio);
	}
err_free:
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit fpga_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpga_data *data = dev_get_drvdata(dev);
	int idx;
	
	device_remove_file(dev, &dev_attr_status);
	device_remove_file(dev, &dev_attr_version);
	device_remove_file(dev, &dev_attr_num);
	device_remove_file(dev, &dev_attr_chassis);
	device_remove_file(dev, &dev_attr_carte);
	iounmap(data->version);
	data->version = NULL;
	if (data->mcrid) iounmap(data->mcrid);
	data->mcrid = NULL;
	for (idx=0; idx<NB_GPIO ; idx++) {
		int gpio = data->gpio[idx];
		if (ldb_gpio_is_valid(gpio))
			ldb_gpio_free(gpio);
	}

	dev_set_drvdata(dev, NULL);
	kfree(data);
	dev_info(dev,"driver for MCR3000 removed.\n");
	return 0;
}

static int __devinit fpga_spi_probe(struct spi_device *spi)
{
	fpga_spi = spi;

	return 0;
}

static int __devexit fpga_spi_remove(struct spi_device *spi)
{
	fpga_spi = NULL;

	return 0;
}

static const struct of_device_id fpga_match[] = {
	{
		.compatible = "s3k,mcr3000-fpga-base-loader",
	},
	{
		.compatible = "s3k,mcr3000-fpga-loader",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fpga_match);
static const struct spi_device_id fpga_ids[] = {
	{ "mcr3000-fpga-loader",   0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, fpga_ids);

static struct of_platform_driver fpga_driver = {
	.probe		= fpga_probe,
	.remove		= __devexit_p(fpga_remove),
	.driver		= {
		.name	= "fpga-loader",
		.owner	= THIS_MODULE,
		.of_match_table	= fpga_match,
	},
};

static struct spi_driver fpga_spi_driver = {
	.driver = {
		.name	= "fpga-loader",
		.owner	= THIS_MODULE,
	},
	.id_table = fpga_ids,
	.probe	= fpga_spi_probe,
	.remove	= __devexit_p(fpga_spi_remove),
};

static int __init fpga_init(void)
{
	int ret;
	ret = spi_register_driver(&fpga_spi_driver);
	if (ret) {
		pr_err("Pb spi_register_driver dans fpga_init: %d\n",ret);
		goto err;
	}
	ret = of_register_platform_driver(&fpga_driver);
err:
	return ret;
}
module_init(fpga_init);

static void __exit fpga_exit(void)
{
	of_unregister_platform_driver(&fpga_driver);
	spi_unregister_driver(&fpga_spi_driver);
}
module_exit(fpga_exit);

MODULE_AUTHOR("Christophe LEROY CSSI");
MODULE_DESCRIPTION("LOader for FPGA on MCR3000 ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:mcr3000-fpga-loader");
MODULE_ALIAS("platform:mcr3000-fpga-loader");
