/*
 * tdm.c - Driver for MPC 8xx TDM
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
#define NB_CANAUX_CODEC		12
#define IDENT_CODEC		(1 << 0)
#define NB_CANAUX_E1		31
#define IDENT_E1		(1 << 1)
#define NB_CANAUX_MAX		(NB_CANAUX_CODEC + NB_CANAUX_E1)
#define NB_BYTE_BY_MS		(1000 / 125)
#define NB_BYTE_BY_5_MS		(5000 / 125)

#define PCM_CODEC_MINOR		240
#define PCM_E1_MINOR		241
#define PCM_NB_TXBD		16
#define PCM_MASK_TXBD		15
#define PCM_NB_RXBD		2

#define TYPE_SCC1		1
#define TYPE_SCC2		2
#define TYPE_SCC3		3
#define TYPE_SCC4		4
#define TYPE_SMC1		5
#define TYPE_SMC2		6
#define NAME_CODEC_DEVICE	"pcm"
//#define NAME_CODEC_DEVICE	"pcm_codec"
#define NAME_E1_DEVICE		"pcm_e1"

#define CARTE_MCR3K_2G		1
#define CARTE_MIAE		2

/* gestion ligne à retard */
#define DELAY_NB_BYTE		(1000 * NB_BYTE_BY_MS)
#define DELAY_NB_LINE_EM	16
#define DELAY_NB_LINE_REC	16
#define DELAY_NB_LINE		(DELAY_NB_LINE_EM + DELAY_NB_LINE_REC)

/*
struct delay_buf {
	int	ix_wr;
	int	ix_rd;
	char	*buf[DELAY_NB_BYTE];
};
*/
struct tdm_data {
	struct device		*dev;
	struct device		*infos;
	scc_t			*cp_scc;
	smc_t			*cp_smc;
	int			carte;
	int			irq;
	sccp_t			*pram;
	struct cpm_buf_desc __iomem	*tx_bd;
	struct cpm_buf_desc __iomem	*rx_bd;
	char			*rx_buf[PCM_NB_RXBD];
	char			*tx_buf[PCM_NB_TXBD];
	int			octet_em[PCM_NB_TXBD];
	unsigned char		flags;		/* type de lien serie */
	unsigned char		ix_tx;		/* descripteur emission à transmettre */
	unsigned char		ix_rx;		/* descripteur reception en cours */
	unsigned char		ix_rx_lct;	/* descripteur reception à lire */
	int			octet_recu;	/* nombre d'octets recus dispo */
	unsigned char		nb_canal;	/* nombre de canaux à gerer */
	char			*packet_repos;
	u32			command;
	wait_queue_head_t	read_wait;
	unsigned long		packet_lost;
	unsigned long		packet_silence;
	unsigned long		open;
	unsigned long		time;
//	struct delay_buf	delay[DELAY_NB_LINE];
};

static struct tdm_data *data_codec;
static struct tdm_data *data_e1;

#define EXTRACT(x,dec,bits) ((x>>dec) & ((1<<bits)-1))

static ssize_t fs_attr_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tdm_data *data = dev_get_drvdata(dev);
	int l=0;
	
#define show_var(fmt, ptr, var) 	l += snprintf(buf+l, PAGE_SIZE-l, #var " %X\n",in_##fmt(&ptr->var))

	l+= snprintf(buf+l, PAGE_SIZE-l, "### SCC ###\n");
	show_var(be32, data->cp_scc, scc_gsmrl);
	show_var(be32, data->cp_scc, scc_gsmrh);
	show_var(be16, data->cp_scc, scc_psmr);
	show_var(be16, data->cp_scc, scc_todr);
	show_var(be16, data->cp_scc, scc_dsr);
	show_var(be16, data->cp_scc, scc_scce);
	show_var(be16, data->cp_scc, scc_sccm);
	show_var(8, data->cp_scc, scc_sccs);
	
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
	struct tdm_data *data = dev_get_drvdata(dev);
	int len = 0;

	len = snprintf(buf, PAGE_SIZE, "Paquets perdus : %ld\nPaquets silence : %ld\n",
			data->packet_lost, data->packet_silence);
	
	return len;
}
static DEVICE_ATTR(stat, S_IRUGO, fs_attr_stat_show, NULL);
	
