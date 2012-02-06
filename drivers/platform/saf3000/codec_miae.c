/*
 * codec_miae.c - MIAE 2G CODEC
 *
 * Authors: Patrick VASSEUR
 * Copyright (c) 2012  CSSI
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/of_spi.h>
#include <saf3000/saf3000.h>


/*
 * 1.0 - 03/01/2012 - creation driver pour MIAE
 */
#define CODEC_VERSION		"1.0"
#define CODEC_AUTHOR		"VASSEUR Patrick - Janvier 2012"

#define MAX_CANAL_AUDIO		4

/* tableau de conversion gain numérique réception carte */
#define GAIN_AUDIO_REC_0DB	13
#define INDEX_AUDIO_REC_MAX	67
static const unsigned int GAIN_AUDIO_REC[INDEX_AUDIO_REC_MAX] = {
/*  (0)	-3,25	-3	-2.75	-2.5	-2.25	-2	-1.75	-1.5	-1.25	-1 */
	1252,	1288,	1326,	1365,	1405,	1446,	1488,	1531,	1576,	1622,
/* (10)	-0.75	-0.5	-0.25	0	0.25	0.5	0.75	1	1.25	1.5 */
	1669,	1718,	1768,	1820,	1873,	1928,	1984,	2042,	2102,	2163,
/* (20)	1.75	2	2.25	2.5	2.75	3	3.25	3.5	3.75	4 */
	2226,	2291,	2358,	2427,	2498,	2571,	2646,	2723,	2803,	2885,
/* (30)	4.25	4.5	4.75	5	5.25	5.5	5.75	6	6.25	6.5 */
	2967,	3055,	3145,	3236,	3331,	3428,	3528,	3631,	3737,	3847,
/* (40)	6.75	7	7.25	7.5	7.75	8	8.25	8.5	8.75	9 */
	3959,	4074,	4193,	4316,	4442,	4572,	4705,	4843,	4984,	5129,
/* (50)	9.25	9.5	9.75	10	10.25	10.5	10.75	11	11.25	11.5 */
	5279,	5433,	5592,	5755,	5923,	6096,	6274,	6458,	6646,	6840,
/* (60)	11.75	12	12.25	12.5	12.75	13	13.25 */
	7040,	7246,	7457,	7675,	7899,	8130,	8192
};

/* tableau de conversion gain numérique émission carte */
#define GAIN_AUDIO_EM_0DB	97
#define INDEX_AUDIO_EM_MAX	110
static const unsigned int GAIN_AUDIO_EM[INDEX_AUDIO_EM_MAX] = {
/*  (0)	-24,25	-24	-23.75	-23.5	-23.25	-23	-22.75	-22.5	-22.25	-22 */
	154,	158,	163,	167,	172,	177,	183,	188,	193,	199,
/* (10)	-21.75	-21.5	-21.25	-21	-20.75	-20.5	-20.25	-20	-19.75	-19.5 */
	205,	211,	217,	223,	230,	237,	243,	251,	258,	265,
/* (20)	-19.25	-19	-18.75	-18.5	-18.25	-18	-17.75	-17.5	-17.25	-17 */
	273,	281,	289,	298,	307,	315,	325,	334,	344,	354,
/* (30)	-16.75	-16.5	-16.25	-16	-15.75	-15.5	-15.25	-15	-14.75	-14.5 */
	364,	375,	386,	397,	409,	421,	433,	446,	459,	472,
/* (40)	-14.25	-14	-13.75	-13.5	-13.25	-13	-12.75	-12.5	-12.25	-12 */
	486,	500,	515,	530,	545,	561,	577,	594,	612,	629,
/* (50)	-11.75	-11.5	-11.25	-11	-10.75	-10.5	-10.25	-10	-9.75	-9.5 */
	648,	667,	686,	706,	727,	748,	770,	792,	816,	839,
/* (60)	-9.25	-9	-8.75	-8.5	-8.25	-8	-7.75	-7.5	-7.25	-7 */
	864,	889,	915,	942,	969,	998,	1027,	1057,	1088,	1119,
/* (70)	-6.75	-6.5	-6.25	-6	-5.75	-5.5	-5.25	-5	-4.75	-4.5 */
	1152,	1186,	1220,	1256,	1293,	1330,	1369,	1409,	1450,	1493,
/* (80)	-4.25	-4	-3.75	-3.5	-3.25	-3	-2.75	-2.5	-2.25	-2 */
	1536,	1581,	1627,	1675,	1724,	1774,	1826,	1879,	1934,	1991,
/* (90)	-1.75	-1.5	1.25	-1	-0.75	-0.5	-0.25	0	0.25	0.5 */
	2049,	2109,	2170,	2233,	2299,	2366,	2435,	2506,	2579,	2654,
/*(100)	0.75	1	1.25	1.5	1.75	2	2.25	2.5	2.75	3 */
	2732,	2812,	2894,	2978,	3065,	3155,	3247,	3342,	3439,	3540
};

