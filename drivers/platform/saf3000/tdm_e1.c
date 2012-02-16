/*
 * tdm_e1.c - Driver for MPC 8xx TDM for E1
 *
 * Authors: Patrick VASSEUR
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
#include <linux/poll.h>
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
#include <asm/cpm1.h>
#include <asm/irq.h>
#include <saf3000/saf3000.h>


/*
 * driver TDM
 */
#define NB_CANAUX_AUDIO		31
#define NB_BYTE_BY_MS		(1000 / 125)
#define NB_BYTE_BY_5_MS		(5000 / 125)

#define PCM_E1_MINOR		241
#define PCM_NB_TXBD		3
#define PCM_NB_BYTE_TXBD	(NB_BYTE_BY_MS * NB_CANAUX_AUDIO)
#define PCM_NB_RXBD		2
#define PCM_NB_BYTE_RXBD	(NB_BYTE_BY_5_MS * NB_CANAUX_AUDIO)
#define PCM_NB_BUF_PACKET	3
#define PCM_NB_BYTE_PACKET	(NB_BYTE_BY_5_MS * NB_CANAUX_AUDIO)

struct em_buf{
	int	num_packet_wr;
	int	octet_packet[PCM_NB_BUF_PACKET];
	char	*packet[PCM_NB_BUF_PACKET];
};

struct rec_buf{
	int	octet_packet;
	char	*packet;
};

struct tdm_e1_data {
	struct device		*dev;
	struct device		*infos;
	smc_t			*cp_smc;
	int			irq;
	sccp_t			*pram;
	struct cpm_buf_desc __iomem	*tx_bd;
	struct cpm_buf_desc __iomem	*rx_bd;
	char			*rx_buf[PCM_NB_RXBD];
	char			*tx_buf[PCM_NB_TXBD];
	int			ix_tx;		/* descripteur emission à écrire */
	int			ix_tx_user;	/* index ecriture user */
	int			ix_tx_trft;	/* index ecriture transfert */
	int			ix_rx;
	unsigned int		packet_lost;
	unsigned int		packet_silence;
	struct em_buf		em;
	struct rec_buf		rec;
	u32			command;
	wait_queue_head_t	read_wait;
	unsigned long		open;
	unsigned long		time;
};

static struct tdm_e1_data *data;

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))

char pattern_silence_e1[PCM_NB_BYTE_TXBD] = {
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5,
	0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5, 0xD5
};

static ssize_t fs_attr_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tdm_e1_data *data = dev_get_drvdata(dev);
	int l=0;
	
#define show_var(fmt, ptr, var) 	l += snprintf(buf+l, PAGE_SIZE-l, #var " %X\n",in_##fmt(&ptr->var))

	l+= snprintf(buf+l, PAGE_SIZE-l, "### SCC ###\n");
	show_var(be16, data->cp_smc, smc_smcmr);
	show_var(8, data->cp_smc, smc_smce);
	show_var(8, data->cp_smc, smc_smcm);
	
	l+= snprintf(buf+l, PAGE_SIZE-l, "### TX BD ###\n");
	show_var(be16, data->tx_bd, cbd_sc);
	show_var(be16, data->tx_bd, cbd_datlen);
	l+= snprintf(buf+l, PAGE_SIZE-l, "### RX BD ###\n");
	show_var(be16, data->rx_bd, cbd_sc);
	show_var(be16, data->rx_bd, cbd_datlen);

	l+= snprintf(buf+l, PAGE_SIZE-l, "### PARAMETER RAM ###\n");
	show_var(be16, data->pram, scc_rbase);
	show_var(be16, data->pram, scc_tbase);
	show_var(8, data->pram, scc_rfcr);
	show_var(8, data->pram, scc_tfcr);
	show_var(be16, data->pram, scc_mrblr);
	show_var(be32, data->pram, scc_rstate);
	show_var(be32, data->pram, scc_idp);
	show_var(be16, data->pram, scc_rbptr);
	show_var(be16, data->pram, scc_ibc);
	show_var(be32, data->pram, scc_rxtmp);
	show_var(be32, data->pram, scc_tstate);
	show_var(be32, data->pram, scc_tdp);
	show_var(be16, data->pram, scc_tbptr);
	show_var(be16, data->pram, scc_tbc);
	show_var(be32, data->pram, scc_txtmp);
	show_var(be32, data->pram, scc_rcrc);
	show_var(be32, data->pram, scc_tcrc);
	
	return l;
}
static DEVICE_ATTR(debug, S_IRUGO, fs_attr_debug_show, NULL);

