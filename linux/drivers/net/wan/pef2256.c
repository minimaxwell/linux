/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */

#include "pef2256.h"

irqreturn_t pef2256_irq(int irq, void *dev_priv);
static int Config_HDLC(struct pef2256_dev_priv *priv);
static int init_FALC(struct pef2256_dev_priv *priv);

void print_regs(pef2256_regs *base_addr)
{
	printk("	mMODE = 0x%02x\n", base_addr->mMODE);
	printk("	mRAH1 = 0x%02x\n", base_addr->mRAH1);
	printk("	mRAH2 = 0x%02x\n", base_addr->mRAH2);
	printk("	mRAL1 = 0x%02x\n", base_addr->mRAL1);
	printk("	mRAL2 = 0x%02x\n", base_addr->mRAL2);
	printk("	mIPC = 0x%02x\n", base_addr->mIPC);
	printk("	mCCR1 = 0x%02x\n", base_addr->mCCR1);
	printk("	mCCR2 = 0x%02x\n", base_addr->mCCR2);
	printk("	mRTR1 = 0x%02x\n", base_addr->mRTR1);
	printk("	mRTR2 = 0x%02x\n", base_addr->mRTR2);
	printk("	mRTR3 = 0x%02x\n", base_addr->mRTR3);
	printk("	mRTR4 = 0x%02x\n", base_addr->mRTR4);
	printk("	mTTR1 = 0x%02x\n", base_addr->mTTR1);
	printk("	mTTR2 = 0x%02x\n", base_addr->mTTR2);
	printk("	mTTR3 = 0x%02x\n", base_addr->mTTR3);
	printk("	mTTR4 = 0x%02x\n", base_addr->mTTR4);
	printk("	mIMR0 = 0x%02x\n", base_addr->mIMR0);
	printk("	mIMR1 = 0x%02x\n", base_addr->mIMR1);
	printk("	mIMR2 = 0x%02x\n", base_addr->mIMR2);
	printk("	mIMR3 = 0x%02x\n", base_addr->mIMR3);
	printk("	mIMR4 = 0x%02x\n", base_addr->mIMR4);
	printk("	mIMR5 = 0x%02x\n", base_addr->mIMR5);
	printk("	mIERR = 0x%02x\n", base_addr->mIERR);
	printk("	mFMR0 = 0x%02x\n", base_addr->mFMR0);
	printk("	mFMR1 = 0x%02x\n", base_addr->mFMR1);
	printk("	mFMR2 = 0x%02x\n", base_addr->mFMR2);
	printk("	mLOOP = 0x%02x\n", base_addr->mLOOP);
	printk("	mXSW = 0x%02x\n", base_addr->mXSW);
	printk("	mXSP = 0x%02x\n", base_addr->mXSP);
	printk("	mXC0 = 0x%02x\n", base_addr->mXC0);
	printk("	mXC1 = 0x%02x\n", base_addr->mXC1);
	printk("	mRC0 = 0x%02x\n", base_addr->mRC0);
	printk("	mRC1 = 0x%02x\n", base_addr->mRC1);
	printk("	mXPM0 = 0x%02x\n", base_addr->mXPM0);
	printk("	mXPM1 = 0x%02x\n", base_addr->mXPM1);
	printk("	mXPM2 = 0x%02x\n", base_addr->mXPM2);
	printk("	mTSWM = 0x%02x\n", base_addr->mTSWM);
	printk("	mIDLE = 0x%02x\n", base_addr->mIDLE);
	printk("	mXSA4 = 0x%02x\n", base_addr->mXSA4);
	printk("	mXSA5 = 0x%02x\n", base_addr->mXSA5);
	printk("	mXSA6 = 0x%02x\n", base_addr->mXSA6);
	printk("	mXSA7 = 0x%02x\n", base_addr->mXSA7);
	printk("	mXSA8 = 0x%02x\n", base_addr->mXSA8);
	printk("	mFMR3 = 0x%02x\n", base_addr->mFMR3);
	printk("	mICB1 = 0x%02x\n", base_addr->mICB1);
	printk("	mICB2 = 0x%02x\n", base_addr->mICB2);
	printk("	mICB3 = 0x%02x\n", base_addr->mICB3);
	printk("	mICB4 = 0x%02x\n", base_addr->mICB4);
	printk("	mLIM0 = 0x%02x\n", base_addr->mLIM0);
	printk("	mLIM1 = 0x%02x\n", base_addr->mLIM1);
	printk("	mPCD = 0x%02x\n", base_addr->mPCD);
	printk("	mPCR = 0x%02x\n", base_addr->mPCR);
	printk("	mLIM2 = 0x%02x\n", base_addr->mLIM2);
	printk("	mLCR1 = 0x%02x\n", base_addr->mLCR1);
	printk("	mLCR2 = 0x%02x\n", base_addr->mLCR2);
	printk("	mLCR3 = 0x%02x\n", base_addr->mLCR3);
	printk("	mSIC1 = 0x%02x\n", base_addr->mSIC1);
	printk("	mSIC2 = 0x%02x\n", base_addr->mSIC2);
	printk("	mSIC3 = 0x%02x\n", base_addr->mSIC3);
	printk("	mCMR1 = 0x%02x\n", base_addr->mCMR1);
	printk("	mCMR2 = 0x%02x\n", base_addr->mCMR2);
	printk("	mGCR = 0x%02x\n", base_addr->mGCR);
	printk("	mESM = 0x%02x\n", base_addr->mESM);
	printk("	mCMR3 = 0x%02x\n", base_addr->mCMR3);
	printk("	mPC1 = 0x%02x\n", base_addr->mPC1);
	printk("	mPC2 = 0x%02x\n", base_addr->mPC2);
	printk("	mPC3 = 0x%02x\n", base_addr->mPC3);
	printk("	mPC4 = 0x%02x\n", base_addr->mPC4);
	printk("	mPC5 = 0x%02x\n", base_addr->mPC5);
	printk("	mGPC1 = 0x%02x\n", base_addr->mGPC1);
	printk("	mPC6 = 0x%02x\n", base_addr->mPC6);
	printk("	mCCR3 = 0x%02x\n", base_addr->mCCR3);
	printk("	mCCR4 = 0x%02x\n", base_addr->mCCR4);
	printk("	mCCR5 = 0x%02x\n", base_addr->mCCR5);
	printk("	mMODE2 = 0x%02x\n", base_addr->mMODE2);
	printk("	mMODE3 = 0x%02x\n", base_addr->mMODE3);
	printk("	mRBC2 = 0x%02x\n", base_addr->mRBC2);
	printk("	mRBC3 = 0x%02x\n", base_addr->mRBC3);
	printk("	mGCM1 = 0x%02x\n", base_addr->mGCM1);
	printk("	mGCM2 = 0x%02x\n", base_addr->mGCM2);
	printk("	mGCM3 = 0x%02x\n", base_addr->mGCM3);
	printk("	mGCM4 = 0x%02x\n", base_addr->mGCM4);
	printk("	mGCM5 = 0x%02x\n", base_addr->mGCM5);
	printk("	mGCM6 = 0x%02x\n", base_addr->mGCM6);
	printk("	SIS2/GCM7 = 0x%02x\n", base_addr->mDif1.mSIS2);
	printk("	RSIS2/GCM8 = 0x%02x\n", base_addr->mDif2.mRSIS2);
	printk("	mTSEO = 0x%02x\n", base_addr->mTSEO);
	printk("	mTSBS1 = 0x%02x\n", base_addr->mTSBS1);
	printk("	mTSBS2 = 0x%02x\n", base_addr->mTSBS2);
	printk("	mTSBS3 = 0x%02x\n", base_addr->mTSBS3);
	printk("	mTSS2 = 0x%02x\n", base_addr->mTSS2);
	printk("	mTSS3 = 0x%02x\n", base_addr->mTSS3);
	printk("	mRes10 = 0x%02x\n", base_addr->mRes10);
	printk("	mRes11 = 0x%02x\n", base_addr->mRes11);
	printk("	mTPC0 = 0x%02x\n", base_addr->mTPC0);
	printk("	mGLC1 = 0x%02x\n", base_addr->mGLC1);
}

