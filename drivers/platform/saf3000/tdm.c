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

#define PCM_MINOR		240
#define PCM_SMC_MINOR		241
#define PCM_NB_TXBD		3
#define PCM_NB_RXBD		2
#define PCM_NB_BUF_PACKET	4

#define TYPE_SCC		0
#define TYPE_SMC		1

struct em_buf{
	int	num_packet_wr;
	int	octet_packet[PCM_NB_BUF_PACKET];
	char	*packet[PCM_NB_BUF_PACKET];
};

struct rec_buf{
	int	octet_packet;
	char	*packet;
};

struct tdm_data {
	struct device		*dev;
	struct device		*infos;
	scc_t			*cp_scc;
	smc_t			*cp_smc;
	int			irq;
	sccp_t			*pram;
	struct cpm_buf_desc __iomem	*tx_bd;
	struct cpm_buf_desc __iomem	*rx_bd;
	char			*rx_buf[PCM_NB_RXBD];
	char			*tx_buf[PCM_NB_TXBD];
	unsigned char		flags;
	unsigned char		ix_tx;		/* descripteur emission à écrire */
	unsigned char		ix_tx_user;	/* index ecriture user */
	unsigned char		ix_tx_trft;	/* index ecriture transfert */
	unsigned char		ix_rx;
	unsigned char		nb_canal;
	unsigned char		ts_inter;
	unsigned char		canal_write;
	unsigned char		canal_read;
	unsigned char		nb_write;
	unsigned char		em_write;
	char			*packet_repos;
	struct em_buf		em;
	struct rec_buf		rec;
	u32			command;
	wait_queue_head_t	read_wait;
	unsigned long		packet_lost;
	unsigned long		packet_silence;
	unsigned long		open;
	unsigned long		time;
};