static ssize_t fs_attr_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tdm_e1_data *data = dev_get_drvdata(dev);
	int len = 0;

	len = snprintf(buf, PAGE_SIZE, "Paquets perdus : %d\nPaquets silence : %d\n",
			data->packet_lost, data->packet_silence);
	
	return len;
}
static DEVICE_ATTR(stat, S_IRUGO, fs_attr_stat_show, NULL);
	
static irqreturn_t pcm_e1_interrupt(s32 irq, void *context)
{
	struct tdm_e1_data *data = (struct tdm_e1_data*)context;
	irqreturn_t ret = IRQ_NONE;
	int i;
	char lct = in_8(&data->cp_smc->smc_smce);

	if (lct & SMCM_TXE) {
		struct cpm_buf_desc __iomem *ad_bd;
		clrbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
		cpm_command(data->command, CPM_CR_INIT_TX);
		/* initialisation des descripteurs Tx */
		for (i = 0; i < PCM_NB_TXBD; i++) {
			ad_bd = data->tx_bd + i;
			if (i != (PCM_NB_TXBD - 1)) {
				memcpy(data->tx_buf[i], pattern_silence_e1, PCM_NB_BYTE_TXBD);
				out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT);
			}
			else
				out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
		}
		data->ix_tx = PCM_NB_TXBD - 1;
		/* initialisation structure emission */
		for (i = 0; i < PCM_NB_BUF_PACKET; i++)
			data->em.octet_packet[i] = 0;
		data->ix_tx_user = 0;
		data->ix_tx_trft = 0;
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
		pr_info("TDM-E1 Interrupt TX underrun\n");
	}

	if (lct & SMCM_TX) {
		i = data->em.octet_packet[data->ix_tx_trft];
		if (i >= PCM_NB_BYTE_TXBD) {
			memcpy(data->tx_buf[data->ix_tx], &data->em.packet[data->ix_tx_trft][PCM_NB_BYTE_PACKET - i], PCM_NB_BYTE_TXBD);
			i -= PCM_NB_BYTE_TXBD;
			data->em.octet_packet[data->ix_tx_trft] = i;
			if (i < PCM_NB_BYTE_TXBD) {
				data->em.octet_packet[data->ix_tx_trft] = 0;
				data->ix_tx_trft++;
				if (data->ix_tx_trft == PCM_NB_BUF_PACKET)
					data->ix_tx_trft = 0;
			}
		}
		else {
			memcpy(data->tx_buf[data->ix_tx], pattern_silence_e1, PCM_NB_BYTE_TXBD);
			data->packet_silence++;
		}
		setbits16(&(data->tx_bd + data->ix_tx)->cbd_sc, BD_SC_READY);
		data->ix_tx++;
		if (data->ix_tx == PCM_NB_TXBD) data->ix_tx = 0;
		data->time++;
	}
	if (lct & SMCM_RX) {
		if (data->rec.octet_packet) data->packet_lost++;
		memcpy(data->rec.packet, data->rx_buf[data->ix_rx], PCM_NB_BYTE_RXBD);
		data->rec.octet_packet = PCM_NB_BYTE_PACKET;
		setbits16(&(data->rx_bd + data->ix_rx)->cbd_sc, BD_SC_EMPTY);
		data->ix_rx++;
		if (data->ix_rx == PCM_NB_RXBD) data->ix_rx = 0;
		if (data->open)		/* si device /dev/pcm utilisé */
			wake_up(&data->read_wait);
	}
	if (lct & SMCM_BSY) {
		pr_info("TDM-E1 Interrupt RX Busy\n");
	}
	setbits8(&data->cp_smc->smc_smce, lct);
	
	ret = IRQ_HANDLED;
	return ret;
}