static irqreturn_t pcm_interrupt(s32 irq, void *context)
{
	struct tdm_data *data = (struct tdm_data*)context;
	irqreturn_t ret = IRQ_NONE;
	int i = 0;
	short lct, mask_tx, mask_rx, mask_bsy, mask_txe;
	
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2)) {
		lct = in_8(&data->cp_smc->smc_smce);
		out_8(&data->cp_smc->smc_smce, lct);
		mask_tx = SMCM_TX;
		mask_rx= SMCM_RX;
		mask_bsy = SMCM_BSY;
		mask_txe = SMCM_TXE;
	}
	else {
		lct = in_be16(&data->cp_scc->scc_scce);
		out_be16(&data->cp_scc->scc_scce, lct);
		mask_tx = UART_SCCM_TX;
		mask_rx= UART_SCCM_RX;
		mask_bsy = UART_SCCM_BSY;
		mask_txe = 0;
	}

	if (lct & mask_txe) {
		struct cpm_buf_desc __iomem *ad_bd;
		clrbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
		cpm_command(data->command, CPM_CR_INIT_TX);
		/* initialisation des descripteurs Tx */
		for (i = 0; i < PCM_NB_TXBD; i++) {
			memcpy(data->tx_buf[i], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
			data->octet_em[i] = 0;
			ad_bd = data->tx_bd + i;
			if (i != (PCM_NB_TXBD - 1)) {
				out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT);
			}
			else
				out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
		}
		data->ix_tx = 2;
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
		pr_info("TDM-E1 Interrupt TX underrun\n");
	}

	if (lct & mask_tx) {
		if (data->octet_em[data->ix_tx] == 0) {
			memcpy(data->tx_buf[data->ix_tx], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
			data->packet_silence++;
		}
		setbits16(&(data->tx_bd + data->ix_tx)->cbd_sc, BD_SC_READY);
		data->octet_em[data->ix_tx] = 0;
		data->ix_tx = (data->ix_tx + 1) & PCM_MASK_TXBD;
		data->time++;
	}

	if (lct & mask_rx) {
		gest_led_debug(1, 1);
		if (data->octet_recu == (NB_BYTE_BY_5_MS * data->nb_canal))
			data->packet_lost++;
		data->octet_recu = (NB_BYTE_BY_5_MS * data->nb_canal);
		setbits16(&(data->rx_bd + data->ix_rx)->cbd_sc, BD_SC_EMPTY);
		data->ix_rx_lct = data->ix_rx++;
		if (data->ix_rx == PCM_NB_RXBD) data->ix_rx = 0;
		if (data->open)		/* si device /dev/pcm utilisé */
			wake_up(&data->read_wait);
		gest_led_debug(1, 0);
	}

	if (lct & mask_bsy) {
		pr_info("TDM Interrupt RX Busy\n");
	}

	ret = IRQ_HANDLED;

	return ret;
}

static unsigned int pcm_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	struct tdm_data *data = NULL;
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	poll_wait(file, &data->read_wait, wait);
	if (data->octet_recu == (NB_BYTE_BY_5_MS * data->nb_canal))
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t pcm_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret, ix_wr, ix, j;
	struct tdm_data *data = NULL;
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	ret = access_ok(VERFIFY_READ, buf, count);
	if ((ret == 0) || (count != (NB_BYTE_BY_5_MS * data->nb_canal))) {
		return -EFAULT;
	}
	
	/* ecriture des canaux */
	ix = (data->ix_tx + 2) & PCM_MASK_TXBD;
	for (j = 0; j < (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS); j++) {
		ix_wr = NB_BYTE_BY_MS * data->nb_canal * j;
		__copy_from_user(data->tx_buf[ix], &buf[ix_wr], (NB_BYTE_BY_MS * data->nb_canal));
		data->octet_em[ix++] = NB_BYTE_BY_MS * data->nb_canal;
		ix &= PCM_MASK_TXBD;
	}

	return count;
}