static struct tdm_data *data_scc;
static struct tdm_data *data_smc;

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
	
	if (data->flags == TYPE_SCC) {
		lct = in_be16(&data->cp_scc->scc_scce);
		out_be16(&data->cp_scc->scc_scce, lct);
		mask_tx = UART_SCCM_TX;
		mask_rx= UART_SCCM_RX;
		mask_bsy = UART_SCCM_BSY;
		mask_txe = 0;
	}
	else {
		lct = in_8(&data->cp_smc->smc_smce);
		out_8(&data->cp_smc->smc_smce, lct);
		mask_tx = SMCM_TX;
		mask_rx= SMCM_RX;
		mask_bsy = SMCM_BSY;
		mask_txe = SMCM_TXE;
	}

	if (lct & mask_txe) {
		struct cpm_buf_desc __iomem *ad_bd;
		clrbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
		cpm_command(data->command, CPM_CR_INIT_TX);
		/* initialisation des descripteurs Tx */
		for (i = 0; i < PCM_NB_TXBD; i++) {
			ad_bd = data->tx_bd + i;
			if (i != (PCM_NB_TXBD - 1)) {
				memcpy(data->tx_buf[i], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
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

	if (lct & mask_tx) {
		i = 0;
		data->nb_write++;
		if (data->ix_tx_trft != data->ix_tx_user) {
			if (data->em_write) {
				i = data->em.octet_packet[data->ix_tx_trft];
			}
			else if (data->nb_write == 6)
				data->em_write = 1;
		}
		else {
			data->em_write = 0;
			data->nb_write = 0;
		}
		if (i >= (NB_BYTE_BY_MS * data->nb_canal)) {
			memcpy(data->tx_buf[data->ix_tx],
				&data->em.packet[data->ix_tx_trft][(NB_BYTE_BY_5_MS * data->nb_canal) - i],
				(NB_BYTE_BY_MS * data->nb_canal));
			i -= (NB_BYTE_BY_MS * data->nb_canal);
			data->em.octet_packet[data->ix_tx_trft] = i;
			if (i < (NB_BYTE_BY_MS * data->nb_canal)) {
				data->em.octet_packet[data->ix_tx_trft] = 0;
				data->ix_tx_trft++;
				if (data->ix_tx_trft == PCM_NB_BUF_PACKET)
					data->ix_tx_trft = 0;
			}
		}
		else {
			memcpy(data->tx_buf[data->ix_tx], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
			data->packet_silence++;
		}
		setbits16(&(data->tx_bd + data->ix_tx)->cbd_sc, BD_SC_READY);
		data->ix_tx++;
		if (data->ix_tx == PCM_NB_TXBD) data->ix_tx = 0;
		data->time++;
	}

	if (lct & mask_rx) {
		gest_led_debug(1, 1);
		if (data->rec.octet_packet == (NB_BYTE_BY_5_MS * data->nb_canal))
			data->packet_lost++;
		memcpy(data->rec.packet, data->rx_buf[data->ix_rx], (NB_BYTE_BY_5_MS * data->nb_canal));
		data->rec.octet_packet = (NB_BYTE_BY_5_MS * data->nb_canal);
		data->canal_read = 0;
		setbits16(&(data->rx_bd + data->ix_rx)->cbd_sc, BD_SC_EMPTY);
		data->ix_rx++;
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
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	poll_wait(file, &data->read_wait, wait);
	if (data->rec.octet_packet == (NB_BYTE_BY_5_MS * data->nb_canal))
		mask = POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t pcm_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret, ix_wr, i, canal;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	/* si changement buffer demande ou buffer utilise */
	if (((count == (NB_BYTE_BY_5_MS * NB_CANAUX_CODEC)) && (data->canal_write & IDENT_CODEC))
		|| ((count == (NB_BYTE_BY_5_MS * NB_CANAUX_E1)) && (data->canal_write & IDENT_E1))) {
		ix_wr = data->ix_tx_user + 1;
		if (ix_wr == PCM_NB_BUF_PACKET) ix_wr = 0;
		if (data->em.octet_packet[ix_wr] == 0) {
			data->em.octet_packet[data->ix_tx_user] = NB_BYTE_BY_5_MS * data->nb_canal;
			data->canal_write = 0;
			data->ix_tx_user = ix_wr;
		}
	}

	ret = access_ok(VERFIFY_READ, buf, count);
	if (ret == 0) {
		return -EFAULT;
	}

	if (data->em.octet_packet[data->ix_tx_user] == 0) {
		for (i = 0; i < (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS); i++) {
			ix_wr = NB_BYTE_BY_MS * data->nb_canal * i;
			memcpy(&data->em.packet[data->ix_tx_user][ix_wr], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
		}
	}
	
	/* ecriture de tous les canaux */
	if (count == (NB_BYTE_BY_5_MS * data->nb_canal)) {
		ret = __copy_from_user(data->em.packet[data->ix_tx_user], buf, count);
		data->em.octet_packet[data->ix_tx_user] = count;
	}
	/* ecriture canaux codec */
	else if ((count == (NB_BYTE_BY_5_MS * NB_CANAUX_CODEC)) && (data->nb_canal == NB_CANAUX_MAX)) {
		pread = (u8 *)buf;
		ix_wr = 1;	/* on saute le premier TS E1 */
		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
	      		pwrite = &data->em.packet[data->ix_tx_user][ix_wr];
			for (canal = 0; canal < NB_CANAUX_CODEC; ) {
				*pwrite++ = *pread++;
				canal++;
				if ((canal % data->ts_inter) == 0)
					pwrite++;	/* on saute le TS E1 */
			}
			ix_wr += data->nb_canal;
		}
		if ((data->canal_write & IDENT_CODEC) == 0) {
			data->em.octet_packet[data->ix_tx_user] += count;
			data->canal_write |= IDENT_CODEC;
		}
	}
	/* ecriture canaux e1 */
	else if ((count == (NB_BYTE_BY_5_MS * NB_CANAUX_E1)) && (data->nb_canal == NB_CANAUX_MAX)) {
		pread = (u8 *)buf;
		ix_wr = 0;
		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
	      		pwrite = &data->em.packet[data->ix_tx_user][ix_wr];
			for (canal = 0; canal < NB_CANAUX_E1; canal++) {
				*pwrite++ = *pread++;
				if ((canal * data->ts_inter) < NB_CANAUX_CODEC) {
					/* on saute les TS phonie Codec */
					ret = NB_CANAUX_CODEC - (canal * data->ts_inter);
					ret = (ret > data->ts_inter) ? data->ts_inter : ret;
					pwrite += ret;
				}
			}
			ix_wr += data->nb_canal;
		}
		if ((data->canal_write & IDENT_E1) == 0) {
			data->em.octet_packet[data->ix_tx_user] += count;
			data->canal_write |= IDENT_E1;
		}
	}
	else
		count = 0;

	/* si buffer plein */
	if (data->em.octet_packet[data->ix_tx_user] == (NB_BYTE_BY_5_MS * data->nb_canal)) {
		ix_wr = data->ix_tx_user + 1;
		if (ix_wr == PCM_NB_BUF_PACKET) ix_wr = 0;
		if (data->em.octet_packet[ix_wr] == 0) {
			data->em.octet_packet[data->ix_tx_user] = NB_BYTE_BY_5_MS * data->nb_canal;
			data->canal_write = 0;
			data->ix_tx_user = ix_wr;
		}
	}

	return count;
}

static ssize_t pcm_aio_write(struct kiocb *iocb, const struct iovec *iov,
			  unsigned long nr_segs, loff_t pos)
{
	int i, ix_wr, canal, nb_byte = 0;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(iocb->ki_filp->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	/* si changement buffer demande ou buffer utilise */
	if (((nr_segs == NB_CANAUX_CODEC) && (data->canal_write & IDENT_CODEC))
		|| ((nr_segs == NB_CANAUX_E1) && (data->canal_write & IDENT_E1))) {
		ix_wr = data->ix_tx_user + 1;
		if (ix_wr == PCM_NB_BUF_PACKET) ix_wr = 0;
		if (data->em.octet_packet[ix_wr] == 0) {
			data->em.octet_packet[data->ix_tx_user] = NB_BYTE_BY_5_MS * data->nb_canal;
			data->canal_write = 0;
			data->ix_tx_user = ix_wr;
		}
	}

	if ((nr_segs != NB_CANAUX_CODEC) && (nr_segs != NB_CANAUX_E1))
		return -EINVAL;
		
	if (data->em.octet_packet[data->ix_tx_user] == 0) {
		for (i = 0; i < (NB_BYTE_BY_5_MS / NB_BYTE_BY_MS); i++) {
			ix_wr = NB_BYTE_BY_MS * data->nb_canal * i;
			memcpy(&data->em.packet[data->ix_tx_user][ix_wr], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
		}
	}

	/* ecriture canaux codec */
	if (nr_segs == NB_CANAUX_CODEC) {
		ix_wr = 1;	/* on saute le premier TS E1 */
		for (canal = 0; canal < NB_CANAUX_CODEC; ) {
			/* test taille buffer >= taille nécessaire pour transfert */
			if (iov->iov_len < NB_BYTE_BY_5_MS)
				return -EINVAL;
			if (!access_ok(VERIFY_READ, iov->iov_base, NB_BYTE_BY_5_MS))
				return -EFAULT;
      			pread = (u8 *)iov->iov_base;
	      		pwrite = &data->em.packet[data->ix_tx_user][ix_wr];
	      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
				*pwrite = *pread;
				pread++;
				pwrite += data->nb_canal;
			}
			nb_byte += NB_BYTE_BY_5_MS;
			canal++;
			iov++;
			ix_wr++;
			if ((canal % data->ts_inter) == 0)
				ix_wr++;	/* on saute le TS E1 */
		}
		if ((data->canal_write & IDENT_CODEC) == 0) {
			data->em.octet_packet[data->ix_tx_user] += nb_byte;
			data->canal_write |= IDENT_CODEC;
		}
	}
	/* ecriture canaux e1 */
	if (nr_segs == NB_CANAUX_E1) {
		ix_wr = 0;	/* index du premier TS E1 */
		for (canal = 0; canal < NB_CANAUX_E1; canal++) {
			/* test taille buffer >= taille nécessaire pour transfert */
			if (iov->iov_len < NB_BYTE_BY_5_MS)
				return -EINVAL;
			if (!access_ok(VERIFY_READ, iov->iov_base, NB_BYTE_BY_5_MS))
				return -EFAULT;
      			pread = (u8 *)iov->iov_base;
	      		pwrite = &data->em.packet[data->ix_tx_user][ix_wr];
	      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
				*pwrite = *pread;
				pread++;
				pwrite += data->nb_canal;
			}
			nb_byte += NB_BYTE_BY_5_MS;
			iov++;
			ix_wr++;
			if ((canal * data->ts_inter) < NB_CANAUX_CODEC) {
				/* on saute les TS phonie Codec */
				i = NB_CANAUX_CODEC - (canal * data->ts_inter);
				i = (i > data->ts_inter) ? data->ts_inter : i;
				ix_wr += i;
			}
		}
		if ((data->canal_write & IDENT_E1) == 0) {
			data->em.octet_packet[data->ix_tx_user] += nb_byte;
			data->canal_write |= IDENT_E1;
		}
	}

	/* si buffer plein */
	if (data->em.octet_packet[data->ix_tx_user] == (NB_BYTE_BY_5_MS * data->nb_canal)) {
		ix_wr = data->ix_tx_user + 1;
		if (ix_wr == PCM_NB_BUF_PACKET) ix_wr = 0;
		if (data->em.octet_packet[ix_wr] == 0) {
			data->em.octet_packet[data->ix_tx_user] = NB_BYTE_BY_5_MS * data->nb_canal;
			data->canal_write = 0;
			data->ix_tx_user = ix_wr;
		}
	}

	return(nb_byte);
}

static ssize_t pcm_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret, ix_lct = 0, i, canal;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(file->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	ret = access_ok(VERFIFY_WRITE, buf, count);
	if (ret == 0) {
		return -EFAULT;
	}

	if ((file->f_flags & O_NONBLOCK) == 0)
		wait_event(data->read_wait, data->rec.octet_packet);
		
	/* lecture de tous les canaux */
	if (count == (NB_BYTE_BY_5_MS * data->nb_canal)) {
		ret = __copy_to_user((void*)buf, data->rec.packet, count);
		data->rec.octet_packet = 0;
	}
	/* lecture canaux codec */
	else if ((count == (NB_BYTE_BY_5_MS * NB_CANAUX_CODEC)) && (data->nb_canal == NB_CANAUX_MAX)) {
      		pwrite = (u8 *)buf;
		ix_lct = 1;
      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
	      		pread = &data->rec.packet[ix_lct];
			for (canal = 0; canal < NB_CANAUX_CODEC; ) {
				*pwrite++ = *pread++;
				canal++;
				if ((canal % data->ts_inter) == 0)
					pread++;	/* on saute le TS E1 */
			}
			ix_lct += data->nb_canal;
		}
		if ((data->canal_read & IDENT_CODEC) == 0) {
			data->rec.octet_packet -= count;
			data->canal_read |= IDENT_CODEC;
		}
	}
	/* lecture canaux e1 */
	else if ((count == (NB_BYTE_BY_5_MS * NB_CANAUX_E1)) && (data->nb_canal == NB_CANAUX_MAX)) {
		ix_lct = 0;
      		pwrite = (u8 *)buf;
      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
	      		pread = &data->rec.packet[ix_lct];
			for (canal = 0; canal < NB_CANAUX_E1; canal++) {
				*pwrite++ = *pread++;
				if ((canal * data->ts_inter) < NB_CANAUX_CODEC) {
					/* on saute les TS phonie Codec */
					ret = NB_CANAUX_CODEC - (canal * data->ts_inter);
					ret = (ret > data->ts_inter) ? data->ts_inter : ret;
					pread += ret;
				}
			}
			ix_lct += data->nb_canal;
		}
		if ((data->canal_read & IDENT_E1) == 0) {
			data->rec.octet_packet -= count;
			data->canal_read |= IDENT_E1;
		}
	}
	else
		count = 0;

	return count;
}

static ssize_t pcm_aio_read(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	int i, canal, nb_byte = 0, ix_lct = 0;
	u8 *pread, *pwrite;
	struct tdm_data *data = NULL;
	int minor = MINOR(iocb->ki_filp->f_dentry->d_inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	if ((nr_segs != NB_CANAUX_CODEC) && (nr_segs != NB_CANAUX_E1) && (nr_segs != data->nb_canal))
		return -EINVAL;
		
	/* lecture canaux codec */
	if ((nr_segs == NB_CANAUX_CODEC) || (nr_segs == NB_CANAUX_MAX)) {
		ix_lct = 1;	/* on saute le premier TS E1 */
		for (canal = 0; canal < NB_CANAUX_CODEC; ) {
			/* test taille buffer >= taille nécessaire pour transfert */
			if (iov->iov_len < NB_BYTE_BY_5_MS)
				return -EINVAL;
			if (!access_ok(VERFIFY_WRITE, iov->iov_base, NB_BYTE_BY_5_MS))
				return -EFAULT;
      			pwrite = (u8 *)iov->iov_base;
	      		pread = &data->rec.packet[ix_lct];
	      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
				*pwrite = *pread;
				pwrite++;
				pread += data->nb_canal;
			}
			nb_byte += NB_BYTE_BY_5_MS;
			canal++;
			iov++;
			ix_lct++;
			if ((canal % data->ts_inter) == 0)
				ix_lct++;	/* on saute le TS E1 */
		}
		if ((data->canal_read & IDENT_CODEC) == 0) {
			data->rec.octet_packet -= nb_byte;
			data->canal_read |= IDENT_CODEC;
		}
	}
	/* lecture canaux e1 */
	if ((nr_segs == NB_CANAUX_E1) || (nr_segs == NB_CANAUX_MAX)) {
		ix_lct = 0;	/* index du premier TS E1 */
		for (canal = 0; canal < NB_CANAUX_E1; canal++) {
			/* test taille buffer >= taille nécessaire pour transfert */
			if (iov->iov_len < NB_BYTE_BY_5_MS)
				return -EINVAL;
			if (!access_ok(VERFIFY_WRITE, iov->iov_base, NB_BYTE_BY_5_MS))
				return -EFAULT;
      			pwrite = (u8 *)iov->iov_base;
	      		pread = &data->rec.packet[ix_lct];
	      		for (i = 0; i < NB_BYTE_BY_5_MS; i++) {
				*pwrite = *pread;
				pwrite++;
				pread += data->nb_canal;
			}
			nb_byte += NB_BYTE_BY_5_MS;
			iov++;
			ix_lct++;
			if ((canal * data->ts_inter) < NB_CANAUX_CODEC) {
				/* on saute les TS phonie Codec */
				i = NB_CANAUX_CODEC - (canal * data->ts_inter);
				i = (i > data->ts_inter) ? data->ts_inter : i;
				ix_lct += i;
			}
		}
		if ((data->canal_read & IDENT_E1) == 0) {
			data->rec.octet_packet -= nb_byte;
			data->canal_read |= IDENT_E1;
		}
	}

	return(nb_byte);
}

static int pcm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long info;
	struct tdm_data *data = NULL;
	int minor = MINOR(inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
	if (data == NULL)
		return -1;
	
	switch (cmd) {
		case SAF3000_PCM_TIME: info = data->time; break;
		case SAF3000_PCM_LOST: info = data->packet_lost; break;
		case SAF3000_PCM_SILENT: info = data->packet_silence; break;
		default: ret = -ENOTTY; break;
	}
	if (ret == 0)
		ret = __copy_to_user((void*)arg, &info, _IOC_SIZE(cmd));
	return ret;
}

static int pcm_open(struct inode *inode, struct file *file)
{
	struct tdm_data *data = NULL;
	int minor = MINOR(inode->i_rdev);
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
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
	
	if (minor == PCM_MINOR)
		data = data_scc;
	else if (minor == PCM_SMC_MINOR)
		data = data_smc;
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
	.ioctl		= pcm_ioctl,
	.open		= pcm_open,
	.release	= pcm_release,
};

static struct miscdevice pcm_scc_miscdev = {
	.minor	= PCM_MINOR,
	.name	= "pcm",
	.fops	= &pcm_fops,
};

static struct miscdevice pcm_smc_miscdev = {
	.minor	= PCM_SMC_MINOR,
	.name	= "pcm_smc",
	.fops	= &pcm_fops,
};

static int __devinit tdm_probe(struct of_device *ofdev, const struct of_device_id *match)
{
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
	const u32 *prop;
	dma_addr_t dma_addr = 0;
	struct cpm_buf_desc __iomem *ad_bd;
	const char *scc = NULL;
	const __be32 *info_ts = NULL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err;
	}
	if (of_device_is_compatible(np, "fsl,cpm1-scc-tdm")) {
		data_scc = data;
		data->flags = TYPE_SCC;
	}
	else {
		data_smc = data;
		data->flags = TYPE_SMC;
	}

	/* releve du nombre et du positionnement des TS sur SCC4 */
	data->nb_canal = 0;
	np = of_find_compatible_node(NULL, NULL, "fsl,cpm1-tsa");
	if (np) {
		scc = of_get_property(np, "scc_codec", &len);
		if (scc && (len >= sizeof(*scc))) {
			if ((data->flags == TYPE_SCC) && sysfs_streq(scc, "SCC4"))
				data->nb_canal += NB_CANAUX_CODEC;
			if ((data->flags == TYPE_SMC) && sysfs_streq(scc, "SMC2"))
				data->nb_canal += NB_CANAUX_CODEC;
		}
		scc = of_get_property(np, "scc_e1", &len);
		if (scc && (len >= sizeof(*scc))) {
			if ((data->flags == TYPE_SCC) && sysfs_streq(scc, "SCC4"))
				data->nb_canal += NB_CANAUX_E1;
			if ((data->flags == TYPE_SMC) && sysfs_streq(scc, "SMC2"))
				data->nb_canal += NB_CANAUX_E1;
			info_ts = of_get_property(np, "ts_info", &len);
			if (info_ts && (len >= sizeof(*info_ts)))
				data->ts_inter = (info_ts[1] / 2) - 1;
		}
	}
	/* sortie si pas de canaux affectes au SCC4 ou au SMC2 */
	if ((data->nb_canal == 0) || (data->ts_inter == 0)) {
		ret = -ENODATA;
		goto err;
	}
	
	/* initialisation structure emission */
	for (i = 0; i < PCM_NB_BUF_PACKET; i++) {
		data->em.packet[i] = kzalloc((NB_BYTE_BY_5_MS * data->nb_canal), GFP_KERNEL);
		if (!data->em.packet[i]) {
			ret = -ENOMEM;
			goto err;
		}
		data->em.octet_packet[i] = 0;
	}
	data->packet_repos = kzalloc((NB_BYTE_BY_MS * data->nb_canal), GFP_KERNEL);
	for (i = 0; i < NB_BYTE_BY_MS; i++) {
		if (i & 1)
			memset(data->packet_repos + (i * data->nb_canal), 0xD5, data->nb_canal);
		else
			memset(data->packet_repos + (i * data->nb_canal), 0x55, data->nb_canal);
	}
	/* initialisation structure reception */
	data->rec.packet = kzalloc((NB_BYTE_BY_5_MS * data->nb_canal), GFP_KERNEL);
	if (!data->rec.packet) {
		ret = -ENOMEM;
		goto err;
	}
	data->rec.octet_packet = 0;
	init_waitqueue_head(&data->read_wait);
	dev_set_drvdata(dev, data);
	data->dev = dev;

	np = dev->of_node;
	prop = of_get_property(np, "fsl,cpm-command", &len);
	if (!prop || len != 4) {
		dev_err(dev, "CPM UART %s has no/invalid fsl,cpm-command property.\n", np->name);
		ret = -EINVAL;
		goto err;
	}
	data->command = *prop;

	if (data->flags == TYPE_SCC) {
		cp_scc = of_iomap(np, 0);
		if (cp_scc == NULL) {
			dev_err(dev,"of_iomap CPM failed\n");
			ret = -ENOMEM;
			goto err;
		}
		data->cp_scc = cp_scc;
	}
	else {
		cp_smc = of_iomap(np, 0);
		if (cp_smc == NULL) {
			dev_err(dev,"of_iomap CPM failed\n");
			ret = -ENOMEM;
			goto err;
		}
		data->cp_smc = cp_smc;
	}
	
	class = saf3000_class_get();
	if (data->flags == TYPE_SCC) {
		infos = device_create(class, dev, MKDEV(0, 0), NULL, "tdm");
		ret = misc_register(&pcm_scc_miscdev);
		if (ret) {
			dev_err(dev, "pcm: cannot register miscdev on minor=%d (err=%d)\n", PCM_MINOR, ret);
			goto err_unfile;
		}
	}
	else {
		infos = device_create(class, dev, MKDEV(0, 0), NULL, "tdm_smc");
		ret = misc_register(&pcm_smc_miscdev);
		if (ret) {
			dev_err(dev, "pcm: cannot register miscdev on minor=%d (err=%d)\n", PCM_SMC_MINOR, ret);
			goto err_unfile;
		}
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
	ret = request_irq(irq, pcm_interrupt, 0, "tdm", data);
	
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
	if (data->flags == TYPE_SCC)
		out_be32(&data->cp_scc->scc_gsmrl, 0);
	else
		out_be16(&data->cp_smc->smc_smcmr, 0);

	/* initialisation des descripteurs Tx */
	for (i = 0; i < PCM_NB_TXBD; i++) {
		ad_bd = data->tx_bd + i;
		data->tx_buf[i] = dma_alloc_coherent(dev, L1_CACHE_ALIGN(NB_BYTE_BY_MS * data->nb_canal), &dma_addr, GFP_KERNEL);
		out_be16(&ad_bd->cbd_datlen, (NB_BYTE_BY_MS * data->nb_canal));
		out_be32(&ad_bd->cbd_bufaddr, dma_addr);
		memcpy(data->tx_buf[i], data->packet_repos, (NB_BYTE_BY_MS * data->nb_canal));
		if (i != (PCM_NB_TXBD - 1))
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT);
		else
			out_be16(&ad_bd->cbd_sc, BD_SC_READY | BD_SC_CM | BD_SC_INTRPT | BD_SC_WRAP);
	}
	data->ix_tx = PCM_NB_TXBD - 1;
	if (data->flags == TYPE_SCC) {
		setbits16(&data->cp_scc->scc_sccm, UART_SCCM_TX);
		setbits16(&data->cp_scc->scc_scce, UART_SCCM_TX);
	}
	else {
		setbits8(&data->cp_smc->smc_smcm, SMCM_TXE | SMCM_TX);
		setbits8(&data->cp_smc->smc_smce, SMCM_TXE | SMCM_TX);
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
	if (data->flags == TYPE_SCC) {
		setbits16(&data->cp_scc->scc_sccm, UART_SCCM_RX | UART_SCCM_BSY);
		setbits16(&data->cp_scc->scc_scce, UART_SCCM_RX | UART_SCCM_BSY);
	}
	else {
		setbits8(&data->cp_smc->smc_smcm, SMCM_RX | SMCM_BSY);
		setbits8(&data->cp_smc->smc_smce, SMCM_RX | SMCM_BSY);
	}

	if (data->flags == TYPE_SCC) {
		out_be32(&data->cp_scc->scc_gsmrh,
			SCC_GSMRH_REVD | SCC_GSMRH_TRX | SCC_GSMRH_TTX | SCC_GSMRH_CDS | SCC_GSMRH_CTSS | SCC_GSMRH_CDP | SCC_GSMRH_CTSP);
		out_be32(&data->cp_scc->scc_gsmrl, SCC_GSMRL_ENR | SCC_GSMRL_ENT);
	}
	else {
		out_be16(&data->cp_smc->smc_smcmr, smcr_mk_clen(15) | SMCMR_SM_TRANS);
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_REN);
		setbits16(&data->cp_smc->smc_smcmr, SMCMR_TEN);
	}

	if (device_create_file(infos, &dev_attr_stat))
		goto err_unfile;
	if (device_create_file(infos, &dev_attr_debug))
		goto err_unfile;

	if (data->flags == TYPE_SCC)
		dev_info(dev, "driver TDM added.\n");
	else
		dev_info(dev, "driver TDM_SCM added.\n");
	
	return 0;

err_unfile:
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	if (data->flags == TYPE_SCC)
		iounmap(data->cp_scc), data->cp_scc = NULL;
	else
		iounmap(data->cp_smc), data->cp_smc = NULL;
	dev_set_drvdata(dev, NULL);
err:
	if (data) {
		for (i = 0; i < PCM_NB_BUF_PACKET; i++) {
			if (data->em.packet[i]) kfree(data->em.packet[i]);
		}
		if (data->packet_repos) kfree(data->packet_repos);
		if (data->rec.packet) kfree(data->rec.packet);
		kfree(data);
	}
	return ret;
}

static int __devexit tdm_remove(struct of_device *ofdev)
{
	int i;
	struct device *dev = &ofdev->dev;
	struct tdm_data *data = dev_get_drvdata(dev);
	struct device *infos = data->infos;
	
	device_remove_file(infos, &dev_attr_stat);
	device_remove_file(infos, &dev_attr_debug);
	free_irq(data->irq, pcm_interrupt);
	dev_set_drvdata(infos, NULL);
	device_unregister(infos), data->infos = NULL;
	if (data->flags == TYPE_SCC)
		iounmap(data->cp_scc), data->cp_scc = NULL;
	else
		iounmap(data->cp_smc), data->cp_smc = NULL;
	dev_set_drvdata(dev, NULL);
	misc_deregister(&pcm_scc_miscdev);
	misc_deregister(&pcm_smc_miscdev);
	for (i = 0; i < PCM_NB_BUF_PACKET; i++)
		kfree(data->em.packet[i]);
	kfree(data->packet_repos);
	kfree(data->rec.packet);
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

static struct of_platform_driver tdm_driver = {
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
	return of_register_platform_driver(&tdm_driver);
}
module_init(tdm_init);

MODULE_AUTHOR("C.LEROY - P.VASSEUR");
MODULE_DESCRIPTION("Driver for TDM on MPC8xx ");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mpc8xx-tdm");