static ssize_t fs_attr_regs_show(struct device *dev, 
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	pef2256_regs *base_addr = (pef2256_regs *)priv->base_addr;

	print_regs(base_addr);
	return sprintf(buf, "*** printk DEBUG ***\n");
}

static DEVICE_ATTR(regs, S_IRUGO, fs_attr_regs_show, NULL);

static ssize_t fs_attr_mode_show(struct device *dev, 
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "%d\n", priv->mode);
}


static ssize_t fs_attr_mode_store(struct device *dev, 
			struct device_attribute *attr,  const char *buf, 
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	u32 value = simple_strtol(buf, NULL, 10);
	int reconfigure = (value != priv->mode);

	if (value != MASTER_MODE && value != SLAVE_MODE)
		return -EINVAL;

	priv->mode = value;
	if (reconfigure && priv->init_done) {
		init_FALC(priv);
		Config_HDLC(priv);
	}

	return count;
}

static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, fs_attr_mode_show, fs_attr_mode_store);



static ssize_t fs_attr_Tx_TS_show(struct device *dev, 
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "0x%08x\n", priv->Tx_TS);
}


static ssize_t fs_attr_Tx_TS_store(struct device *dev, 
			struct device_attribute *attr,  const char *buf, 
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	u32 value = simple_strtol(buf, NULL, 10);
	int reconfigure = (value != priv->Tx_TS);

	/* TS 0 is reserved */
	if (value & 0x80000000)
		return -EINVAL;

	priv->Tx_TS = value;
	if (reconfigure && priv->init_done)
		Config_HDLC(priv);

	return count;
}

static DEVICE_ATTR(Tx_TS, S_IRUGO | S_IWUSR, fs_attr_Tx_TS_show, fs_attr_Tx_TS_store);


static ssize_t fs_attr_Rx_TS_show(struct device *dev, 
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "0x%08x\n", priv->Rx_TS);
}


static ssize_t fs_attr_Rx_TS_store(struct device *dev, 
			struct device_attribute *attr,  const char *buf, 
			size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;
	u32 value = simple_strtol(buf, NULL, 10);
	int reconfigure = (value != priv->Rx_TS);

	/* TS 0 is reserved */
	if (value & 0x80000000) 
		return -EINVAL;

	priv->Rx_TS = value;
	if (reconfigure && priv->init_done)
		Config_HDLC(priv);

	return count;
}

static DEVICE_ATTR(Rx_TS, S_IRUGO | S_IWUSR, fs_attr_Rx_TS_show, fs_attr_Rx_TS_store);