static ssize_t pcm_aio_write(struct kiocb *iocb, const struct iovec *iov,
			  unsigned long nr_segs, loff_t pos)
{
	int i, ix_wr, canal, nb_byte = 0, j, ix;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(iocb->ki_filp->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	if (nr_segs != data->nb_canal)
		return -EINVAL;
		
	/* ecriture des canaux */
	ix = (data->ix_tx + 2) & PCM_MASK_TXBD;
	for (canal = 0, ix_wr = 0; canal < NB_CANAUX_CODEC; canal++, ix_wr++) {
		/* test taille buffer >= taille nécessaire pour transfert */
		if (iov->iov_len < NB_BYTE_BY_5_MS)
			return -EINVAL;
		if (!access_ok(VERIFY_READ, iov->iov_base, NB_BYTE_BY_5_MS))
			return -EFAULT;
		pread = (u8 *)iov->iov_base;
		for (j = 0; j < (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS); j++) {
			pwrite = &data->tx_buf[(ix + j) & PCM_MASK_TXBD][ix_wr];
	      		for (i = 0; i < NB_BYTE_BY_MS; i++) {
				*pwrite = *pread;
				pread++;
				pwrite += data->nb_canal;
			}
		}
		nb_byte += NB_BYTE_BY_MS;
		iov++;
	}
	for (j = 0; j < (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS); j++)
      		data->octet_em[(ix + j) & PCM_MASK_TXBD] += nb_byte;

	nb_byte *= (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS);
	return(nb_byte);
}

static ssize_t pcm_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	struct tdm_data *data = NULL;
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	ret = access_ok(VERFIFY_WRITE, buf, count);
	if ((ret == 0) || (count != (NB_BYTE_BY_5_MS * data->nb_canal))) {
		return -EFAULT;
	}

	if ((file->f_flags & O_NONBLOCK) == 0)
		wait_event(data->read_wait, data->octet_recu);
		
	/* lecture dees canaux */
	ret = __copy_to_user((void*)buf, data->rx_buf[data->ix_rx_lct], count);
	data->octet_recu = 0;

	return count;
}

static ssize_t pcm_aio_read(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	int i, canal, nb_byte = 0, ix_lct = 0;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(iocb->ki_filp->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	if (nr_segs != data->nb_canal)
		return -EINVAL;
		
	if ((iocb->ki_filp->f_flags & O_NONBLOCK) == 0)
		wait_event(data->read_wait, data->octet_recu);

	/* lecture des canaux */
	for (canal = 0, ix_lct = 0; canal < data->nb_canal; canal++, ix_lct++) {
		/* test taille buffer >= taille nécessaire pour transfert */
		if (iov->iov_len < NB_BYTE_BY_5_MS)
			return -EINVAL;
		if (!access_ok(VERFIFY_WRITE, iov->iov_base, NB_BYTE_BY_5_MS))
			return -EFAULT;
		pwrite = (u8 *)iov->iov_base;
      		pread = &data->rx_buf[data->ix_rx_lct][ix_lct];
      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
			*pwrite = *pread;
			pwrite++;
			pread += data->nb_canal;
		}
		nb_byte += NB_BYTE_BY_5_MS;
		iov++;
	}
	if (nb_byte == (NB_BYTE_BY_5_MS * data->nb_canal))
		data->octet_recu = 0;	

	return(nb_byte);
}

static int pcm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0/*, ix*/;
	unsigned long info;
//	struct pose_delay delay;
	struct tdm_data *data = NULL;
	int minor = MINOR(inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	switch (cmd) {
		case SAF3000_PCM_TIME: info = data->time; break;
		case SAF3000_PCM_LOST: info = data->packet_lost; break;
		case SAF3000_PCM_SILENT: info = data->packet_silence; break;
		default: ret = -ENOTTY; goto mcr3k; break;
	}
	ret = __copy_to_user((void*)arg, &info, _IOC_SIZE(cmd));
	return ret;

mcr3k:
/*	if (data->carte == CARTE_MCR3K_2G) {
		delay.ident = 0;
		if (__copy_from_user((void*)arg, &delay, _IOC_SIZE(cmd)) == 0) {
			if (delay.ident && (delay.ident < DELAY_NB_LINE_EM)
				&& (delay.delay <= 1000)) {
				switch (cmd) {
				case SAF3000_PCM_DELAY_EM:
					ix = 16 + delay.ident - 1;
					break;
				case SAF3000_PCM_DELAY_REC:
					ix = delay.ident - 1;
					break;
				default: ret = -ENOTTY; goto erreur; break;
				}
				data->delay[ix].ix_wr = 0;
				data->delay[ix].ix_rd = 0;
				if (delay.delay > 5) {
					data->delay[ix].ix_rd = -((delay.delay * NB_BYTE_BY_MS) - 1);
				}
			}
			else ret = -EINVAL;
		}
		else ret = -EACCES;
	}

erreur:
*/	return ret;
}

