/*
 * tsa.c - Driver for MPC 8xx TSA
 *
 * Authors: Christophe LEROY - Patrick VASSEUR
 * Copyright (c) 2011  CSSI
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
#include <asm/8xx_immap.h>
#include <saf3000/saf3000.h>
#include <asm/cpm1.h>
#include "tsa.h"

/*
 * driver TSA
 */
/*
 * 1.0 - xx/xx/2011 - creation du module TSA
 * 1.1 - 09/08/2012 - evolution pour gestion MCR3K-2G NVCS
 */
#define	TSA_VERSION		"1.1"

struct tsa_data {
//	int carte;
	struct device *dev;
	struct device *infos;
};

#define EXTRACT(x,dec,bits)	((x>>dec) & ((1<<bits)-1))
//#define CARTE_MCR3K_2G		1
//#define CARTE_MIAE		2

static ssize_t fs_attr_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int l;
	volatile immap_t *immap = ioremap(get_immrbase(),sizeof(*immap));

	l = snprintf(buf, PAGE_SIZE, "PAODR %04X\n",in_be16(&immap->im_ioport.iop_paodr));
	l += snprintf(buf+l, PAGE_SIZE-l, "PAPAR %04X\n",in_be16(&immap->im_ioport.iop_papar));
	l += snprintf(buf+l, PAGE_SIZE-l, "PADIR %04X\n",in_be16(&immap->im_ioport.iop_padir));
	l += snprintf(buf+l, PAGE_SIZE-l, "PADAT %04X\n",in_be16(&immap->im_ioport.iop_padat));
	
	l += snprintf(buf+l, PAGE_SIZE-l, "PDPAR %04X\n",in_be16(&immap->im_ioport.iop_pdpar));
	l += snprintf(buf+l, PAGE_SIZE-l, "PDDIR %04X\n",in_be16(&immap->im_ioport.iop_pddir));
	l += snprintf(buf+l, PAGE_SIZE-l, "PDDAT %04X\n",in_be16(&immap->im_ioport.iop_pddat));
	
	iounmap(immap);
	
	return l;
}
static DEVICE_ATTR(debug, S_IRUGO, fs_attr_debug_show, NULL);

static ssize_t fs_attr_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	len = snprintf(buf, PAGE_SIZE, "Le driver TSA est en version %s\n", TSA_VERSION);
	
	return len;
}
static DEVICE_ATTR(version, S_IRUGO, fs_attr_version_show, NULL);