#define CODEC_HS		0
#define CODEC_OK		1

struct canal_audio {
	char			mode;
	char			ts;
	char			io;
	char			gain_em[10];
	char			gain_rec[10];
};

struct codec {
	struct spi_device	*spi_dev;
	struct device		*dev;
	int			status;
	int			debug;
	struct canal_audio	canal[MAX_CANAL_AUDIO];
};


/* conversion texte niveau en valeur arrondie au centieme */
static int gain_val(const char *buf, size_t count)
{
	int _Val = 0, i, j;

	for (i = 0, j = 0; i < count; i++) {
		if ((buf[i] == ',') || (buf[i] == '.'))
			j = 1;
		else if ((buf[i] >= '0') && (buf[i] <= '9')) {
			if (j == 1) {
				_Val *= 100;
				_Val += (buf[i] - '0') * 10;
				j++;
			}
			else if (j > 1) {
				_Val += buf[i] - '0';
				break;
			}
			else {
				_Val *= 10;
				_Val += buf[i] - '0';
			}
		}
	}
	if (j == 0) _Val *= 100;
	if (buf[0] == '-') _Val = ~(_Val - 1);

	return _Val;
}


/* ecriture du gain emission dans le codec */
static int gain_em(struct spi_device *spi, int canal, int val)
{
	char _Info;
	int _Result = 0, _Ix, _Gain;	

	_Ix = GAIN_AUDIO_EM_0DB;		/* index gain pour 0 dB*/
	if (val < 0) {
		val = ~val;
		_Ix -= val / 25;
		if ((val % 25) > 12) _Ix--;
		if (_Ix < 0) _Result = -EINVAL;
	}
	else {
		_Ix += val / 25;
		if ((val % 25) > 12) _Ix++;
		if (_Ix >= INDEX_AUDIO_EM_MAX) _Result = -EINVAL;
	}
	if (_Result < 0) goto fin;

	_Gain = GAIN_AUDIO_EM[_Ix];
	_Info = 0xC2 + (canal << 2);
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = _Gain & 0x7F;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = 0xC3 + (canal << 2);
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = (_Gain >> 7) & 0x7F;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Result = _Ix;

fin:
	return _Result;
}


/* ecriture du gain reception dans le codec */
static int gain_rec(struct spi_device *spi, int canal, int val)
{
	char _Info;
	int _Result = 0, _Ix, _Gain;	

	_Ix = GAIN_AUDIO_REC_0DB;		/* index gain pour 0 dB*/
	if (val < 0) {
		val = ~val;
		_Ix -= val / 25;
		if ((val % 25) > 12) _Ix--;
		if (_Ix < 0) _Result = -EINVAL;
	}
	else {
		_Ix += val / 25;
		if ((val % 25) > 12) _Ix++;
		if (_Ix >= INDEX_AUDIO_REC_MAX) _Result = -EINVAL;
	}
	if (_Result < 0) goto fin;

	_Gain = GAIN_AUDIO_EM[_Ix];
	_Info = 0xC0 + (canal << 2);
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = _Gain & 0x7F;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = 0xC1 + (canal << 2);
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = (_Gain >> 7) & 0x7F;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Result = _Ix;

fin:
	return _Result;
}