/*
 * Configuration canal HDLC 
 */
int Config_HDLC(struct pef2256_dev_priv *priv)
{
	int i;
	int TS_idx;
	pef2256_regs *base_addr;
	u8 dummy;

	/* positionnement de l'adresse du trameur E1 */
	base_addr = (pef2256_regs *)priv->base_addr;

	/* Lecture pour effacer les IT existantes */
	dummy = base_addr->mISR0;
	dummy = base_addr->mISR1;

	/* Masquage des IT HDLC 1 Emission */
	/* IMR1 - XPR */
	base_addr->mIMR1 |= 1;
	/* IMR1 - XDU */
	base_addr->mIMR1 |= 1 << 4;
	/* IMR1 - ALLS */
	base_addr->mIMR1 |= 1 << 5;

	/* Masquage des IT HDLC 1 Reception */
	/* IMR0 - RPF */
	base_addr->mIMR0 |= 1;
	/* IMR0 - RME */
	base_addr->mIMR0 |= 1 << 7;
	/* IMR1 - RDO */
	base_addr->mIMR1 |= 1 << 6;

	/* attente vidage des fifos */
	udelay((2 * M_DRV_E1_TAILLE_FIFO) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   pour FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->mMODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* positionnement Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->mCCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->mCCR2), 0x00);

	/* attente vidage des fifos */
	udelay((2 * M_DRV_E1_TAILLE_FIFO) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   pour FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->mMODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* positionnement Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->mCCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->mCCR2), 0x00);

	/* attente vidage des fifos */
	udelay((2 * M_DRV_E1_TAILLE_FIFO) * 125);

	/* MODE.HRAC = 0 (Receiver inactive)
	   MODE.DIV = 0 (Data normal operation)
	   pour FALC V2.2 : MODE.HDLCI = 0 (normal operation) */
	/* MODE.MDS2:0 = 100 (No address comparison) */
	/* MODE.HRAC = 1 (Receiver active) */
	out_8(&(base_addr->mMODE), 1 << 3);
	/* CCR1.EITS = 1 (Enable internal Time Slot 31:0 Signaling)
	   CCR1.XMFA = 0 (No transmit multiframe alignment)
	   CCR1.RFT1:0 = 00 (RFIFO sur 32 bytes) */
	/* positionnement Interframe Time Fill */
	/* CCR1.ITF = 1 (Interframe Time Fill Continuous flag) */
	out_8(&(base_addr->mCCR1), 0x10 | (1 << 3));
	/* CCR2.XCRC = 0 (Transmit CRC ON)
	   CCR2.RCRC = 0 (Receive CRC ON, no write in RFIFO)
	   CCR2.RADD = 0 (No write address in RFIFO) */
	out_8(&(base_addr->mCCR2), 0x00);
	/* Initialisation sélection Time Slot */
	out_8(&(base_addr->mTTR1), 0x00);
	out_8(&(base_addr->mTTR2), 0x00);
	out_8(&(base_addr->mTTR3), 0x00);
	out_8(&(base_addr->mTTR4), 0x00);
	out_8(&(base_addr->mRTR1), 0x00);
	out_8(&(base_addr->mRTR2), 0x00);
	out_8(&(base_addr->mRTR3), 0x00);
	out_8(&(base_addr->mRTR4), 0x00);
	/* positionnement bits pour TS sélectionnés */
	/* On commence au TS 1, le TS 0 est réservé */
	for (TS_idx=1; TS_idx<32; TS_idx++) {
		i = 7 - (TS_idx % 8);
		switch (TS_idx / 8) {
		case 0:
			if (priv->Tx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mTTR1), 1 << i);
			if (priv->Rx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mRTR1), 1 << i);
			break;
		case 1:
			if (priv->Tx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mTTR2), 1 << i);
			if (priv->Rx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mRTR2), 1 << i);
			break;
		case 2:
			if (priv->Tx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mTTR3), 1 << i);
			if (priv->Rx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mRTR3), 1 << i);
			break;
		case 3:
			if (priv->Tx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mTTR4), 1 << i);
			if (priv->Rx_TS & (1 << TS_idx))
				setbits8(&(base_addr->mRTR4), 1 << i);
			break;
		}
	}

	/* Demasquage des IT HDLC 1 Emission */
	/* IMR1 - XPR */
	base_addr->mIMR1 &= ~1;
	/* IMR1 - XDU */
	base_addr->mIMR1 &= ~(1 << 4);
	/* IMR1 - ALLS */
	base_addr->mIMR1 &= ~(1 << 5);

	/* Demasquage des IT HDLC 1 Reception */
	/* IMR0 - RPF */
	base_addr->mIMR0 &= ~1;
	/* IMR0 - RME */
	base_addr->mIMR0 &= ~(1 << 7);
	/* IMR1 - RDO */
	base_addr->mIMR1 &= ~(1 << 6);

	return 0;
}


/*
 * Initialisation du FALC56
 */
