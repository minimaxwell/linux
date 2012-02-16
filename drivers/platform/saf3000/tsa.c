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
#include <linux/of_spi.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/leds.h>
#include <sysdev/fsl_soc.h>
#include <asm/8xx_immap.h>
#include <saf3000/saf3000.h>
#include "tsa.h"

/*
 * driver TSA
 */
 
struct tsa_data {
	struct device *dev;
	struct device *infos;
	cpm8xx_t *cpm;
};

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))

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

static int __devinit tsa_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct tsa_data *data;
	struct class *class;
	struct device *infos;
	cpm8xx_t *cpm;
	int ret, i, nb_ts, val;
	u32 simode = 0, siram_c, sicr = 0, siram_e, simode_tdma;
	siram_entry *siram_rx, *siram_tx;
	volatile immap_t *immap = ioremap(get_immrbase(),sizeof(*immap));
	const char *scc = NULL;
	const __be32 *ts_c = NULL, *ts_e1 = NULL;
	int len_c = 0, len_scc = 0, len_e1 = 0, offset = 0;

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

	scc = of_get_property(np, "scc_codec", &len_scc);
	if (!scc || (len_scc < sizeof(*scc))) {
		ret = -EINVAL;
		goto err;
	}
	if (sysfs_streq(scc, "SCC4")) siram_c = SIRAM_CSEL_SCC4, sicr |= SICR_SC4;
	else if (sysfs_streq(scc, "SMC2")) siram_c = SIRAM_CSEL_SMC2, simode |= SIMODE_SMC2;
	else siram_c = 0;

	scc = NULL;
	scc = of_get_property(np, "scc_e1", &len_scc);
	if (!scc || (len_scc < sizeof(*scc))) {
		ret = -EINVAL;
		goto err;
	}
	if (sysfs_streq(scc, "SCC4")) siram_e = SIRAM_CSEL_SCC4, sicr |= SICR_SC4;
	else if (sysfs_streq(scc, "SMC2")) siram_e = SIRAM_CSEL_SMC2, simode |= SIMODE_SMC2;
	else siram_e = 0;

	ts_c = of_get_property(np, "ts_codec", &len_c);
	if (!ts_c || (len_c < sizeof(*ts_c))) {
		ret = -EINVAL;
		goto err;
	}
	len_c /= sizeof(__be32);

	ts_e1 = of_get_property(np, "ts_e1", &len_e1);
	if (!ts_e1 || (len_e1 < sizeof(*ts_e1))) {
		ret = -EINVAL;
		goto err;
	}
	len_e1 /= sizeof(__be32);

	/* mise en ordre croissant des TS codec */
	i = 0;
	while (i < len_c) {
		for (i = 1; i < len_c; i++) {
			if (ts_c[i] < ts_c[i - 1]) {
				val = ts_c[i];
				*(__be32 *)&(ts_c[i]) = *(__be32 *)&(ts_c[i - 1]);
				*(__be32 *)&(ts_c[i - 1]) = (__be32)val;
				break;
			}
		}
	}
	/* vérification TS codec differents et ordre croissant */
	for (i = 1; i < len_c; i++) {
		if (ts_c[i] <= ts_c[i - 1])
			break;
	}
	if (!siram_c || !len_c || (i < len_c)) {
		ret = -EINVAL;
		goto err;
	}
	siram_c |= SIRAM_BYT;

	/* vérification TS e1 differents et ordre croissant */
	if (!siram_e || (len_e1 != 2) || (ts_e1[0] > ts_e1[1])) {
		ret = -EINVAL;
		goto err;
	}
	siram_e |= SIRAM_BYT;

	cpm = of_iomap(np, 0);
	if (cpm == NULL) {
		dev_err(dev,"of_iomap CPM failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->cpm = cpm;
	
	class = saf3000_class_get();
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "tsa");
	dev_set_drvdata(infos, data);
	data->infos = infos;

	siram_rx = (siram_entry*)&cpm->cp_siram;
	siram_tx = (siram_entry*)&cpm->cp_siram + 64;

	/* programmation des TimeSlots e1 si avant ceux des codecs */
	val = 0;
	if (ts_e1[1] < ts_c[0]) {
		if (ts_e1[0] != 0) {
			nb_ts = ts_e1[0];
			while (nb_ts > 16) {
				out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(16));
				out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(16));
				offset++;
				nb_ts -= 16;
			}
			out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
			out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
			offset++;
		}
		nb_ts = ts_e1[1] - ts_e1[0] + 1;
		/* programmation des TimeSlots affectes au lien E1 */
		while (nb_ts > 16) {
			out_be32(siram_rx + offset, siram_e | SIRAM_CNT(16));
			out_be32(siram_tx + offset, siram_e | SIRAM_CNT(16));
			offset++;
			nb_ts -= 16;
		}
		out_be32(siram_rx + offset, siram_e | SIRAM_CNT(nb_ts));
		out_be32(siram_tx + offset, siram_e | SIRAM_CNT(nb_ts));
		offset++;
		val = ts_e1[1] + 1;
	}

	/* programmation des TimeSlots codecs */
	if (ts_c[0] != val) {
		nb_ts = ts_c[0] - val;
		while (nb_ts > 16) {
			out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(16));
			out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(16));
			offset++;
			nb_ts -= 16;
		}
		out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
		out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
		offset++;
	}
	/* programmation des TimeSlots affectes aux codecs */
	for (i = 1, nb_ts = 1; i < len_c; i++) {
		ret = ts_c[i] - ts_c[i - 1];
		if (ret == 1)	/* TS consecutifs */
			nb_ts++;
		else {
			/* programmation des TS consecutifs */
			while (nb_ts > 16) {
				out_be32(siram_rx + offset, siram_c | SIRAM_CNT(16));
				out_be32(siram_tx + offset, siram_c | SIRAM_CNT(16));
				offset++;
				nb_ts -= 16;
			}
			out_be32(siram_rx + offset, siram_c | SIRAM_CNT(nb_ts));
			out_be32(siram_tx + offset, siram_c | SIRAM_CNT(nb_ts));
			offset++;
			nb_ts = 1;
			/* programmation des TS non utilises */
			ret--;		/* intervalle entre TS utilises */
			while (ret > 16) {
				out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(16));
				out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(16));
				offset++;
				ret -= 16;
			}
			out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(ret));
			out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(ret));
			offset++;
		}
	}
	/* programmation des derniers TimeSlots affectes aux codecs */
	while (nb_ts > 16) {
		out_be32(siram_rx + offset, siram_c | SIRAM_CNT(16));
		out_be32(siram_tx + offset, siram_c | SIRAM_CNT(16));
		offset++;
		nb_ts -= 16;
	}
	if (ts_e1[1] < ts_c[0])
		siram_c |= SIRAM_LST;
	out_be32(siram_rx + offset, siram_c | SIRAM_CNT(nb_ts));
	out_be32(siram_tx + offset, siram_c | SIRAM_CNT(nb_ts));
	offset++;
	
	/* programmation des TimeSlots e1 si apres ceux des codecs */
	if (ts_e1[1] > ts_c[len_c - 1]) {
		val = ts_c[len_c - 1] + 1;
		if (ts_e1[0] > val) {
			nb_ts = ts_e1[0] - val;
			while (nb_ts > 16) {
				out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(16));
				out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(16));
				offset++;
				nb_ts -= 16;
			}
			out_be32(siram_rx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
			out_be32(siram_tx + offset, SIRAM_BYT | SIRAM_CNT(nb_ts));
			offset++;
		}
		nb_ts = ts_e1[1] - ts_e1[0] + 1;
		/* programmation des TimeSlots affectes au lien E1 */
		while (nb_ts > 16) {
			out_be32(siram_rx + offset, siram_e | SIRAM_CNT(16));
			out_be32(siram_tx + offset, siram_e | SIRAM_CNT(16));
			offset++;
			nb_ts -= 16;
		}
		siram_e |= SIRAM_LST;
		out_be32(siram_rx + offset, siram_e | SIRAM_CNT(nb_ts));
		out_be32(siram_tx + offset, siram_e | SIRAM_CNT(nb_ts));
	}

	simode_tdma = (SIMODE_SDM_NORM | SIMODE_RFSD_0 | SIMODE_CRT | SIMODE_FE | SIMODE_GM | SIMODE_TFSD_0);
	out_be32(&cpm->cp_simode, (in_be32(&cpm->cp_simode) & ~SIMODE_TDMa(SIMODE_TDM_MASK)) | SIMODE_TDMa(simode_tdma) | simode);
	
	setbits32(&cpm->cp_sicr, sicr);
	
	out_8(&cpm->cp_sigmr, SIGMR_ENa | SIGMR_RDM_STATIC_TDMa);
	
	if ((ret=device_create_file(infos, &dev_attr_debug))) {
		goto err_unfile;
	}
	
	dev_info(dev,"driver TSA added.\n");
	
	return 0;

err_unfile:
	device_remove_file(infos, &dev_attr_debug);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->cpm), data->cpm = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit tsa_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct tsa_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(infos, &dev_attr_debug);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->cpm), data->cpm = NULL;
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

static struct of_platform_driver tsa_driver = {
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
	return of_register_platform_driver(&tsa_driver);
}
module_init(tsa_init);

MODULE_AUTHOR("C.LEROY - P.VASSEUR");
MODULE_DESCRIPTION("Driver for TSA on MPC8xx ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mpc8xx-tsa");