/* ecriture de la commande du gain analogique dans le codec */
static int io_canal(struct spi_device *spi, int canal, int val)
{
	char _Info;
	int _Result = 0;	

	_Info = 0xD0 + (canal << 2);
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = val;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Result = val;

fin:
	return _Result;
}


/* ecriture du mode de fonctionnement dans le codec */
static int mode_canal(struct spi_device *spi, struct codec *codec, int canal, int val)
{
	char _Info;
	int _Result = 0;	

	/* changement de mode => initialisation switch codec */
	_Result = io_canal(spi, canal, 0x1F);
	if (_Result >= 0)
		codec->canal[canal].io = _Result;
	else goto fin;
	/* changement de mode => initialisation gain emission */
	_Info = gain_val(codec->canal[canal].gain_em, 10);
	_Result = gain_em(spi, canal, _Info);
	if (_Result < 0) goto fin;
	/* changement de mode => initialisation gain reception */
	_Info = gain_val(codec->canal[canal].gain_rec, 10);
	_Result = gain_rec(spi, canal, _Info);
	if (_Result < 0) goto fin;

	_Info = 0xA0 + (canal << 2);
	if (val) _Info |= 0x03;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Info = codec->canal[canal].ts;
	_Result = spi_write_then_read(spi, &_Info, 1, NULL, 0);
	if (_Result < 0) goto fin;
	_Result = val;
	
fin:
	return _Result;
}


/* gestion du mode de fonctionnement du codec */
static int mode(struct device *dev, int canal, const char *buf, size_t count)
{
	struct codec *codec = dev_get_drvdata(dev);
	int _Result = -1, _Val;

	if (codec->debug)
		dev_info(dev, "entree mode = %s (lg = %d)\n", buf, count);

	if ((buf[0] >= '0') && (buf[0] <= '2')) _Val = buf[0] - '0';
	else  goto fin;
	_Result = mode_canal(codec->spi_dev, codec, canal, _Val);
	if (_Result >= 0) {
		if (codec->debug)
			dev_info(dev, "Ecriture mode OK\n");
		codec->canal[canal].mode = _Result;
	}

fin:
	return _Result;
}


/* gestion du niveau emission du codec */
static int niveau_em(struct device *dev, int canal, const char *buf, size_t count)
{
	struct codec *codec = dev_get_drvdata(dev);
	int _Result = 0, _Ix, _Val = 0;

	if (codec->debug)
		dev_info(dev, "entree gain = %s (lg = %d)\n", buf, count);
	_Val = gain_val(buf, count);
	if (codec->debug)
		dev_info(dev, "lecture = %d\n", _Val);

	if (codec->canal[0].mode == 2) {
		_Ix = codec->canal[canal].io;
		if (_Val <= -1000) {
			_Val += 1000;
			if (codec->debug)
				dev_info(dev, "Activation I/O -10dB\n");
			_Ix &= ~(0x04);
		}
		else {
			if (codec->debug)
				dev_info(dev, "Activation I/O 0dB\n");
			_Ix |= 0x04;
		}
		_Result = io_canal(codec->spi_dev, canal, _Ix);
		if (_Result >= 0)
			codec->canal[canal].io = _Result;
		else goto fin;
	}

	_Result = gain_em(codec->spi_dev, canal, _Val);
	if (_Result >= 0) {
		if (codec->debug)
			dev_info(dev, "Ecriture gain em d'indice %d\n", _Result);
		sprintf(codec->canal[canal].gain_em, buf);
		codec->canal[canal].gain_em[count - 1] = 0;
	}

fin:
	return _Result;
}


