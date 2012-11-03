/*
 * messagerie_DSP.c - MCR3K
 *
 * Authors: Patrick VASSEUR
 * Copyright (c) 2012  CSSI
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <saf3000/saf3000.h>
#include <saf3000/dpram.h>


/*
 * 1.0 - 09/08/2012 - creation du module MSG DSP
 */
#define	MSG_DSP_VERSION		"1.0"

struct messagerie_data {
	struct device *dev;
	struct device *info;
	int gestion;
	int irq;
	short *ades;
	short ix_wr_cmd;
	short cmde[32];
	short ix_rd_msg;
	short mesg[96];
	thandler handler[MSG_LAST_MSG];
};

static struct messagerie_data *data_msg = NULL;


static ssize_t fs_attr_messagerie_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct messagerie_data *data = dev_get_drvdata(dev);
	int len = 0;
	
	len += snprintf(buf + len, PAGE_SIZE - len, "Le driver messagerie DSP est en version %s\n", MSG_DSP_VERSION);
	len += snprintf(buf + len, PAGE_SIZE - len, "La messagerie DSP est geree par le %s\n",
			data->gestion ? "KERNEL" : "DRIVER");
	
	return len;
}
static DEVICE_ATTR(messagerie, S_IRUGO, fs_attr_messagerie_show, NULL);

static ssize_t fs_attr_fifo_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct messagerie_data *data = dev_get_drvdata(dev);
	short *ades = data->ades;
	int len = 0, i;
	len += snprintf(buf + len, PAGE_SIZE - len, "La fifo \"commande\" (indice ecriture %d) :\n", data->ix_wr_cmd);
	for (i = 0; i < 32; i++, ades += 3) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s %02d (%04hX > %03hhu/%03hhu) = %04hX %04hX %04hX\n",
				(i == data->ix_wr_cmd) ? ">" : " ", i, data->cmde[i], data->cmde[i],
				data->cmde[i] >> 8, *ades, *(ades + 1), *(ades + 2));
	}
	return len;
}
static DEVICE_ATTR(fifo_cmd, S_IRUGO, fs_attr_fifo_cmd_show, NULL);

static ssize_t fs_attr_fifo_msg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct messagerie_data *data = dev_get_drvdata(dev);
	short *ades = data->ades + (32 * 3);
	int len = 0, i;
	len += snprintf(buf + len, PAGE_SIZE - len, "La fifo \"message\" (indice lecture %d) :\n", data->ix_rd_msg);
	for (i = 0; i < 96; i++, ades += 3)
		len += snprintf(buf + len, PAGE_SIZE - len, "%s %02d (%04hX > %03hhu/%03hhu) = %04hX %04hX %04hX\n",
				(i == data->ix_rd_msg) ? ">" : " ", i, data->mesg[i], data->mesg[i],
				data->mesg[i] >> 8, *ades, *(ades + 1), *(ades + 2));
	
	return len;
}
static DEVICE_ATTR(fifo_msg, S_IRUGO, fs_attr_fifo_msg_show, NULL);


static irqreturn_t gest_irq(s32 irq, void *context)
{
	short *base = data_msg->ades + (32 * 3), *ades;
	unsigned short info[3];
	
	ades = base + (data_msg->ix_rd_msg * 3);
	while (*ades) {
		data_msg->mesg[data_msg->ix_rd_msg] = *ades;
		info[0] = *ades;
		info[1] = *(ades + 1);
		info[2] = *(ades + 2);
		*ades = 0;
		if (data_msg->handler[info[0] & 0xff] != 0) {
			data_msg->handler[info[0] & 0xff](info);
		}
		data_msg->ix_rd_msg++;
		if (data_msg->ix_rd_msg == 96)
			data_msg->ix_rd_msg = 0;
		ades = base + (data_msg->ix_rd_msg * 3);
	}

	return IRQ_HANDLED;
}