static int init_FALC(struct pef2256_dev_priv *priv)
{
	pef2256_regs *base_addr;
	int Version;
	
	/* récupération de la version du composant */
	Version = priv->component_id;
	
	/* Initialisation du composant FALC56 */
	base_addr = (pef2256_regs *)priv->base_addr;
	/* RCLK output : DPLL clock, DCO-X enabled, DCO-X internal reference clock */
	out_8(&(base_addr->mCMR1), 0x00);
	/* SCLKR selected, SCLKX selected, receive synchro pulse sourced by SYPR,
	   transmit synchro pulse sourced by SYPX */
	out_8(&(base_addr->mCMR2), 0x00);
	/* NRZ coding, no alarm simulation */
	out_8(&(base_addr->mFMR0), 0x00);
	/* E1 double frame format, 2 Mbit/s system data rate, no AIS transmission
	   to remote end or system interface, payload loop off, transmit remote alarm on */
	out_8(&(base_addr->mFMR1), 0x00);
	out_8(&(base_addr->mFMR2), 0x02);
	/* E1 default for LIM2 */
	out_8(&(base_addr->mLIM2), 0x20);
	if (priv->mode == MASTER_MODE)
		/* SEC input, active high */
		out_8(&(base_addr->mGPC1), 0x00);
	else
		/* FSC output, active high */
		out_8(&(base_addr->mGPC1), 0x40);
	/* internal second timer, power on */
	out_8(&(base_addr->mGCR), 0x00);
	/* slave mode, local loop off, mode short-haul */
	if (Version == E_DRV_E1_VERSION_1_2)
		out_8(&(base_addr->mLIM0), 0x00);
	else
		out_8(&(base_addr->mLIM0), 0x08);
	/* analog interface selected, remote loop off */
	out_8(&(base_addr->mLIM1), 0x00);
	if (Version == E_DRV_E1_VERSION_1_2) {
		/* function of ports RP(A to D) : output receive sync pulse
		   function of ports XP(A to D) : output transmit line clock */
		out_8(&(base_addr->mPC1), 0x77);
		out_8(&(base_addr->mPC2), 0x77);
		out_8(&(base_addr->mPC3), 0x77);
		out_8(&(base_addr->mPC4), 0x77);
	}
	else {
		/* function of ports RP(A to D) : output high
		   function of ports XP(A to D) : output high */
		out_8(&(base_addr->mPC1), 0xAA);
		out_8(&(base_addr->mPC2), 0xAA);
		out_8(&(base_addr->mPC3), 0xAA);
		out_8(&(base_addr->mPC4), 0xAA);
	}
	/* function of port RPA : input SYPR
	   function of port XPA : input SYPX */
	out_8(&(base_addr->mPC1), 0x00);
	/* SCLKR, SCLKX, RCLK configured to inputs,
	   XFMS active low, CLK1 and CLK2 pin configuration */
	out_8(&(base_addr->mPC5), 0x00);
	out_8(&(base_addr->mPC6), 0x00);
	/* the receive clock offset is cleared
	   the receive time slot offset is cleared */
	out_8(&(base_addr->mRC0), 0x00);
	out_8(&(base_addr->mRC1), 0x9C);
	/* 2.048 MHz system clocking rate, receive buffer 2 frames, transmit buffer
	   bypass, data sampled and transmitted on the falling edge of SCLKR/X,
	   automatic freeze signaling, data is active in the first channel phase */
	out_8(&(base_addr->mSIC1), 0x00);
	out_8(&(base_addr->mSIC2), 0x00);
	out_8(&(base_addr->mSIC3), 0x00);
	/* channel loop-back and single frame mode are disabled */
	out_8(&(base_addr->mLOOP), 0x00);
	/* all bits of the transmitted service word are cleared */
	out_8(&(base_addr->mXSW), 0x1F);
	/* spare bit values are cleared */
	out_8(&(base_addr->mXSP), 0x00);
	/* no transparent mode active */
	out_8(&(base_addr->mTSWM), 0x00);
	/* the transmit clock offset is cleared
	   the transmit time slot offset is cleared */
	out_8(&(base_addr->mXC0), 0x00);
	out_8(&(base_addr->mXC1), 0x9C);
	/* transmitter in tristate mode */
	out_8(&(base_addr->mXPM2), 0x40);
	/* transmit pulse mask */
	if (Version != E_DRV_E1_VERSION_1_2)
		out_8(&(base_addr->mXPM0), 0x9C);
	
	if (Version == E_DRV_E1_VERSION_1_2) {
		/* master clock is 16,384 MHz (flexible master clock) */
		out_8(&(base_addr->mGCM2), 0x58);
		out_8(&(base_addr->mGCM3), 0xD2);
		out_8(&(base_addr->mGCM4), 0xC2);
		out_8(&(base_addr->mGCM5), 0x07);
		out_8(&(base_addr->mGCM6), 0x10);
	}
	else {
		/* master clock is 16,384 MHz (flexible master clock) */
		out_8(&(base_addr->mGCM2), 0x18);
		out_8(&(base_addr->mGCM3), 0xFB);
		out_8(&(base_addr->mGCM4), 0x0B);
		out_8(&(base_addr->mGCM5), 0x01);
		out_8(&(base_addr->mGCM6), 0x0B);
		out_8(&(base_addr->mDif1.mGCM7), 0xDB);
		out_8(&(base_addr->mDif2.mGCM8), 0xDF);
	}

	/* master mode => LIM0.MAS = 1 (bit 0) */
	if (priv->mode == MASTER_MODE)
		setbits8(&(base_addr->mLIM0), 1 << 0);

	/* transmit line in normal operation => XPM2.XLT = 0 (bit 6) */
	clrbits8(&(base_addr->mXPM2), 1 << 6);

	if (Version == E_DRV_E1_VERSION_1_2) {
		/* receive input threshold = 0,21V => LIM1.RIL2:0 = 101 (bits 6, 5 et 4) */
		setbits8(&(base_addr->mLIM1), 1 << 4);
		setbits8(&(base_addr->mLIM1), 1 << 6);
	}
	else {
		/* receive input threshold = 0,21V => LIM1.RIL2:0 = 100 (bits 6, 5 et 4) */
		setbits8(&(base_addr->mLIM1), 1 << 6);
	}
	/* transmit line coding = HDB3 => FMR0.XC1:0 = 11 (bits 7 et 6) */
	setbits8(&(base_addr->mFMR0), 1 << 6);
	setbits8(&(base_addr->mFMR0), 1 << 7);
	/* receive line coding = HDB3 => FMR0.RC1:0 = 11 (bits 5 et 4) */
	setbits8(&(base_addr->mFMR0), 1 << 4);
	setbits8(&(base_addr->mFMR0), 1 << 5);
	/* detection of LOS alarm = 176 pulses (soit (10 + 1) * 16) */
	out_8(&(base_addr->mPCD), 10);
	/* recovery of LOS alarm = 22 pulses (soit 21 + 1) */
	out_8(&(base_addr->mPCR), 21);
	/* DCO-X center frequency => CMR2.DCOXC = 1 (bit 5) */
	setbits8(&(base_addr->mCMR2), 1 << 5);
	if (priv->mode == SLAVE_MODE) {
		/* select RCLK source = 2M => CMR1.RS(1:0) = 10 (bits 5 et 4) */
		setbits8(&(base_addr->mCMR1), 1 << 5);
		/* disable switching RCLK -> SYNC => CMR1.DCS = 1 (bit 3) */
		setbits8(&(base_addr->mCMR1), 1 << 3);
	}
	if (Version != E_DRV_E1_VERSION_1_2)
		/* during inactive channel phase RDO into tri-state mode */
		setbits8(&(base_addr->mSIC3), 1 << 5);
	if (! strcmp (priv->rising_edge_sync_pulse, "transmit")) {
		/* rising edge sync pulse transmit => SIC3.RESX = 1 (bit 3) */
		setbits8(&(base_addr->mSIC3), 1 << 3);
	}
	else {
		/* rising edge sync pulse receive => SIC3.RESR = 1 (bit 2) */
		setbits8(&(base_addr->mSIC3), 1 << 2);
	}
	/* transmit offset counter = 4
	   => XC0.XCO10:8 = 000 (bits 2, 1 et 0); XC1.XCO7:0 = 4 (bits 7 ... 0) */
	out_8(&(base_addr->mXC1), 4);
	/* receive offset counter = 4
	   => RC0.RCO10:8 = 000 (bits 2, 1 et 0); RC1.RCO7:0 = 4 (bits 7 ... 0) */
	out_8(&(base_addr->mRC1), 4);

	/* clocking rate 8M and data rate 2M on the system highway */
	setbits8(&(base_addr->mSIC1), 1 << 7);
	/* data rate 4M on the system highway */
	if (priv->data_rate == DATA_RATE_4M)
		setbits8(&(base_addr->mFMR1), 1 << 1);
	/* data rate 8M on the system highway */
	if (priv->data_rate == DATA_RATE_8M)
		setbits8(&(base_addr->mSIC1), 1 << 6);
	/* channel phase for FALC56 */
	if ((priv->channel_phase == CHANNEL_PHASE_1)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		setbits8(&(base_addr->mSIC2), 1 << 1);
	if ((priv->channel_phase == CHANNEL_PHASE_2)
		|| (priv->channel_phase == CHANNEL_PHASE_3))
		setbits8(&(base_addr->mSIC2), 1 << 2);
	
	if (priv->mode == SLAVE_MODE) {
		/* transmit buffer size = 2 frames => SIC1.XBS1:0 = 10 (bits 1 et 0) */
		setbits8(&(base_addr->mSIC1), 1 << 1);
	}

	dev_err(priv->dev, "Before setting multiframe FMR1=%0x FMR2=%0x, XSP=%0x\n", 
		base_addr->mFMR1, base_addr->mFMR2, base_addr->mXSP);
	/* transmit in multiframe => FMR1.XFS = 1 (bit 3) */
	setbits8(&(base_addr->mFMR1), 1 << 3);
	/* receive in multiframe => FMR2.RFS1:0 = 10 (bits 7 et 6) */
	setbits8(&(base_addr->mFMR2), 1 << 7);
	/* Automatic transmission of submultiframe status => XSP.AXS = 1 (bit 3) */
	setbits8(&(base_addr->mXSP), 1 << 3);
	dev_err(priv->dev, "After setting multiframe FMR1=%0x FMR2=%0x, XSP=%0x\n", 
		base_addr->mFMR1, base_addr->mFMR2, base_addr->mXSP);

	/* error counter mode toutes les 1s => FMR1.ECM = 1 (bit 2) */
	setbits8(&(base_addr->mFMR1), 1 << 2);
	/* error counter mode COFA => GCR.ECMC = 1 (bit 4) */
	setbits8(&(base_addr->mGCR), 1 << 4);
	/* errors in service words with no influence => RC0.SWD = 1 (bit 7) */
	setbits8(&(base_addr->mRC0), 1 << 7);
	/* 4 consecutive incorrect FAS = loss of sync => RC0.ASY4 = 1 (bit 6) */
	setbits8(&(base_addr->mRC0), 1 << 6);
	/* Si-Bit in service word from XDI => XSW.XSIS = 1 (bit 7) */
	setbits8(&(base_addr->mXSW), 1 << 7);
	/* Si-Bit in FAS word from XDI => XSP.XSIF = 1 (bit 2) */
	setbits8(&(base_addr->mXSP), 1 << 2);

	/* port RCLK is output => PC5.CRP = 1 (bit 0) */
	setbits8(&(base_addr->mPC5), 1 << 0);
	/* visibility of the masked interrupts => GCR.VIS = 1 (bit 7) */
	setbits8(&(base_addr->mGCR), 1 << 7);
	/* reset des lignes
	   => CMDR.RRES = 1 (bit 6); CMDR.XRES = 1 (bit 4); CMDR.SRES = 1 (bit 0) */
	out_8(&(base_addr->mCMDR), 0x51);

	return 0;
}



static int pef2256_open(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	pef2256_regs *base_addr = (pef2256_regs *)priv->base_addr;
	int ret;

	if (hdlc_open(netdev))
		return -EAGAIN;

	/* Do E1 stuff */
	/* Enregistrement interruption FALC pour MPC */
	ret = request_irq(priv->irq, pef2256_irq, 0, "e1-wan", priv);
	if (ret) {
		dev_err(priv->dev, "Cannot request irq. Device seems busy.\n");
		return -EBUSY;
	}

	/* programmation du composant FALC */
	if (priv->component_id != E_DRV_E1_VERSION_UNDEF) {
		ret = init_FALC(priv);
	}
	else {
		dev_err(priv->dev, "Composant ident (%X/%X) = %d\n", 
			base_addr->mVSTR, base_addr->mWID, priv->component_id);
		ret = -ENODEV;
	}

	if (ret < 0)
		return ret; 

	priv->tx_skb = NULL;
	priv->rx_len = 0;

	Config_HDLC(priv);

	netif_start_queue(netdev);
	netif_carrier_on(netdev);

	priv->init_done = 1;

	return 0;
}


static int pef2256_close(struct net_device *netdev)
{
	netif_stop_queue(netdev);

	/* Do E1 stuff */

	hdlc_close(netdev);
	return 0;
}



static int pef2256_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int ret; 

	ret = hdlc_ioctl(dev, ifr, cmd);
	return ret;
}