static unsigned int pcm_e1_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	
	poll_wait(file, &data->read_wait, wait);
	if (data->rec.octet_packet)
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t pcm_e1_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	
	if (count && (data->em.octet_packet[data->ix_tx_user] == 0)) {
		if (count > PCM_NB_BYTE_PACKET) count = PCM_NB_BYTE_PACKET;
		ret = access_ok(VERFIFY_READ, buf, count);
		if (ret == 0) {
			return -EFAULT;
		}
		ret = __copy_from_user(data->em.packet[data->ix_tx_user], buf, count);
		data->em.octet_packet[data->ix_tx_user++] = count;
		if (data->ix_tx_user == PCM_NB_BUF_PACKET) data->ix_tx_user = 0;
	}
	else
		count = 0;

	return count;
}

static ssize_t pcm_e1_aio_write(struct kiocb *iocb, const struct iovec *iov,
			  unsigned long nr_segs, loff_t pos)
{
	int i, canal, nb_byte = 0;
	u8 *pread, *pwrite;
	
	if (nr_segs != NB_CANAUX_AUDIO)
		return -EINVAL;
		
	for (canal = 0; canal < NB_CANAUX_AUDIO; canal++) {
		/* test taille buffer >= taille nécessaire pour transfert */
		if (iov->iov_len < NB_BYTE_BY_5_MS)
			return -EINVAL;
		if (!access_ok(VERIFY_READ, iov->iov_base, NB_BYTE_BY_5_MS))
			return -EFAULT;
      		pwrite = &data->em.packet[data->ix_tx_user][canal];
      		pread = (u8 *)iov->iov_base;
      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
			*pwrite = *pread;
			pread++;
			pwrite += NB_CANAUX_AUDIO;
		}
		nb_byte += NB_BYTE_BY_5_MS;
		iov++;
	}

	data->em.octet_packet[data->ix_tx_user++] = nb_byte;
	if (data->ix_tx_user == PCM_NB_BUF_PACKET) data->ix_tx_user = 0;

	return(nb_byte);
}

static ssize_t pcm_e1_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	
	if ((file->f_flags & O_NONBLOCK) == 0)
		wait_event(data->read_wait, data->rec.octet_packet);

	if (count && data->rec.octet_packet) {
		if (count > PCM_NB_BYTE_PACKET) count = PCM_NB_BYTE_PACKET;
		ret = access_ok(VERFIFY_WRITE, buf, count);
		if (ret == 0) {
			return -EFAULT;
		}
		ret = __copy_to_user((void*)buf, data->rec.packet, count);
		data->rec.octet_packet = 0;
	}
	else
		count = 0;
	
	return count;
}

static ssize_t pcm_e1_aio_read(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	int i, canal, nb_byte = 0;
	u8 *pread, *pwrite;
	
	if (nr_segs != NB_CANAUX_AUDIO)
		return -EINVAL;
		
	for (canal = 0; canal < NB_CANAUX_AUDIO; canal++) {
		/* test taille buffer >= taille nécessaire pour transfert */
		if (iov->iov_len < NB_BYTE_BY_5_MS)
			return -EINVAL;
		if (!access_ok(VERIFY_WRITE, iov->iov_base, NB_BYTE_BY_5_MS))
			return -EFAULT;
      		pread = &data->rec.packet[canal];
      		pwrite = (u8 *)iov->iov_base;
      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
			*pwrite = *pread;
			pwrite++;
			pread += NB_CANAUX_AUDIO;
		}
		nb_byte += NB_BYTE_BY_5_MS;
		iov++;
	}

	data->rec.octet_packet = 0;

	return(nb_byte);
}

static int pcm_e1_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct info info;
	
	switch (cmd) {
	case SAF3000_PCM_TIME:
		info.info = data->time;
		break;
	case SAF3000_PCM_LOST:
		info.info = data->packet_lost;
		break;
	case SAF3000_PCM_SILENT:
		info.info = data->packet_silence;
		break;
	default:
		ret = -ENOTTY;
		break;
	}
	if (ret == 0)
		ret = __copy_to_user((struct info *)arg, &info, _IOC_SIZE(cmd));
	return ret;
}

static int pcm_e1_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &data->open))
		return -EBUSY;
	return 0;
}