/* gestion du niveau reception du codec */
static int niveau_rec(struct device *dev, int canal, const char *buf, size_t count)
{
	struct codec *codec = dev_get_drvdata(dev);
	int _Result = 0, _Ix, _Val = 0;

	if (codec->debug)
		dev_info(dev, "entree gain = %s (lg = %d)\n", buf, count);
	_Val = gain_val(buf, count);
	if (codec->debug)
		dev_info(dev, "lecture = %d\n", _Val);

	if (codec->canal[0].mode == 2) {
		_Ix = codec->canal[canal].io;
		if (_Val <= -1000) {
			_Val += 1000;
			if (codec->debug)
				dev_info(dev, "Activation I/O -10dB\n");
			_Ix &= ~(0x08);
		}
		else {
			if (codec->debug)
				dev_info(dev, "Activation I/O 0dB\n");
			_Ix |= 0x08;
		}
		_Result = io_canal(codec->spi_dev, canal, _Ix);
		if (_Result >= 0)
			codec->canal[canal].io = _Result;
		else goto fin;
	}

	_Result = gain_rec(codec->spi_dev, canal, _Val);
	if (_Result >= 0) {
		if (codec->debug)
			dev_info(dev, "Ecriture gain rec d'indice %d\n", _Result);
		sprintf(codec->canal[canal].gain_rec, buf);
		codec->canal[canal].gain_rec[count - 1] = 0;
	}

fin:
	return _Result;
}


/* gestion de l'attribut 'debug' */
static ssize_t fs_attr_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct codec *codec = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "Le mode debug est %s\n", codec->debug ? "actif" : "inactif");
}
static ssize_t fs_attr_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct codec *codec = dev_get_drvdata(dev);
	if (buf[0] != '0') codec->debug = 1;
	else codec->debug = 0;
	return count;
}
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_debug_show, fs_attr_debug_store);


/* gestion de l'attribut 'infos' */
static ssize_t fs_attr_infos_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct codec *codec = dev_get_drvdata(dev);
	int len = 0, i;
	len += snprintf(buf + len, PAGE_SIZE - len, "Le codec est %s\n", codec->status ? "initialise" : "HS");
	if (codec->status == CODEC_OK) {
		for (i = 0; i < MAX_CANAL_AUDIO; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len, "Canal %d : TS %d, OUT1 %s, OUT2 %s, OUT3 %s, OUT4 %s, OUT5 %s\n",
			(i + 1), codec->canal[i].ts, (codec->canal[i].io) & 0x01 ? " On" : "Off",
			(codec->canal[i].io) & 0x02 ? " On" : "Off", (codec->canal[i].io) & 0x04 ? " On" : "Off",
			(codec->canal[i].io) & 0x08 ? " On" : "Off", (codec->canal[i].io) & 0x10 ? " On" : "Off");
		}
	}
	return len;
}
static DEVICE_ATTR(infos, S_IRUGO, fs_attr_infos_show, NULL);


/* gestion de l'attribut 'mode' d'un canal audio */
static ssize_t fs_attr_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct codec *codec = dev_get_drvdata(dev);
	char info[20];
	int canal = simple_strtol(attr->attr.name + 6, NULL, 10) - 1;
	if (codec->canal[canal].mode == 2) sprintf(info, "actif avec switch");
	else if (codec->canal[canal].mode == 1) sprintf(info, "actif sans switch");
	else sprintf(info, "inactif");
	return snprintf(buf, PAGE_SIZE, "Le canal %d est %s\n", canal + 1, info);
}
static ssize_t fs_attr_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int _Result;
	int canal = simple_strtol(attr->attr.name + 6, NULL, 10) - 1;
	_Result = mode(dev, canal, buf, count);
	return count;
}
static DEVICE_ATTR(mode_c1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_mode_show, fs_attr_mode_store);
static DEVICE_ATTR(mode_c2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_mode_show, fs_attr_mode_store);
static DEVICE_ATTR(mode_c3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_mode_show, fs_attr_mode_store);
static DEVICE_ATTR(mode_c4, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_mode_show, fs_attr_mode_store);