/* Handler IT lecture */
static int pef2256_rx(struct pef2256_dev_priv *priv)
{
	struct sk_buff *skb;
	int idx, size;
	pef2256_regs *base_addr;

	base_addr = priv->base_addr;

	/* RDO has been received -> wait for RME */
	if (priv->rx_len == -1) {
		/* Acknowledge the FIFO */
		setbits8(&(base_addr->mCMDR), 1 << 7);

		if (priv->ISR0 & (1 << 7)) 
			priv->rx_len = 0;

		return 0;
	}

	/* Do E1 stuff */
	/* RPF : a block is available in the receive FIFO */
	if (priv->ISR0 & 1) {
		for (idx=0; idx < 32; idx++)
			priv->rx_buff[priv->rx_len + idx] = base_addr->mFIFO.mRFIFO[idx & 1];

		/* Acknowledge the FIFO */
		setbits8(&(base_addr->mCMDR), 1 << 7);

		priv->rx_len += 32;
	}

	/* RME : Message end : Read the receive FIFO */
	if (priv->ISR0 & (1 << 7)) {
		/* Get size of last block */
		size = base_addr->mRBCL & 0x1F;

		/* Read last block */
		for (idx=0; idx < size; idx++)
			priv->rx_buff[priv->rx_len + idx] = base_addr->mFIFO.mRFIFO[idx & 1];

		/* Acknowledge the FIFO */
		setbits8(&(base_addr->mCMDR), 1 << 7);

		priv->rx_len += size;

		/* Packet received */
		if (priv->rx_len > 0) {
			skb = dev_alloc_skb(priv->rx_len);
			if (! skb) {
				priv->rx_len = 0;
				priv->netdev->stats.rx_dropped++;
				return -ENOMEM;
			}
			memcpy(skb->data, priv->rx_buff, priv->rx_len);
			skb_put(skb, priv->rx_len);
			priv->rx_len = 0;
			skb->protocol = hdlc_type_trans(skb, priv->netdev);
			priv->netdev->stats.rx_packets++;
			priv->netdev->stats.rx_bytes += skb->len;
			netif_rx(skb);
		}
	}

	return 0;
}