static const struct of_device_id tsa_match[];
static int __devinit tsa_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct tsa_data *data;
	struct class *class;
	struct device *infos;
	cpm8xx_t *cpm;
	cpic8xx_t *cpic;
	int ret, i, nb_ts, inter = 0;
	u32 siram_a = 0, siram_b = 0, ts_enreg = 0, simode_tdma;
	siram_entry *siram_rx, *siram_tx;
	volatile immap_t *immap = ioremap(get_immrbase(),sizeof(*immap));
	const char *scc = NULL;
	const __be32 *info_ts = NULL;
	int len_scc = 0, len_info = 0, offset = 0;

	match = of_match_device(tsa_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	/* programmation des signaux ports pour le TSA */
	setbits16(&immap->im_ioport.iop_papar, 0x01c0);
	setbits16(&immap->im_ioport.iop_padir, 0x00c0);
	clrbits16(&immap->im_ioport.iop_padir, 0x0100);
	setbits16(&immap->im_ioport.iop_pdpar, 0x0002);
	clrbits16(&immap->im_ioport.iop_pdpar, 0x4000);
	clrbits16(&immap->im_ioport.iop_pddir, 0x0002);
	iounmap(immap);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(dev, data);
	data->dev = dev;

	/* releve des informations du TSA */
	scc = of_get_property(np, "scc_codec", &len_scc);
	/* TSA du MIAE */
	if (scc && (len_scc >= sizeof(*scc))) {
		siram_a = SIRAM_BYT;
		if (sysfs_streq(scc, "SCC3")) siram_a |= SIRAM_CSEL_SCC3;
		else if (sysfs_streq(scc, "SCC4")) siram_a |= SIRAM_CSEL_SCC4;
		else if (sysfs_streq(scc, "SMC2")) siram_a |= SIRAM_CSEL_SMC2;
		else siram_a = 0;
		scc = of_get_property(np, "scc_e1", &len_scc);
		if (scc && (len_scc >= sizeof(*scc))) {
			siram_b = SIRAM_BYT;
			if (sysfs_streq(scc, "SCC3")) siram_b |= SIRAM_CSEL_SCC3;
			else if (sysfs_streq(scc, "SCC4")) siram_b |= SIRAM_CSEL_SCC4;
			else if (sysfs_streq(scc, "SMC2")) siram_b |= SIRAM_CSEL_SMC2;
		}
		/* releve recurrence des TS E1 */
		info_ts = of_get_property(np, "ts_info", &len_info);
		if (info_ts && (len_info >= sizeof(*info_ts))) {
			len_info /= sizeof(__be32);
			if ((len_info == 2) && (info_ts[0] < (info_ts[1] / 2))
				&& ((info_ts[1] == 4) || (info_ts[1] == 8)))
				inter = info_ts[1] / 2;
		}
		/* verification que tous les parametres soient corrects */
		if ((siram_a == 0) || (siram_b == 0) || (inter == 0)) {
			ret = -EINVAL;
			goto err;
		}
	}
	/* TSA de la MCR3K-2G */
	else {
		scc = of_get_property(np, "scc_voie", &len_scc);
		if (scc && (len_scc >= sizeof(*scc))) {
			siram_a = SIRAM_BYT;
			if (sysfs_streq(scc, "SCC3")) siram_a |= SIRAM_CSEL_SCC3;
			else if (sysfs_streq(scc, "SCC4")) siram_a |= SIRAM_CSEL_SCC4;
			else if (sysfs_streq(scc, "SMC2")) siram_a |= SIRAM_CSEL_SMC2;
		}
		scc = of_get_property(np, "scc_retard", &len_scc);
		if (scc && (len_scc >= sizeof(*scc))) {
			siram_b = SIRAM_BYT;
			if (sysfs_streq(scc, "SCC3")) siram_b |= SIRAM_CSEL_SCC3;
			else if (sysfs_streq(scc, "SCC4")) siram_b |= SIRAM_CSEL_SCC4;
			else if (sysfs_streq(scc, "SMC2")) siram_b |= SIRAM_CSEL_SMC2;
			else siram_b = 0;
		}
		/* TS supplementaires pour enregistrement */
		info_ts = of_get_property(np, "ts_enreg", &len_info);
		if (info_ts && (len_info >= sizeof(*info_ts)) && (len_info == sizeof(__be32)))
			ts_enreg = info_ts[0];
		/* verification que tous les parametres soient corrects */
		/* voies GRS definies et ts_enreg <= 64 TS */
		if ((siram_a == 0) && (ts_enreg <= 64)) {
			ret = -EINVAL;
			goto err;
		}
	}

	/* affectation differente sur les SCCs */
	if (siram_a == siram_b) {
		ret = -EINVAL;
		goto err;
	}

	cpm = of_iomap(np, 0);
	if (cpm == NULL) {
		dev_err(dev,"of_iomap CPM failed\n");
		ret = -ENOMEM;
		goto err;
	}
	
	class = saf3000_class_get();
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "tsa");
	dev_set_drvdata(infos, data);
	data->infos = infos;

	siram_rx = (siram_entry*)&cpm->cp_siram;
	siram_tx = (siram_entry*)&cpm->cp_siram + 64;

	if (inter) {
		/* programmation des TimeSlots : 31 TS phonie E1 et 12 TS phonie Codec */
		/* programmation des premiers TS non utilises TS0 E1 + interval E1 */
		nb_ts = info_ts[0] + inter;
		out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
		out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
		offset++;
		/* programmation des 30 premiers TS phonie E1 et des 12 TS phonie Codec */
		for (i = 0, nb_ts = 12; i < 30; i++) {
			/* programmation TS E1 */
			out_be32(siram_rx + offset, siram_b | SIRAM_CNT(1));
			out_be32(siram_tx + offset, siram_b | SIRAM_CNT(1));
			offset++;
			/* programmation de X TS phonie codec ou X TS non utilises entre 2 TS E1 */
			ret = (nb_ts > (inter - 1)) ? 3 : nb_ts;
			if (ret) {
				out_be32(siram_rx + offset, siram_a | SIRAM_CNT(ret));
				out_be32(siram_tx + offset, siram_a | SIRAM_CNT(ret));
				offset++;
				nb_ts -= ret;
				if (ret < (inter - 1)) {
					ret = (inter - 1) - ret;
					out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(ret));
					out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(ret));
					offset++;
				}
			}
			else {
				out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(inter - 1));
				out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(inter - 1));
				offset++;
			}
		}
		/* programmation du dernier TS E1 */
		siram_b |= SIRAM_LST;
		out_be32(siram_rx + offset, siram_b | SIRAM_CNT(1));
		out_be32(siram_tx + offset, siram_b | SIRAM_CNT(1));
	}
	else {
		u32 siram = SIRAM_LST;
		/* si TS enreg => programmation obligatoire des TS retard */
		if (ts_enreg) siram_b |= SIRAM_BYT;
		/* programmation des 16 TimeSlots voies */
		if (siram_b) nb_ts = 16; else nb_ts = 15, siram |= siram_a;
		out_be32(siram_rx + offset, siram_a | SIRAM_CNT(nb_ts));
		out_be32(siram_tx + offset, siram_a | SIRAM_CNT(nb_ts));
		offset++;
		if (siram_b) {
			out_be32(siram_rx + offset, siram_b | SIRAM_CNT(16));
			out_be32(siram_tx + offset, siram_b | SIRAM_CNT(16));
			offset++;
			if (ts_enreg) nb_ts = 16; else nb_ts = 15, siram |= siram_b;
			out_be32(siram_rx + offset, siram_b | SIRAM_CNT(nb_ts));
			out_be32(siram_tx + offset, siram_b | SIRAM_CNT(nb_ts));
			offset++;
		}
		if (ts_enreg) {
			while (ts_enreg > 16) {
				out_be32(siram_rx + offset, siram_a | SIRAM_CNT(16));
				out_be32(siram_tx + offset, siram_a | SIRAM_CNT(16));
				offset++;
				ts_enreg -= 16;
			}
			if (ts_enreg > 1) {
				out_be32(siram_rx + offset, siram_a | SIRAM_CNT(ts_enreg - 1));
				out_be32(siram_tx + offset, siram_a | SIRAM_CNT(ts_enreg - 1));
				offset++;
			}
			siram |= siram_a;
		}
		out_be32(siram_rx + offset, siram | SIRAM_CNT(1));
		out_be32(siram_tx + offset, siram | SIRAM_CNT(1));
	}

	simode_tdma = (SIMODE_SDM_NORM | SIMODE_RFSD_0 | SIMODE_CRT | SIMODE_FE | SIMODE_GM | SIMODE_TFSD_0);
	out_be32(&cpm->cp_simode, (in_be32(&cpm->cp_simode) & ~SIMODE_TDMa(SIMODE_TDM_MASK)) | SIMODE_TDMa(simode_tdma));