static int pcm_open(struct inode *inode, struct file *file)
{
	struct tdm_data *data = NULL;
	int minor = MINOR(inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;
	
	if (test_and_set_bit(0, &data->open))
		return -EBUSY;
	return 0;
}

static int pcm_release(struct inode *inode, struct file *file)
{
	struct tdm_data *data = NULL;
	int minor = MINOR(inode->i_rdev);
	
	if (minor == PCM_CODEC_MINOR)
		data = data_codec;
	else if (minor == PCM_E1_MINOR)
		data = data_e1;
	if (data == NULL)
		return -1;

	wake_up(&data->read_wait);
	clear_bit(0, &data->open);
	return 0;
}

static const struct file_operations pcm_fops = {
	.owner		= THIS_MODULE,
	.poll		= pcm_poll,
	.write		= pcm_write,
	.aio_write	= pcm_aio_write,
	.read		= pcm_read,
	.aio_read	= pcm_aio_read,
	.unlocked_ioctl		= pcm_ioctl,
	.open		= pcm_open,
	.release	= pcm_release,
};

static struct miscdevice pcm_codec_miscdev = {
	.minor	= PCM_CODEC_MINOR,
	.name	= NAME_CODEC_DEVICE,
	.fops	= &pcm_fops,
};

static struct miscdevice pcm_e1_miscdev = {
	.minor	= PCM_E1_MINOR,
	.name	= NAME_E1_DEVICE,
	.fops	= &pcm_fops,
};

static const struct of_device_id tdm_match[];
static int __devinit tdm_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct device *dev = &ofdev->dev;
	struct device_node *np = dev->of_node;
	struct class *class;
	struct device *infos;
	struct tdm_data *data = NULL;
	scc_t *cp_scc;
	smc_t *cp_smc;
	int ret, i;
	int irq;
	unsigned short bds_ofs;
	int len;
	const u32 *cmd;
	dma_addr_t dma_addr = 0;
	struct cpm_buf_desc __iomem *ad_bd;
	const char *scc = NULL;
	const __be32 *info_ts = NULL;
	const void *prop = 0;
	struct miscdevice *device = NULL;

	match = of_match_device(tdm_match, &ofdev->dev);
	if (!match)
		return -EINVAL;
	
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}

	cmd = of_get_property(np, "fsl,cpm-command", &len);
	if (!cmd || len != 4) {
		dev_err(dev, "CPM UART %s has no/invalid fsl,cpm-command property.\n", np->name);
		ret = -EINVAL;
		goto err;
	}
	data->command = *cmd;

	scc = of_get_property(np, "tdm_name", &len);
	if (sysfs_streq(scc, "scc1"))
		data->flags = TYPE_SCC1;
	else if (sysfs_streq(scc, "scc2"))
		data->flags = TYPE_SCC2;
	else if (sysfs_streq(scc, "scc3"))
		data->flags = TYPE_SCC3;
	else if (sysfs_streq(scc, "scc4"))
		data->flags = TYPE_SCC4;
	else if (sysfs_streq(scc, "smc1"))
		data->flags = TYPE_SMC1;
	else if (sysfs_streq(scc, "smc2"))
		data->flags = TYPE_SMC2;
	dev_info(dev, "Le flag est %d.\n", data->flags);

	/* recherche type de carte support MIAE ou MCR3K-2G */
	np = of_find_compatible_node(NULL, NULL, "fsl,cmpc885");
	if (np) {
		prop = of_get_property(np, "model", NULL);
		if (prop) {
			if (strcmp(prop, "MCR3000_2G") == 0)
				data->carte = CARTE_MCR3K_2G;
			else if (strcmp(prop, "MIAE") == 0)
				data->carte = CARTE_MIAE;
		}
	}
	if (data->carte)
		dev_info(dev, "La carte support est %s.\n", (data->carte == CARTE_MIAE) ? "MIAe" : "MCR3K-2G");
	else {
		ret = -EINVAL;
		goto err;
	}

	/* releve du nombre et du positionnement des TS */
	data->nb_canal = 0;
	np = of_find_compatible_node(NULL, NULL, "fsl,cpm1-tsa");
	if (np) {
		scc = of_get_property(np, "scc_codec", &len);
		if (scc && (len >= sizeof(*scc))) {
			if (((data->flags == TYPE_SCC4) && sysfs_streq(scc, "SCC4"))
				|| ((data->flags == TYPE_SCC3) && sysfs_streq(scc, "SCC3"))
				|| ((data->flags == TYPE_SMC2) && sysfs_streq(scc, "SMC2"))) {
				data->nb_canal = NB_CANAUX_CODEC;
				data_codec = data;
				device = &pcm_codec_miscdev;
			}
		}
		scc = of_get_property(np, "scc_e1", &len);
		if (scc && (len >= sizeof(*scc))) {
			if (((data->flags == TYPE_SCC4) && sysfs_streq(scc, "SCC4"))
				|| ((data->flags == TYPE_SCC3) && sysfs_streq(scc, "SCC3"))
				|| ((data->flags == TYPE_SMC2) && sysfs_streq(scc, "SMC2"))) {
				data->nb_canal = NB_CANAUX_E1;
				data_e1 = data;
				device = &pcm_e1_miscdev;
			}
			info_ts = of_get_property(np, "ts_info", &len);
		}
	}
	/* sortie si pas de canaux affectes au SCC4 ou au SMC2 */
	if (data->nb_canal == 0) {
		ret = -ENODATA;
		goto err;
	}
	
	/* initialisation */
	data->packet_repos = kzalloc((NB_BYTE_BY_MS * data->nb_canal), GFP_KERNEL);
	for (i = 0; i < NB_BYTE_BY_MS; i++) {
		if (i & 1)
			memset(data->packet_repos + (i * data->nb_canal), 0xD5, data->nb_canal);
		else
			memset(data->packet_repos + (i * data->nb_canal), 0x55, data->nb_canal);
	}
	data->octet_recu = 0;
	init_waitqueue_head(&data->read_wait);
	dev_set_drvdata(dev, data);
	data->dev = dev;

	np = dev->of_node;
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2)) {
		cp_smc = of_iomap(np, 0);
		if (cp_smc == NULL) {
			dev_err(dev,"of_iomap CPM failed\n");
			ret = -ENOMEM;
			goto err;
		}
		data->cp_smc = cp_smc;
	}
	else {
		cp_scc = of_iomap(np, 0);
		if (cp_scc == NULL) {
			dev_err(dev,"of_iomap CPM failed\n");
			ret = -ENOMEM;
			goto err;
		}
		data->cp_scc = cp_scc;
	}
	
	class = saf3000_class_get();
	infos = device_create(class, dev, MKDEV(0, 0), NULL, device->name);
	ret = misc_register(device);
	if (ret) {
		dev_err(dev, "pcm: cannot register miscdev on minor=%d (err=%d)\n", device->minor, ret);
		goto err_unfile;
	}
	dev_set_drvdata(infos, data);
	data->infos = infos;
	
	irq = of_irq_to_resource(np, 0, NULL);
	if (!irq) {
		dev_err(dev,"no irq defined\n");
		ret = -EINVAL;
		goto err_unfile;
	}
	data->irq = irq;
	ret = request_irq(irq, pcm_interrupt, 0, device->name, data);
	
	data->pram = of_iomap(np, 1);

	ret = sizeof(cbd_t) * (PCM_NB_TXBD + PCM_NB_RXBD);
	bds_ofs = cpm_dpalloc(ret, 8);
	data->tx_bd = cpm_dpram_addr(bds_ofs);
	data->rx_bd = cpm_dpram_addr(bds_ofs + (sizeof(*data->tx_bd) * PCM_NB_TXBD));
	
	/* Initialize parameter ram. */
	out_be16(&data->pram->scc_rbase, (void*)data->rx_bd-cpm_dpram_addr(0));
	out_be16(&data->pram->scc_tbase, (void*)data->tx_bd-cpm_dpram_addr(0));
	out_8(&data->pram->scc_tfcr, CPMFCR_EB);
	out_8(&data->pram->scc_rfcr, CPMFCR_EB);
	out_be16(&data->pram->scc_mrblr, (NB_BYTE_BY_5_MS * data->nb_canal));
	
	cpm_command(data->command, CPM_CR_INIT_TRX);
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2))
		out_be16(&data->cp_smc->smc_smcmr, 0);
	else
		out_be32(&data->cp_scc->scc_gsmrl, 0);

	/* initialisation des descripteurs Tx */
	data->tx_buf[0] = dma_alloc_coherent(dev, L1_CACHE_ALIGN(NB_BYTE_BY_MS * data->nb_canal * PCM_NB_TXBD), &dma_addr, GFP_KERNEL);
	for (i = 0; i < PCM_NB_TXBD; i++) {
		ad_bd = data->tx_bd + i;
		if (i)
			data->tx_buf[i] = data->tx_buf[0] + (i * NB_BYTE_BY_MS * data->nb_canal);
		out_be16(&ad_bd->cbd_datlen, (NB_BYTE_BY_MS * data->nb_canal));
		out_be32(&ad_bd->cbd_bufaddr, dma_addr + (i * NB_BYTE_BY_MS * data->nb_canal));
		memcpy(data->tx_buf[i], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
		if (i != (PCM_NB_TXBD - 1))
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT);
		else
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
	}
	data->ix_tx = 2;
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2)) {
		setbits8(&data->cp_smc->smc_smcm, SMCM_TXE | SMCM_TX);
		setbits8(&data->cp_smc->smc_smce, SMCM_TXE | SMCM_TX);
	}
	else {
		setbits16(&data->cp_scc->scc_sccm, UART_SCCM_TX);
		setbits16(&data->cp_scc->scc_scce, UART_SCCM_TX);
	}

	/* initialisation des descripteurs Rx */
	for (i = 0; i < PCM_NB_RXBD; i++) {
		ad_bd = data->rx_bd + i;
		data->rx_buf[i] = dma_alloc_coherent(dev, L1_CACHE_ALIGN(NB_BYTE_BY_5_MS * data->nb_canal), &dma_addr, GFP_KERNEL);
		out_be16(&ad_bd->cbd_datlen, (NB_BYTE_BY_5_MS * data->nb_canal));
		out_be32(&ad_bd->cbd_bufaddr, dma_addr);
		if (i != (PCM_NB_RXBD - 1))
			out_be16(&ad_bd->cbd_sc, BD_SC_EMPTY | BD_SC_CM | BD_SC_INTRPT);
		else
			out_be16(&ad_bd->cbd_sc, BD_SC_EMPTY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
	}
	data->ix_rx = 0;
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2)) {
		setbits8(&data->cp_smc->smc_smce, SMCM_RX | SMCM_BSY);
	}
	else {
		setbits16(&data->cp_scc->scc_scce, UART_SCCM_RX | UART_SCCM_BSY);
	}

	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2)) {
		out_be16(&data->cp_smc->smc_smcmr, smcr_mk_clen(15) | SMCMR_SM_TRANS);
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_REN);
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
	}
	else {
		out_be32(&data->cp_scc->scc_gsmrh,
			SCC_GSMRH_REVD | SCC_GSMRH_TRX | SCC_GSMRH_TTX | SCC_GSMRH_CDS | SCC_GSMRH_CTSS | SCC_GSMRH_CDP | SCC_GSMRH_CTSP);
		out_be32(&data->cp_scc->scc_gsmrl, SCC_GSMRL_ENR | SCC_GSMRL_ENT);
	}

	if (device_create_file(infos, &dev_attr_stat))
		goto err_unfile;
	if (device_create_file(infos, &dev_attr_debug))
		goto err_unfile;

	dev_info(dev, "driver TDM %s added.\n", device->name);
	
	return 0;