/* Handler IT ecriture */
static int pef2256_tx(struct pef2256_dev_priv *priv)
{
	int idx;
	static int size;
	pef2256_regs *base_addr;
	u8 *tx_buff = priv->tx_skb->data;

	base_addr = (pef2256_regs *)priv->base_addr;

	/* Do E1 stuff */

	/* ALLS : transmit all done */
	if (priv->ISR1 & (1 << 5)) {
		priv->netdev->stats.tx_packets++;
		priv->netdev->stats.tx_bytes += priv->tx_skb->len;
		// dev_kfree_skb(priv->tx_skb); 
		priv->tx_skb=NULL;
		priv->tx_len=0;
		netif_wake_queue(priv->netdev);
	}
	/* XPR : write a new block in transmit FIFO */
	else if (priv->tx_len < priv->tx_skb->len) {
		size = priv->tx_skb->len - priv->tx_len;
		if (size > 32)
			size = 32;

		for (idx=0; idx < size; idx++)
			base_addr->mFIFO.mXFIFO[idx & 1] = tx_buff[priv->tx_len + idx];

		priv->tx_len += size;

		if (priv->tx_len == priv->tx_skb->len) 
			base_addr->mCMDR |= ((1 << 3) | (1 << 1));
		else
			setbits8(&(base_addr->mCMDR), 1 << 3);
	}

	return 0;
}


