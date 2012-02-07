/*
 * fpga_miae.c - MIAE 2G FPGA
 *
 * Authors: Patrick VASSEUR
 * Copyright (c) 2012  CSSI
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/of_platform.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <asm/bitops.h>
#include <linux/list.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/uio.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of_spi.h>
#include <linux/device.h>
#include <asm/prom.h>
#include <saf3000/saf3000.h>
#include <saf3000/fpgam.h>


/*
 * 1.0 - 03/01/2012 - creation driver pour MIAE
 */
#define FPGA_M_VERSION		"1.0"
#define FPGA_M_AUTHOR		"VASSEUR Patrick - Janvier 2012"


struct fpga_data {
	struct fpgam __iomem	*fpgam;
	struct device		*dev;
	struct device		*infos;
	int			status;
};

#define FPGA_HS		0
#define FPGA_OK		1

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))


static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 version = in_be16(&fpgam->version);
	return snprintf(buf, PAGE_SIZE, "La version du binaire est %X.%X.%X.%X\n", EXTRACT(version,12,4),
			EXTRACT(version,8,4), EXTRACT(version,4,4), EXTRACT(version,0,4));
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static ssize_t fs_attr_registres_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	int len = 0;
	len += snprintf(buf + len, PAGE_SIZE - len, "Registres infos :\n    ");
	len += snprintf(buf + len, PAGE_SIZE - len, "Ident : 0x%04X; Version : 0x%04X; Test : 0x%04X; Reset : 0x%04X\n",
		in_be16(&fpgam->ident), in_be16(&fpgam->version), in_be16(&fpgam->test),
		in_be16(&fpgam->reset));
	len += snprintf(buf + len, PAGE_SIZE - len, "Registres ITs :\n    ");
	len += snprintf(buf + len, PAGE_SIZE - len, "Mask1 : 0x%04X; Mask2 : 0x%04X; Pend1 : 0x%04X; Pend2 : 0x%04X; Ctl : 0x%04X\n",
		in_be16(&fpgam->it_mask1), in_be16(&fpgam->it_mask2), in_be16(&fpgam->it_pend1),
		in_be16(&fpgam->it_pend2), in_be16(&fpgam->it_ctrl));
	len += snprintf(buf + len, PAGE_SIZE - len, "Registres I/Os :\n    ");
	len += snprintf(buf + len, PAGE_SIZE - len, "TorIn : 0x%04X; TorOut : 0x%04X; FctGen : 0x%04X; FAR : 0x%04X; FAV : 0x%04X\n",
		in_be16(&fpgam->tor_in), in_be16(&fpgam->tor_out), in_be16(&fpgam->fct_gen),
		in_be16(&fpgam->gest_far), in_be16(&fpgam->gest_fav));
	len += snprintf(buf + len, PAGE_SIZE - len, "Registres PLL :\n    ");
	len += snprintf(buf + len, PAGE_SIZE - len, "Status : 0x%04X; Source : 0x%04X; Etat Ref : 0x%04X\n",
		in_be16(&fpgam->pll_status), in_be16(&fpgam->pll_src), in_be16(&fpgam->etat_ref));
	return len;
}
static DEVICE_ATTR(registres, S_IRUGO, fs_attr_registres_show, NULL);

static ssize_t fs_attr_reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info = in_be16(&fpgam->reset);
	int len = strlen(attr->attr.name);
	int num = simple_strtol(attr->attr.name + (len - 1), NULL, 10);
	char type[20];
	if (len == 10) {
		info >>= (num - 1);
		sprintf(type, "ETH%d", num);
	}
	else if (len == 8) {
		if (num == 1) {
			info >>= 4;
			sprintf(type, "E1");
		}
		else {
			info >>= 5;
			sprintf(type, "S0");
		}
	}
	else {
		info >>= 3;
		sprintf(type, "DSP");
	}
	return snprintf(buf, PAGE_SIZE, "Le reset %s est %s\n", type, EXTRACT(info,0,1)?"inactif":"actif");
}
static ssize_t fs_attr_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 msk = 0, info = in_be16(&fpgam->reset);
	int len = strlen(attr->attr.name);
	int num = simple_strtol(attr->attr.name + (len - 1), NULL, 10);
	if (len == 10)
		msk = 1 << (num - 1);
	else if (len == 8) {
		msk = 1 << 4;
		if (num != 1)
			msk = 1 << 5;
	}
	else
		msk = 1 << 3;
	if (sysfs_streq(buf, "0"))
		info |= msk;
	else
		info &= ~(msk);
	out_be16(&fpgam->reset, info);
	return count;
}
static DEVICE_ATTR(reset_eth1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);
static DEVICE_ATTR(reset_eth2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);
static DEVICE_ATTR(reset_eth3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);
static DEVICE_ATTR(reset_dsp, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);
static DEVICE_ATTR(reset_e1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);
static DEVICE_ATTR(reset_s0, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_reset_show, fs_attr_reset_store);