/* gestion de l'attribut 'niveau emission' d'un canal audio */
static ssize_t fs_attr_niveau_em_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct codec *codec = dev_get_drvdata(dev);
	int canal = simple_strtol(attr->attr.name + 11, NULL, 10) - 1;
	return snprintf(buf, PAGE_SIZE, "Le niveau emission du canal %d est %s dBm\n", canal + 1, codec->canal[canal].gain_em);
}
static ssize_t fs_attr_niveau_em_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int _Result;
	int canal = simple_strtol(attr->attr.name + 11, NULL, 10) - 1;
	_Result = niveau_em(dev, canal, buf, count);
	return count;
}
static DEVICE_ATTR(niveau_em_c1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_em_show, fs_attr_niveau_em_store);
static DEVICE_ATTR(niveau_em_c2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_em_show, fs_attr_niveau_em_store);
static DEVICE_ATTR(niveau_em_c3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_em_show, fs_attr_niveau_em_store);
static DEVICE_ATTR(niveau_em_c4, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_em_show, fs_attr_niveau_em_store);


/* gestion de l'attribut 'niveau reception' d'un canal audio */
static ssize_t fs_attr_niveau_rec_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct codec *codec = dev_get_drvdata(dev);
	int canal = simple_strtol(attr->attr.name + 12, NULL, 10) - 1;
	return snprintf(buf, PAGE_SIZE, "Le niveau reception du canal %d est %s dBm\n", canal + 1, codec->canal[0].gain_rec);
}
static ssize_t fs_attr_niveau_rec_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int _Result;
	int canal = simple_strtol(attr->attr.name + 12, NULL, 10) - 1;
	_Result = niveau_rec(dev, canal, buf, count);
	return count;
}
static DEVICE_ATTR(niveau_rec_c1, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_rec_show, fs_attr_niveau_rec_store);
static DEVICE_ATTR(niveau_rec_c2, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_rec_show, fs_attr_niveau_rec_store);
static DEVICE_ATTR(niveau_rec_c3, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_rec_show, fs_attr_niveau_rec_store);
static DEVICE_ATTR(niveau_rec_c4, S_IRUGO | S_IWUSR | S_IWGRP, fs_attr_niveau_rec_show, fs_attr_niveau_rec_store);


/* gestion de la suppression des attributs */
static void codec_free_attr(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_debug);
	device_remove_file(&spi->dev, &dev_attr_infos);
	device_remove_file(&spi->dev, &dev_attr_mode_c1);
	device_remove_file(&spi->dev, &dev_attr_niveau_em_c1);
	device_remove_file(&spi->dev, &dev_attr_niveau_rec_c1);
	device_remove_file(&spi->dev, &dev_attr_mode_c2);
	device_remove_file(&spi->dev, &dev_attr_niveau_em_c2);
	device_remove_file(&spi->dev, &dev_attr_niveau_rec_c2);
	device_remove_file(&spi->dev, &dev_attr_mode_c3);
	device_remove_file(&spi->dev, &dev_attr_niveau_em_c3);
	device_remove_file(&spi->dev, &dev_attr_niveau_rec_c3);
	device_remove_file(&spi->dev, &dev_attr_mode_c4);
	device_remove_file(&spi->dev, &dev_attr_niveau_em_c4);
	device_remove_file(&spi->dev, &dev_attr_niveau_rec_c4);
	device_unregister(&spi->dev);
	dev_set_drvdata(&spi->dev, NULL);
	spi_set_drvdata(spi, NULL);
}


