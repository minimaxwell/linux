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
	int ret, i, nb_ts;
	u32 simode, siram = 0;
	siram_entry *siram_rx, *siram_tx;
	volatile immap_t *immap = ioremap(get_immrbase(),sizeof(*immap));
	const char *param = NULL;
	const __be32 *ts = NULL;
	int len = 0, len_param = 0, offset = 0;

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

	param = of_get_property(np, "param", &len_param);
	ts = of_get_property(np, "ts_codec", &len);
	if (!param || (len_param < sizeof(*param)) || !ts || (len < sizeof(*ts))) {
		ret = -EINVAL;
		goto err;
	}
	
	if (sysfs_streq(param, "SCC1")) siram = SIRAM_CSEL_SCC1;
	else if (sysfs_streq(param, "SCC2")) siram = SIRAM_CSEL_SCC2;
	else if (sysfs_streq(param, "SCC3")) siram = SIRAM_CSEL_SCC3;
	else if (sysfs_streq(param, "SCC4")) siram = SIRAM_CSEL_SCC4;
	else if (sysfs_streq(param, "SMC1")) siram = SIRAM_CSEL_SMC1;
	else if (sysfs_streq(param, "SMC2")) siram = SIRAM_CSEL_SMC2;
	len /= sizeof(__be32);
	for (i = 1; i < len; i++) {
		if (ts[i] <= ts[i - 1])
			break;
	}
	if (!siram || !len || (i < len)) {
		ret = -EINVAL;
		goto err;
	}
	siram |= SIRAM_BYT;

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
	/* programmation des premiers TimeSlots si non utilises */
	if (ts[0] != 0) {
		out_be32(siram_rx + offset, SIRAM_CNT(ts[0]) | SIRAM_BYT);
		out_be32(siram_tx + offset, SIRAM_CNT(ts[0]) | SIRAM_BYT);
		offset++;
	}
	/* programmation des TimeSlots affectes aux codecs */
	for (i = 1, nb_ts = 1; i < len; i++) {
		ret = ts[i] - ts[i - 1];
		if (ret == 1)	/* TS consecutifs */
			nb_ts++;
		else {
			/* programmation des TS consecutifs */
			out_be32(siram_rx + offset, siram | SIRAM_CNT(nb_ts));
			out_be32(siram_tx + offset, siram | SIRAM_CNT(nb_ts));
			offset++;
			nb_ts = 1;
			/* programmation des TS non utilises */
			out_be32(siram_rx + offset, SIRAM_CNT(ret) | SIRAM_BYT);
			out_be32(siram_tx + offset, SIRAM_CNT(ret) | SIRAM_BYT);
			offset++;
		}
	}
	/* programmation des derniers TimeSlots affectes aux codecs */
	siram |= SIRAM_LST;
	out_be32(siram_rx + ts[i], siram | SIRAM_CNT(nb_ts));
	out_be32(siram_tx + ts[i], siram | SIRAM_CNT(nb_ts));
	
	simode = (SIMODE_SDM_NORM | SIMODE_RFSD_0 | SIMODE_CRT | SIMODE_FE | SIMODE_GM | SIMODE_TFSD_0);
	out_be32(&cpm->cp_simode, (in_be32(&cpm->cp_simode) & ~SIMODE_TDMa(SIMODE_TDM_MASK)) | SIMODE_TDMa(simode));
	
	setbits32(&cpm->cp_sicr, SICR_SC4);
	
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