/* Handler IRQ */
irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	struct pef2256_dev_priv *priv = (struct pef2256_dev_priv *)dev_priv;
	pef2256_regs *base_addr;
	u8 GIS;

	/* Do E1 stuff */
	base_addr = (pef2256_regs *)priv->base_addr;
	GIS = base_addr->mGIS;

	priv->ISR0 = priv->ISR1 = 0;

	/* We only care about ISR0 and ISR1 */
	/* ISR0 */
	if (GIS & 1)
		priv->ISR0 = base_addr->mISR0 & ~(base_addr->mIMR0);
	/* ISR1 */
	if (GIS & (1 << 1))
		priv->ISR1 = base_addr->mISR1 & ~(base_addr->mIMR1);

	/* Don't do anything else before init is done */
	if (! priv->init_done)
		return IRQ_HANDLED;

	/* RDO : Receive data overflow -> RX error */
	if (priv->ISR1 & (1 << 6)) {
printk("*********** pef2256_irq : data overflow ! \n");
		/* Acknowledge the FIFO */
		setbits8(&(base_addr->mCMDR), 1 << 7);
		priv->netdev->stats.rx_errors++;
		/* RME received ? */
		if (priv->ISR0 & (1 << 7)) 
			priv->rx_len = 0;
		else
			priv->rx_len = -1;
		return IRQ_HANDLED;
	}

	/* XDU : Transmit data underrun -> TX error */
	if (priv->ISR1 & (1 << 4)) {
printk("*********** pef2256_irq : data underrun ! \n");
		priv->netdev->stats.tx_errors++;
		// dev_kfree_skb(priv->tx_skb);
		priv->tx_skb=NULL;
		netif_wake_queue(priv->netdev);
		return IRQ_HANDLED;
	}

	/* RPF or RME : FIFO received -> call pef2256_rx */
	if (priv->ISR0 & (1 | (1 << 7)))
		pef2256_rx(priv);

	/* XPR or ALLS : FIFO sent -> call pef2256_tx */
	if (priv->ISR1 & (1 | (1 << 5)))
		pef2256_tx(priv);

	return IRQ_HANDLED;
}


static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	int idx, size;
	pef2256_regs *base_addr;
	u8 *tx_buff = skb->data;

	base_addr = (pef2256_regs *)priv->base_addr;

	/* demande emission trame skb->data de longueur skb->len */
	priv->tx_skb = skb;
	priv->tx_len = 0;

	/* Do E1 stuff */
	size = priv->tx_skb->len - priv->tx_len;
	if (size > 32)
		size = 32;

	for (idx=0; idx < size; idx++)
		base_addr->mFIFO.mXFIFO[idx & 1] = tx_buff[priv->tx_len + idx];

	priv->tx_len += size;

	setbits8(&(base_addr->mCMDR), 1 << 3);
	if (priv->tx_len == priv->tx_skb->len)
		setbits8(&(base_addr->mCMDR), 1 << 1);

	netif_stop_queue(netdev);
	return NETDEV_TX_OK;
}

static const struct net_device_ops pef2256_ops = {
	.ndo_open       = pef2256_open,
	.ndo_stop       = pef2256_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = pef2256_ioctl,
};


/* A vérifier : Qu'est ce que supporte le composant au juste ? */
static int pef2256_hdlc_attach(struct net_device *netdev, 
				unsigned short encoding, unsigned short parity)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	if (encoding != ENCODING_NRZ &&
	    encoding != ENCODING_NRZI &&
	    encoding != ENCODING_FM_MARK &&
	    encoding != ENCODING_FM_SPACE &&
	    encoding != ENCODING_MANCHESTER)
		return -EINVAL;

	if (parity != PARITY_NONE &&
	    parity != PARITY_CRC16_PR0_CCITT &&
	    parity != PARITY_CRC16_PR1_CCITT &&
	    parity != PARITY_CRC32_PR0_CCITT &&
	    parity != PARITY_CRC32_PR1_CCITT)
		return -EINVAL;

	priv->encoding = encoding;
	priv->parity = parity;
	return 0;
}


/*
 * Chargement du module
 */