err_unfile:
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2))
		iounmap(data->cp_smc), data->cp_smc = NULL;
	else
		iounmap(data->cp_scc), data->cp_scc = NULL;
	dev_set_drvdata(dev, NULL);
err:
	if (data) {
		if (data->packet_repos) kfree(data->packet_repos);
		kfree(data);
	}
	return ret;
}

static int __devexit tdm_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct tdm_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	if ((data->flags == TYPE_SMC1) || (data->flags == TYPE_SMC2))
		iounmap(data->cp_smc), data->cp_smc = NULL;
	else
		iounmap(data->cp_scc), data->cp_scc = NULL;
	if (data->nb_canal == NB_CANAUX_CODEC)
		misc_deregister(&pcm_codec_miscdev);
	else
		misc_deregister(&pcm_e1_miscdev);
	dev_set_drvdata(dev, NULL);
	kfree(data->packet_repos);
	kfree(data);
	
	dev_info(dev,"driver TDM removed.\n");
	return 0;
}

static const struct of_device_id tdm_match[] = {
	{
		.compatible = "fsl,cpm1-scc-tdm",
	},
	{
		.compatible = "fsl,cpm1-smc-tdm",
	},
	{},
};
MODULE_DEVICE_TABLE(of, tdm_match);

static struct platform_driver tdm_driver = {
	.probe		= tdm_probe,
	.remove		= __devexit_p(tdm_remove),
	.driver		= {
			  	.name		= "tdm",
			  	.owner		= THIS_MODULE,
			  	.of_match_table	= tdm_match,
			  },
};

static int __init tdm_init(void)
{
	return platform_driver_register(&tdm_driver);
}
module_init(tdm_init);

MODULE_AUTHOR("C.LEROY - P.VASSEUR");
MODULE_DESCRIPTION("Driver for TDM on MPC8xx ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mpc8xx-tdm");