static ssize_t fs_attr_tor_in_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info = in_be16(&fpgam->tor_in);
	int tor = simple_strtol(attr->attr.name + 6, NULL, 10) - 1;
	return snprintf(buf, PAGE_SIZE, "Etat TOR_IN%d : %s\n", tor + 1, EXTRACT(info,tor,1)?"inactif":"actif");
}
static DEVICE_ATTR(tor_in1, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in2, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in3, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in4, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in5, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in6, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in7, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in8, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in9, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in10, S_IRUGO, fs_attr_tor_in_show, NULL);
static DEVICE_ATTR(tor_in11, S_IRUGO, fs_attr_tor_in_show, NULL);

static ssize_t fs_attr_tor_out_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info = in_be16(&fpgam->tor_out);
	int tor = simple_strtol(attr->attr.name + 7, NULL, 10) - 1;
	return snprintf(buf, PAGE_SIZE, "Etat TOR_OUT%d : %s\n", tor + 1, EXTRACT(info,tor,1)?"inactif":"actif");
}
static ssize_t fs_attr_tor_out_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info = in_be16(&fpgam->tor_out);
	int tor = simple_strtol(attr->attr.name + 7, NULL, 10) - 1;
	if (sysfs_streq(buf, "0"))
		info |= (1 << tor);
	else
		info &= ~(1 << tor);
	out_be16(&fpgam->tor_out, info);
	return count;
}
static DEVICE_ATTR(tor_out1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out4, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out5, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out6, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out7, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);
static DEVICE_ATTR(tor_out8, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_tor_out_show, fs_attr_tor_out_store);

static ssize_t fs_attr_type_face_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info;
	char face[10], type[20];
	if (strncmp(attr->attr.name, "fav", 3) == 0) {
		info = in_be16(&fpgam->gest_fav) >> 14;
		sprintf(face, "FAV");
	}
	else {
		info = in_be16(&fpgam->gest_far) >> 6;
		sprintf(face, "FAR");
	}
	if (EXTRACT(info,0,2) == 2) sprintf(type, "FABEC");
	else if (EXTRACT(info,0,2) == 3) sprintf(type, "CLA2000");
	else sprintf(type, "non defini");
	return snprintf(buf, PAGE_SIZE, "La %s est de type %s\n", face, type);
}
static DEVICE_ATTR(fav, S_IRUGO, fs_attr_type_face_show, NULL);
static DEVICE_ATTR(far, S_IRUGO, fs_attr_type_face_show, NULL);