static int pcm_e1_release(struct inode *inode, struct file *file)
{
	wake_up(&data->read_wait);
	clear_bit(0, &data->open);
	return 0;
}

static const struct file_operations pcm_e1_fops = {
	.owner		= THIS_MODULE,
	.poll		= pcm_e1_poll,
	.write		= pcm_e1_write,
	.aio_write	= pcm_e1_aio_write,
	.read		= pcm_e1_read,
	.aio_read	= pcm_e1_aio_read,
	.ioctl		= pcm_e1_ioctl,
	.open		= pcm_e1_open,
	.release	= pcm_e1_release,
};

static struct miscdevice pcm_e1_miscdev = {
	.minor	= PCM_E1_MINOR,
	.name	= "pcm-e1",
	.fops	= &pcm_e1_fops,
};

static int __devinit tdm_e1_probe(struct of_device *ofdev, const struct of_device_id *match)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct class *class;
	struct device *infos;
	smc_t *cp_smc;
	int ret, i;
	int irq;
	unsigned short bds_ofs;
	int len;
	const u32 *prop;
	dma_addr_t dma_addr = 0;
	struct cpm_buf_desc __iomem *ad_bd;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	/* initialisation structure emission */
	for (i = 0; i < PCM_NB_BUF_PACKET; i++) {
		data->em.packet[i] = kzalloc(PCM_NB_BYTE_PACKET, GFP_KERNEL);
		if (!data->em.packet[i]) {
			ret = -ENOMEM;
			goto err;
		}
		data->em.octet_packet[i] = 0;
	}
	/* initialisation structure reception */
	data->rec.packet = kzalloc(PCM_NB_BYTE_PACKET, GFP_KERNEL);
	if (!data->rec.packet) {
		ret = -ENOMEM;
		goto err;
	}
	data->rec.octet_packet = 0;
	init_waitqueue_head(&data->read_wait);
	dev_set_drvdata(dev, data);
	data->dev = dev;

	prop = of_get_property(np, "fsl,cpm-command", &len);
	if (!prop || len != 4) {
		dev_err(dev, "CPM UART %s has no/invalid fsl,cpm-command property.\n", np->name);
		ret = -EINVAL;
		goto err;
	}
	data->command = *prop;

	cp_smc = of_iomap(np, 0);
	if (cp_smc == NULL) {
		dev_err(dev,"of_iomap CPM failed\n");
		ret = -ENOMEM;
		goto err;
	}
	data->cp_smc = cp_smc;
	
	class = saf3000_class_get();
		
	infos = device_create(class, dev, MKDEV(0, 0), NULL, "tdm_e1");
	dev_set_drvdata(infos, data);
	data->infos = infos;
	
	ret = misc_register(&pcm_e1_miscdev);
	if (ret) {
		dev_err(dev, "pcm: cannot register miscdev on minor=%d (err=%d)\n", PCM_E1_MINOR, ret);
		goto err_unfile;
	}
	
	irq = of_irq_to_resource(np, 0, NULL);
	if (!irq) {
		dev_err(dev,"no irq defined\n");
		ret = -EINVAL;
		goto err_unfile;
	}
	data->irq = irq;
	ret = request_irq(irq, pcm_e1_interrupt, 0, "tdm_e1", data);
	
	data->pram = of_iomap(np, 1);

	ret = sizeof(cbd_t) * (PCM_NB_TXBD + PCM_NB_RXBD);
	bds_ofs = cpm_dpalloc(ret, 8);
	data->rx_bd = cpm_dpram_addr(bds_ofs);
	data->tx_bd = cpm_dpram_addr(bds_ofs + (sizeof(*data->tx_bd) * PCM_NB_RXBD));
	
	/* Initialize parameter ram. */
	out_be16(&data->pram->scc_rbase, (void*)data->rx_bd-cpm_dpram_addr(0));
	out_be16(&data->pram->scc_tbase, (void*)data->tx_bd-cpm_dpram_addr(0));
	out_8(&data->pram->scc_tfcr, CPMFCR_EB);
	out_8(&data->pram->scc_rfcr, CPMFCR_EB);
	out_be16(&data->pram->scc_mrblr, (PCM_NB_BYTE_TXBD > PCM_NB_BYTE_RXBD) ? PCM_NB_BYTE_TXBD : PCM_NB_BYTE_RXBD);
	
	cpm_command(data->command, CPM_CR_INIT_TRX);
	out_be16(&data->cp_smc->smc_smcmr, 0);
	
	/* initialisation des descripteurs Tx */
	for (i = 0; i < PCM_NB_TXBD; i++) {
		ad_bd = data->tx_bd + i;
		data->tx_buf[i] = dma_alloc_coherent(dev, L1_CACHE_ALIGN(PCM_NB_BYTE_TXBD), &dma_addr, GFP_KERNEL);
		out_be16(&ad_bd->cbd_datlen, PCM_NB_BYTE_TXBD);
		out_be32(&ad_bd->cbd_bufaddr, dma_addr);
		if (i != (PCM_NB_TXBD - 1)) {
			memcpy(data->tx_buf[i], pattern_silence_e1, PCM_NB_BYTE_TXBD);
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT);
		}
		else
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
	}
	data->ix_tx = PCM_NB_TXBD - 1;
	
	setbits8(&data->cp_smc->smc_smcm, SMCM_TXE | SMCM_TX);
	setbits8(&data->cp_smc->smc_smce, SMCM_TXE | SMCM_TX);

	/* initialisation des descripteurs Rx */
	for (i = 0; i < PCM_NB_RXBD; i++) {
		ad_bd = data->rx_bd + i;
		data->rx_buf[i] = dma_alloc_coherent(dev, L1_CACHE_ALIGN(PCM_NB_BYTE_RXBD), &dma_addr, GFP_KERNEL);
		out_be16(&ad_bd->cbd_datlen, PCM_NB_BYTE_RXBD);
		out_be32(&ad_bd->cbd_bufaddr, dma_addr);
		if (i != (PCM_NB_RXBD - 1))
			out_be16(&ad_bd->cbd_sc, BD_SC_EMPTY | BD_SC_CM | BD_SC_INTRPT);
		else
			out_be16(&ad_bd->cbd_sc, BD_SC_EMPTY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
	}
	data->ix_rx = 0;
	
	setbits8(&data->cp_smc->smc_smcm, SMCM_RX | SMCM_BSY);
	setbits8(&data->cp_smc->smc_smce, SMCM_RX | SMCM_BSY);

	out_be16(&data->cp_smc->smc_smcmr, smcr_mk_clen(15) | SMCMR_SM_TRANS);
	setbits16(&data->cp_smc->smc_smcmr, SMCMR_REN);
	setbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);

	ret = device_create_file(infos, &dev_attr_stat);
	if (ret) {
		goto err_unfile;
	}
	ret = device_create_file(infos, &dev_attr_debug);
	if (ret) {
		goto err_unfile;
	}

	dev_info(dev, "driver TDM-E1 added.\n");
	
	return 0;