//	out_be32(&cpm->cp_simode, (in_be32(&cpm->cp_simode) & ~SIMODE_TDMa(SIMODE_TDM_MASK)) | SIMODE_TDMa(simode_tdma) | simode);
//	
//	setbits32(&cpm->cp_sicr, sicr);
//
	/* SCC4 a la priorite la plus haute dans son niveau de priorite */ 
	cpic = of_iomap(np, 1);
	if ((siram_a & SIRAM_CSEL_SCC4) || (siram_b & SIRAM_CSEL_SCC4))
		clrsetbits_be32(&cpic->cpic_cicr, CICR_HP_MASK, CICR_HP_SCC4);
	iounmap(cpic);
	
	out_8(&cpm->cp_sigmr, SIGMR_ENa | SIGMR_RDM_STATIC_TDMa);
	iounmap(cpm);
	
	if ((ret=device_create_file(infos, &dev_attr_debug))) {
		goto err_unfile;
	}
	if ((ret=device_create_file(infos, &dev_attr_version))) {
		device_remove_file(infos, &dev_attr_debug);
		goto err_unfile;
	}
	
	dev_info(dev, "driver TSA added.\n");
	
	return 0;

err_unfile:
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit tsa_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct tsa_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(infos, &dev_attr_debug);
	device_remove_file(infos, &dev_attr_version);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
	
	dev_info(dev,"driver TSA removed.\n");
	return 0;
}

static const struct of_device_id tsa_match[] = {
	{
		.compatible = "fsl,cpm1-tsa",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tsa_match);

static struct platform_driver tsa_driver = {
	.probe		= tsa_probe,
	.remove		= __devexit_p(tsa_remove),
	.driver		= {
			  	.name		= "tsa",
			  	.owner		= THIS_MODULE,
			  	.of_match_table	= tsa_match,
			  },
};

static int __init tsa_init(void)
{
	return platform_driver_register(&tsa_driver);
}
module_init(tsa_init);

MODULE_AUTHOR("C.LEROY - P.VASSEUR");
MODULE_DESCRIPTION("Driver for TSA on MPC8xx ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mpc8xx-tsa");