void enreg_it_msg(void)
{
	struct device_node *np = data_msg->dev->of_node;
	int ret, irq;

	irq = of_irq_to_resource(np, 0, NULL);
	if (irq) {
		data_msg->irq = irq;
		ret = request_irq(irq, gest_irq, 0, "messagerie", NULL);
		if (ret)
			dev_err(data_msg->dev, "request irq retour %d\n", ret);
		else {
			dev_info(data_msg->dev, "Irq messagerie %d\n", irq);
			data_msg->gestion = 1;	/* temoin messagerie geree par KERNEL */
		}
	}
	else
		dev_err(data_msg->dev, "Pb enregistrment IT DSP Messagerie\n");
}
EXPORT_SYMBOL(enreg_it_msg);


void enreg_handler(int msg, thandler ipFonction)
{
	if (msg < MSG_LAST_MSG)
		data_msg->handler[msg] = ipFonction;
}
EXPORT_SYMBOL(enreg_handler);


int write_fifo_cmde(short *cmde)
{
	short *ades;
	
	if (data_msg == NULL) return -ENOMEM;
	
	ades = data_msg->ades + (data_msg->ix_wr_cmd * 3);
	if (*ades) return -ENOSPC;
	
	data_msg->cmde[data_msg->ix_wr_cmd] = *cmde;
	*ades++ = *cmde++;
	*ades++ = *cmde++;
	*ades = *cmde;
	data_msg->ix_wr_cmd++;
	if (data_msg->ix_wr_cmd == 32)
		data_msg->ix_wr_cmd = 0;
	
	return 0;
}
EXPORT_SYMBOL(write_fifo_cmde);

static const struct of_device_id messagerie_dsp_match[];
static int __devinit messagerie_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct messagerie_data *data;
	struct class *class;
	struct device *info;
	int ret;

	match = of_match_device(messagerie_dsp_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	/* remappage de l'adresse physique de la DPRAM Messagerie */
	data->ades = of_iomap(np, 0);
	if (!data->ades) {
		dev_err(dev, "of_iomap DPRAM Messagerie failed\n");
		ret = -ENOMEM;
		goto err_unmap;
	}

	class = saf3000_class_get();
	info = device_create(class, dev, MKDEV(0, 0), NULL, "msg_dsp");
	dev_set_drvdata(info, data);
	data->info = info;

	if ((ret = device_create_file(info, &dev_attr_messagerie))
		|| (ret = device_create_file(info, &dev_attr_fifo_cmd))
		|| (ret = device_create_file(info, &dev_attr_fifo_msg)))
		goto err_unfile;

	data_msg = data;
	dev_info(dev, "driver messagerie DSP added.\n");
	return 0;

err_unfile:
	device_remove_file(info, &dev_attr_messagerie);
	device_remove_file(info, &dev_attr_fifo_cmd);
	device_remove_file(info, &dev_attr_fifo_msg);
	
	dev_set_drvdata(info, NULL);
	device_unregister(info), data->info = NULL;

err_unmap:
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}


static int __devexit messagerie_remove(struct platform_device *ofdev)
{
	return 0;
}


/* informations d'exploitation de la messagerie DSP */
static const struct of_device_id messagerie_dsp_match[] = {
	{
		.compatible = "s3k,mcr3000-msg-dsp",
	},
	{},
};
MODULE_DEVICE_TABLE(of, messagerie_dsp_match);

static struct platform_driver messagerie_driver = {
	.probe		= messagerie_probe,
	.remove		= __devexit_p(messagerie_remove),
	.driver		= {
		.name	= "messagerie",
		.owner	= THIS_MODULE,
		.of_match_table	= messagerie_dsp_match,
	},
};

/* routine de chargement du driver messagerie DSP */
static int __init messagerie_init(void)
{
	int ret;
	ret = platform_driver_register(&messagerie_driver);
	return ret;
}
module_init(messagerie_init);

/* routine de dechargement du driver messagerie DSP */
static void __exit messagerie_exit(void)
{
	platform_driver_unregister(&messagerie_driver);
}
module_exit(messagerie_exit);

/* Informations generales sur le module messagerie DSP */
MODULE_AUTHOR("P.VASSEUR");
MODULE_DESCRIPTION("Driver for Messagerie of DSP");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-messagerie-dsp");