static ssize_t fs_attr_led_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info;
	int debug = simple_strtol(attr->attr.name + 9, NULL, 10);
	char etat[10], type[10];
	if (debug) {
		sprintf(type, "DEBUG%d", debug);
		info = in_be16(&fpgam->fct_gen) >> 4;
		if (EXTRACT(info, (debug - 1), 1))
			sprintf(etat, "eteinte");
		else
			sprintf(etat, "allumee");
	}
	else {
		info = in_be16(&fpgam->gest_fav) >> 8;
		if (strncmp(attr->attr.name, "led_rouge", 9) == 0)
			sprintf(type, "ROUGE");
		else {
			sprintf(type, "VERTE");
			info >>= 1;
		}
		if (EXTRACT(info, 0, 1))
			sprintf(etat, "allumee");
		else
			sprintf(etat, "eteinte");
	}
	return snprintf(buf, PAGE_SIZE, "La led %s est %s\n", type, etat);
}
static ssize_t fs_attr_led_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 info;
	int debug = simple_strtol(attr->attr.name + 9, NULL, 10);
	if (debug) {
		info = in_be16(&fpgam->fct_gen);
		debug += 3;
		if (sysfs_streq(buf, "0"))
			info |= (1 << debug);
		else
			info &= ~(1 << debug);
		out_be16(&fpgam->fct_gen, info);
	}
	else {
		info = in_be16(&fpgam->gest_fav);
		debug = 8;
		if (strncmp(attr->attr.name, "led_verte", 9) == 0)
			debug++;
		if (sysfs_streq(buf, "0"))
			info &= ~(1 << debug);
		else
			info |= (1 << debug);
		out_be16(&fpgam->gest_fav, info);
	}
	return count;
}
static DEVICE_ATTR(led_rouge, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_led_show, fs_attr_led_store);
static DEVICE_ATTR(led_verte, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_led_show, fs_attr_led_store);
static DEVICE_ATTR(led_debug1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_led_show, fs_attr_led_store);
static DEVICE_ATTR(led_debug2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_led_show, fs_attr_led_store);
static DEVICE_ATTR(led_debug3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_led_show, fs_attr_led_store);

static ssize_t fs_attr_presence_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	int len = strlen(attr->attr.name);
	u16 info = 0, num = simple_strtol(attr->attr.name + (len - 1), NULL, 10);
	char type[20];
	if (len == 9) {
		info = in_be16(&fpgam->fct_gen) >> 1;
		sprintf(type, "bloc alim 24V");
	}
	else if (len == 11) {
		if (num) {
			info = in_be16(&fpgam->gest_fav) >> 1;
			if (num == 1)
				info >>= 4;
			sprintf(type, "micro main %d", num);
		}
	}
	else {
		if (num) {
			info = in_be16(&fpgam->gest_fav) >> 3;
			if (num == 1)
				info >>= 4;
			sprintf(type, "micro casque %d", num);
		}
		else {
			info = in_be16(&fpgam->gest_far) >> 4;
			sprintf(type, "combine");
		}
	}
	return snprintf(buf, PAGE_SIZE, "Le %s est %s\n", type, EXTRACT(info,0,1)?"absent":"present");
}
static DEVICE_ATTR(pres_combine, S_IRUGO, fs_attr_presence_show, NULL);
static DEVICE_ATTR(pres_micro1, S_IRUGO, fs_attr_presence_show, NULL);
static DEVICE_ATTR(pres_micro2, S_IRUGO, fs_attr_presence_show, NULL);
static DEVICE_ATTR(pres_casque1, S_IRUGO, fs_attr_presence_show, NULL);
static DEVICE_ATTR(pres_casque2, S_IRUGO, fs_attr_presence_show, NULL);
static DEVICE_ATTR(pres_alim, S_IRUGO, fs_attr_presence_show, NULL);

static ssize_t fs_attr_alternat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	int len = strlen(attr->attr.name);
	u16 info, num = simple_strtol(attr->attr.name + (len - 1), NULL, 10);
	char type[20];
	if (len == 5) {
		info = in_be16(&fpgam->gest_far) >> 1;
		if (num == 1)
			info >>= 1;
		sprintf(type, "%d", num);
	}
	else if (len == 10) {
		if (num) {
			info = in_be16(&fpgam->gest_fav);
			if (num == 1)
				info >>= 4;
			sprintf(type, "micro main %d", num);
		}
		else {
			info = in_be16(&fpgam->gest_far);
			sprintf(type, "pedale");
		}
	}
	else {
		if (num) {
			info = in_be16(&fpgam->gest_fav) >> 2;
			if (num == 1)
				info >>= 4;
			sprintf(type, "micro casque %d", num);
		}
		else {
			info = in_be16(&fpgam->gest_far) >> 3;
			sprintf(type, "combine");
		}
	}
	return snprintf(buf, PAGE_SIZE, "L'alternat %s est %s\n", type, EXTRACT(info,0,1)?"absent":"present");
}
static DEVICE_ATTR(alt_pedale, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_1, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_2, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_combine, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_micro1, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_micro2, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_casque1, S_IRUGO, fs_attr_alternat_show, NULL);
static DEVICE_ATTR(alt_casque2, S_IRUGO, fs_attr_alternat_show, NULL);