static const struct of_device_id pef2256_match[];
static int pef2256_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct pef2256_dev_priv *priv;
	int ret = -ENOMEM;
	struct net_device *netdev;
	hdlc_device *hdlc;
	int sys_ret;
	pef2256_regs *base_addr;
	struct device_node *np = (&ofdev->dev)->of_node;
	const u32 *data;
	int len;

	match = of_match_device(pef2256_match, &ofdev->dev);
	if (!match)
		return -EINVAL;

	dev_err(&ofdev->dev, "Found PEF2256\n");

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (! priv)
		return ret;

	priv->dev = &ofdev->dev;

	data = of_get_property(np, "data-rate", &len);
	if (!data || len != 4) {
		dev_err(&ofdev->dev,"failed to read data-rate -> using 8Mb\n");
		priv->data_rate = DATA_RATE_8M;
	}
	else
		priv->data_rate = *data;

	data = of_get_property(np, "channel-phase", &len);
	if (!data || len != 4) {
		dev_err(&ofdev->dev,"failed to read channel phase -> using 0\n");
		priv->channel_phase = CHANNEL_PHASE_0;
	}
	else
		priv->channel_phase = *data;

	data = of_get_property(np, "rising-edge-sync-pulse", NULL);
	if (!data) {
		dev_err(&ofdev->dev, "failed to read rising edge sync pulse -> using \"transmit\"\n");
		strcpy (priv->rising_edge_sync_pulse, "transmit");
	}
	else if (strcmp ((char *)data, "transmit") && 
			strcmp ((char *)data, "receive")) {
		dev_err(&ofdev->dev, "invalid rising edge sync pulse -> using \"transmit\"\n");
		strcpy (priv->rising_edge_sync_pulse, "transmit");
	}
	else
		strncpy (priv->rising_edge_sync_pulse, (char *)data, 10);

	/* Do E1 stuff */
	priv->irq = of_irq_to_resource(np, 0, NULL);
	if (!priv->irq) {
		dev_err(priv->dev, "no irq defined\n");
		return -EINVAL;
	}

	/* remappage de l'adresse physique du composant E1 */
	priv->base_addr = of_iomap(np, 0);
	if (!priv->base_addr) {
		dev_err(&ofdev->dev,"of_iomap failed\n");
		kfree(priv);
		return ret;
	}
	
	/* lecture du registre d'identification */
	base_addr = (pef2256_regs *)priv->base_addr;
	priv->component_id = E_DRV_E1_VERSION_UNDEF;
	if (base_addr->mVSTR == 0x00) {
		if ((base_addr->mWID & M_DRV_E1_WID_IDENT_1) == M_DRV_E1_WID_IDENT_1_2)
			priv->component_id = E_DRV_E1_VERSION_1_2;
	}
	else if (base_addr->mVSTR == 0x05) {
		if ((base_addr->mWID & M_DRV_E1_WID_IDENT_2) == M_DRV_E1_WID_IDENT_2_1)
			priv->component_id = E_DRV_E1_VERSION_2_1;
		else if ((base_addr->mWID & M_DRV_E1_WID_IDENT_2) == M_DRV_E1_WID_IDENT_2_2)
			priv->component_id = E_DRV_E1_VERSION_2_2;
	}

	priv->tx_skb = NULL;

	/* Par defaut ; Tx et Rx sur TS 1 */
	priv->Tx_TS = priv->Rx_TS = 0x40000000;
	/* Par defaut ; mode = MASTER */
	priv->mode = 0;

	netdev = alloc_hdlcdev(priv);
	if (! netdev) {
		ret = -ENOMEM;
		return ret;
	}

	priv->netdev = netdev;
	hdlc = dev_to_hdlc(netdev);
/* ???
	d->base_addr = ;
	d->irq = ;
*/
	netdev->netdev_ops = &pef2256_ops;
	SET_NETDEV_DEV(netdev, &ofdev->dev);
	hdlc->attach = pef2256_hdlc_attach;
	hdlc->xmit = pef2256_start_xmit;
	
	dev_set_drvdata(&ofdev->dev, netdev);

	ret = register_hdlc_device(netdev);
	if (ret < 0) {
		pr_err("unable to register\n");
		return ret;
	}

	sys_ret = 0;
	sys_ret |= device_create_file(priv->dev, &dev_attr_mode);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Tx_TS);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Rx_TS);
	sys_ret |= device_create_file(priv->dev, &dev_attr_regs);

	if (sys_ret) {
		device_remove_file(priv->dev, &dev_attr_mode);
		unregister_hdlc_device(priv->netdev);
		free_netdev(priv->netdev);
	}

	priv->init_done = 0;

	return 0;
}


/*
 * Suppression du module
 */
static int pef2256_remove(struct platform_device *ofdev)
{
	struct net_device *ndev = dev_get_drvdata(&ofdev->dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	device_remove_file(priv->dev, &dev_attr_Rx_TS);
	device_remove_file(priv->dev, &dev_attr_Tx_TS);
	device_remove_file(priv->dev, &dev_attr_mode);

	unregister_hdlc_device(priv->netdev);
	free_netdev(priv->netdev);

	/* Do E1 stuff */

	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(ofdev);
	return 0;
}

static const struct of_device_id pef2256_match[] = {
	{
		.compatible = "s3k,mcr3000-e1-wan",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pef2256_match);


static struct platform_driver pef2256_driver = {
	.probe		= pef2256_probe,
	.remove		= pef2256_remove,
	.driver		= {
		.name	= "pef2256",
		.owner	= THIS_MODULE,
		.of_match_table	= pef2256_match,
	},
};


static int __init pef2256_init(void)
{
	int ret;
	ret = platform_driver_register(&pef2256_driver);
	return ret;
}
module_init(pef2256_init);


static void __exit pef2256_exit(void)
{
	platform_driver_unregister(&pef2256_driver);
}
module_exit(pef2256_exit);


/* INFORMATIONS GENERALES */
MODULE_AUTHOR(M_DRV_PEF2256_AUTHOR);
MODULE_VERSION(M_DRV_PEF2256_VERSION);
MODULE_DESCRIPTION("Infineon PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