err_unfile:
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_e1_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->cp_smc), data->cp_smc = NULL;
	dev_set_drvdata(dev, NULL);
	kfree(data);
err:
	return ret;
}

static int __devexit tdm_e1_remove(struct of_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct tdm_e1_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_e1_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	iounmap(data->cp_smc), data->cp_smc = NULL;
	dev_set_drvdata(dev, NULL);
	misc_deregister(&pcm_e1_miscdev);
	kfree(data);
	
	dev_info(dev,"driver TDM-E1 removed.\n");
	return 0;
}

static const struct of_device_id tdm_e1_match[] = {
	{
		.compatible = "fsl,cpm1-smc-tdm",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tdm_e1_match);

static struct of_platform_driver tdm_e1_driver = {
	.probe		= tdm_e1_probe,
	.remove		= __devexit_p(tdm_e1_remove),
	.driver		= {
			  	.name		= "tdm-e1",
			  	.owner		= THIS_MODULE,
			  	.of_match_table	= tdm_e1_match,
			  },
};

static int __init tdm_e1_init(void)
{
	return of_register_platform_driver(&tdm_e1_driver);
}
module_init(tdm_e1_init);

MODULE_AUTHOR("P.VASSEUR");
MODULE_DESCRIPTION("Driver for TDM-E1 on MPC8xx");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mpc8xx-tdm-e1");