static ssize_t fs_attr_config_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fpga_data *data = dev_get_drvdata(dev);
	struct fpgam *fpgam = data->fpgam;
	u16 config = in_be16(&fpgam->fct_gen);
	return snprintf(buf, PAGE_SIZE, "La configuration \"%s\" est active\n", EXTRACT(config,0,1)?"usine":"exploitation");
}
static DEVICE_ATTR(config, S_IRUGO, fs_attr_config_show, NULL);


/* gestion de la suppression des attributs */
static void fpga_m_free_attr(struct device *infos)
{
	device_remove_file(infos, &dev_attr_version);
	device_remove_file(infos, &dev_attr_registres);
	device_remove_file(infos, &dev_attr_reset_eth1);
	device_remove_file(infos, &dev_attr_reset_eth2);
	device_remove_file(infos, &dev_attr_reset_eth3);
	device_remove_file(infos, &dev_attr_reset_dsp);
	device_remove_file(infos, &dev_attr_reset_e1);
	device_remove_file(infos, &dev_attr_reset_s0);
	device_remove_file(infos, &dev_attr_tor_in1);
	device_remove_file(infos, &dev_attr_tor_in2);
	device_remove_file(infos, &dev_attr_tor_in3);
	device_remove_file(infos, &dev_attr_tor_in4);
	device_remove_file(infos, &dev_attr_tor_in5);
	device_remove_file(infos, &dev_attr_tor_in6);
	device_remove_file(infos, &dev_attr_tor_in7);
	device_remove_file(infos, &dev_attr_tor_in8);
	device_remove_file(infos, &dev_attr_tor_in9);
	device_remove_file(infos, &dev_attr_tor_in10);
	device_remove_file(infos, &dev_attr_tor_in11);
	device_remove_file(infos, &dev_attr_tor_out1);
	device_remove_file(infos, &dev_attr_tor_out2);
	device_remove_file(infos, &dev_attr_tor_out3);
	device_remove_file(infos, &dev_attr_tor_out4);
	device_remove_file(infos, &dev_attr_tor_out5);
	device_remove_file(infos, &dev_attr_tor_out6);
	device_remove_file(infos, &dev_attr_tor_out7);
	device_remove_file(infos, &dev_attr_tor_out8);
	device_remove_file(infos, &dev_attr_far);
	device_remove_file(infos, &dev_attr_fav);
	device_remove_file(infos, &dev_attr_led_rouge);
	device_remove_file(infos, &dev_attr_led_verte);
	device_remove_file(infos, &dev_attr_led_debug1);
	device_remove_file(infos, &dev_attr_led_debug2);
	device_remove_file(infos, &dev_attr_led_debug3);
	device_remove_file(infos, &dev_attr_pres_combine);
	device_remove_file(infos, &dev_attr_pres_micro1);
	device_remove_file(infos, &dev_attr_pres_micro2);
	device_remove_file(infos, &dev_attr_pres_casque1);
	device_remove_file(infos, &dev_attr_pres_casque2);
	device_remove_file(infos, &dev_attr_pres_alim);
	device_remove_file(infos, &dev_attr_alt_pedale);
	device_remove_file(infos, &dev_attr_alt_1);
	device_remove_file(infos, &dev_attr_alt_2);
	device_remove_file(infos, &dev_attr_alt_combine);
	device_remove_file(infos, &dev_attr_alt_micro1);
	device_remove_file(infos, &dev_attr_alt_micro2);
	device_remove_file(infos, &dev_attr_alt_casque1);
	device_remove_file(infos, &dev_attr_alt_casque2);
	device_remove_file(infos, &dev_attr_config);
	dev_set_drvdata(infos, NULL);
}


/*
 * Chargement du module
 */