/* routine d'initialisation du driver codec */
static int __devinit codec_probe(struct spi_device *spi)
{
	struct codec *codec;
	struct device *dev;
	struct class *class;
	int _Result = -1, _Canal;
	const char *name_codec = NULL;
	const __be32 *ts = NULL;
	int len = 0, num = 0;
	struct device_node *np = spi->dev.of_node;
	
	codec = kzalloc(sizeof *codec, GFP_KERNEL);
	if (!codec) {
		_Result = -ENOMEM;
		goto err;
	}

	spi_set_drvdata(spi, codec);
	codec->spi_dev = spi;
	codec->status = CODEC_HS;
	if (np)
		name_codec = of_get_property(np, "name_codec", &len);
	if (!name_codec || (len < sizeof(*name_codec))) {
		_Result = -ENODEV;
		goto err;
	}
	len = 0;
	np = of_find_compatible_node(NULL, NULL, "fsl,mpc885-tsa");
	if (np)
		ts = of_get_property(np, "ts_codec", &len);
	if (!ts || (len < sizeof(*ts))) {
		_Result = -EINVAL;
		goto err;
	}

	num = simple_strtol(name_codec + 6, NULL, 10);
	class = saf3000_class_get();
	dev = device_create(class, &spi->dev, MKDEV(0, 0), NULL, "%s", name_codec);
	dev_set_drvdata(dev, codec);
	codec->dev = dev;
	dev_info(dev, "Reservation spi codec : 0x%08X\n", (int)spi);
	dev_info(dev, "Reservation dev codec : 0x%08X\n", (int)codec->dev);

	len = (num - 1) * MAX_CANAL_AUDIO;	/* index pour TS d'un codec */
	for (_Canal = 0; _Canal < MAX_CANAL_AUDIO; _Canal++) {
		dev_info(dev, "Initialisation du canal %d du CODEC %d\n", _Canal, num);
		/* affectation TS em et rec en loi A et canal en 'power down' */
		codec->canal[_Canal].ts = ts[_Canal + len];
		if (mode_canal(spi, codec, _Canal, 0) < 0)
			break;
		codec->canal[_Canal].mode = 0;
		/* initialisation gain emission carte à 0 dB */
		if (gain_em(spi, _Canal, 0) < 0)
			break;
		sprintf(codec->canal[_Canal].gain_em, "0");
		/* initialisation gain reception carte à 0 dB */
		if (gain_em(spi, _Canal, 0) < 0)
			break;
		sprintf(codec->canal[_Canal].gain_rec, "0");
		/* initialisation des I/O d'un canal en sortie a l'etat '1' */
		if (io_canal(spi, _Canal, 0x1F) < 0)
			break;
		codec->canal[_Canal].io = 0x1F;
	}

	if (_Canal != MAX_CANAL_AUDIO) {
		dev_info(dev, "Pb sur canal %d du CODEC %d\n", _Canal, num);
		_Result = -ENODEV;
		goto err_unfile;
	}
		
	codec->status = CODEC_OK;
	dev_info(dev, "Le CODEC %d est initialise\n", num);

	_Result = device_create_file(dev, &dev_attr_debug);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_infos);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_mode_c1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_em_c1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_rec_c1);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_mode_c2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_em_c2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_rec_c2);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_mode_c3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_em_c3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_rec_c3);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_mode_c4);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_em_c4);
	if (_Result) goto err_unfile;
	_Result = device_create_file(dev, &dev_attr_niveau_rec_c4);
	if (_Result) goto err_unfile;

	return 0;
	
err_unfile:
	codec_free_attr(spi);
	kfree(codec);
err:
	return _Result;
}


/* routine d'abandon du driver codec */
static int __devexit codec_remove(struct spi_device *spi)
{
	struct codec *codec = spi_get_drvdata(spi);

	codec_free_attr(spi);
	kfree(codec);

	return 0;
}


/* informations d'exploitation du driver codec */
static const struct spi_device_id codec_ids[] = {
	{ "mcr3000-sicofi", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, codec_ids);

static struct spi_driver codec_driver = {
	.driver = {
		.name	= "codec",
		.owner	= THIS_MODULE,
	},
	.id_table = codec_ids,
	.probe	= codec_probe,
	.remove	= __devexit_p(codec_remove),
};


/* routine de chargement du driver codec */
static int __init codec_init(void)
{
	return spi_register_driver(&codec_driver);
}
module_init(codec_init);


/* routine de dechargement du driver codec */
static void __exit codec_exit(void)
{
	spi_unregister_driver(&codec_driver);
}
module_exit(codec_exit);


/* Informations generales sur le module driver CODEC */
MODULE_AUTHOR(CODEC_AUTHOR);
MODULE_VERSION(CODEC_VERSION);
MODULE_DESCRIPTION("Driver for CODEC on MIA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:mcr3000-sicofi");