static const struct of_device_id fpga_m_match[];
static int __devinit fpga_m_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct class *class;
	struct device *infos;
	int _Result = 0;
	struct fpgam *fpgam;
	struct fpga_data *data;

	match = of_match_device(fpga_m_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	data = kzalloc(sizeof *data, GFP_KERNEL);
	if (!data) {
		_Result = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	fpgam = of_iomap(np, 0);
	if (fpgam == NULL) {
		dev_err(dev, "of_iomap FPGAM failed\n");
		_Result = -ENOMEM;
		goto err_map;
	}
	data->fpgam = fpgam;

	if ((in_be16(&fpgam->ident) >> 8) != 0x23) {
		dev_err(dev,"support card is not MIAE\n");
		_Result = -ENODEV;
		iounmap(data->fpgam);
		goto err_map;
	}
	data->status = FPGA_OK;
	
	class = saf3000_class_get();
		
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "fpgam");
	dev_set_drvdata(infos, data);
	data->infos = infos;

	/* positionnement fonctionnement des interruptions */
	/* masque toutes les ITs */
	out_be16(&fpgam->it_mask1, 0);
	out_be16(&fpgam->it_mask2, 0);
	/* clear toutes les ITs */
	out_be16(&fpgam->it_ack1, 0);
	out_be16(&fpgam->it_ack2, 0);
	/* validation IT sur front descendant */
	clrsetbits_be16(&fpgam->it_ctrl, 0x0003, 0x0001);
	/* force led rouge en FAV et extinction leds debug */
	clrsetbits_be16(&fpgam->gest_fav, 0x0300, 0x0100);
	clrsetbits_be16(&fpgam->fct_gen, 0x0070, 0x0070);

	_Result = device_create_file(infos, &dev_attr_version);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_registres);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_eth1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_eth2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_eth3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_dsp);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_e1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_reset_s0);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in4);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in5);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in6);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in7);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in8);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in9);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in10);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_in11);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out4);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out5);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out6);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out7);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_tor_out8);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_far);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_fav);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_led_rouge);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_led_verte);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_led_debug1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_led_debug2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_led_debug3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_combine);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_micro1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_micro2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_casque1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_casque2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_pres_alim);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_pedale);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_combine);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_micro1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_micro2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_casque1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_alt_casque2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(infos, &dev_attr_config);
	if (_Result) goto err_unfile;

	dev_info(dev, "driver MIAE FPGA-M added.\n");

	return 0;

err_unfile:
	fpga_m_free_attr(infos);
	iounmap(data->fpgam);
err_map:
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return _Result;
}


/*
 * Déchargement du module
 */
static int __devexit fpga_m_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct fpga_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;

	fpga_m_free_attr(infos);
	iounmap(data->fpgam);
	dev_set_drvdata(dev, NULL);
	kfree(data);
	dev_info(dev,"driver MIAE FPGA-M removed.\n");
	return 0;
}

static const struct of_device_id fpga_m_match[] = {
	{
		.compatible = "s3k,mcr3000-fpga-m",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fpga_m_match);


static struct platform_driver fpga_m_driver = {
	.probe		= fpga_m_probe,
	.remove		= __devexit_p(fpga_m_remove),
	.driver		= {
		.name	= "fpgam",
		.owner	= THIS_MODULE,
		.of_match_table	= fpga_m_match,
	},
};


static int __init fpga_m_init(void)
{
	int ret;
	ret = platform_driver_register(&fpga_m_driver);
//	u16_gpiochip_init("s3k,mcr3000-fpga-m-gpio");
	return ret;
}
subsys_initcall(fpga_m_init);
//module_init(fpga_m_init);


//static void __exit fpga_m_exit(void)
//{
//	of_unregister_platform_driver(&fpga_m_driver);
//}
//module_exit(fpga_m_exit);


/* INFORMATIONS GENERALES */
MODULE_AUTHOR(FPGA_M_AUTHOR);
MODULE_VERSION(FPGA_M_VERSION);
MODULE_DESCRIPTION("Driver for FPGA on MIAE");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mcr3000-fpga-m");
