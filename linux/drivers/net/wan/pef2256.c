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

static ssize_t fs_attr_master_show(struct device *dev, 
			struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct pef2256_dev_priv *priv = dev_to_hdlc(ndev)->priv;

	return sprintf(buf, "%d\n", priv->mode);
}


static ssize_t fs_attr_master_store(struct device *dev, 
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
	if (reconfigure) {
		init_FALC(priv);
		Config_HDLC(priv);
	}

	return 0;
}

static DEVICE_ATTR(master, S_IRUGO | S_IWUSR, fs_attr_master_show, fs_attr_master_store);



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
	if (reconfigure)
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
	if (reconfigure)
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
	unsigned char *ptr;
//	tDRVE1IdentDev _Dev;
	int TS_idx;
	pef2256_regs *base_addr;

//	/* arret systématique du canal
//	   arret des ITs du canal HDLC
//	   positionnement ITs émission */
//	_Dev.mSelIt = (1 << E_DRV_E1_HDLC_XPR) | (1 << E_DRV_E1_HDLC_ALLS) | (1 << E_DRV_E1_HDLC_XDU);
//	/* positionnement ITs réception */
//	_Dev.mSelIt |= (1 << E_DRV_E1_HDLC_RPF) | (1 << E_DRV_E1_HDLC_RME);
//	_Dev.mAppel = M_DRV_E1_UNREG_HDL;
//	_Dev.mIdent = E_DRV_E1_IT_HDLC1;
//	fDRVE1PoseHdl(&_Dev, NULL, NULL);

	/* attente vidage des fifos */
	udelay((2 * M_DRV_E1_TAILLE_FIFO) * 125);

	/* positionnement de l'adresse du trameur E1 */
	base_addr = (pef2256_regs *)priv->base_addr;

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

//	/* gestion des ITs du canal HDLC 1 */
//	_Dev.mIdent = E_DRV_E1_IT_HDLC1;
//	/* positionnement ITs émission */
//	_Dev.mSelIt = (1 << E_DRV_E1_HDLC_XPR) | (1 << E_DRV_E1_HDLC_ALLS) | (1 << E_DRV_E1_HDLC_XDU);
//	/* positionnement ITs réception */
//	if (ipParam->mRes[0] == E_DRV_E1_MODE_HDLC_MAX)
//		_Dev.mSelIt |= (1 << E_DRV_E1_HDLC_RPF) | (1 << E_DRV_E1_HDLC_RME);
//	if (ipParam->mMode != E_DRV_E1_MODE_HDLC_OFF)
//		_Dev.mAppel = M_DRV_E1_REG_HDL;
//	else
//		_Dev.mAppel = M_DRV_E1_UNREG_HDL;
//	fDRVE1PoseHdl(&_Dev, NULL, NULL);

	/* initialisation des trames émission et réception */
	memset(&priv->Tx_buffer.mOctet[0], 0, sizeof(tDRVE1TrameEm));
	memset(&priv->Rx_buffer.mOctet[0], 0, sizeof(tDRVE1TrameRec));
	priv->Tx_buffer.mTrEnreg.mTrame.mAdes = M_DRV_E1_ADES_INIT;
	priv->Tx_buffer.mTrEnreg.mTrame.mCtl = M_DRV_E1_CTL_INIT;
	priv->Tx_buffer.mTrEnreg.mIxTrft = 0;
	/* transfert d'un premier paquet de x octets */
	ptr = (unsigned char *)&priv->Tx_buffer.mTrEnreg.mTrame;
	/* transfert dans FIFO possible */
	if (base_addr->mSIS & 0x40) {
		for (i = 0; i < M_DRV_E1_TAILLE_FIFO; i++) {
			base_addr->mFIFO.mXFIFO[i % sizeof(short)] = *(ptr++);
			priv->Tx_buffer.mTrEnreg.mIxTrft++;
			if (priv->Tx_buffer.mTrEnreg.mIxTrft >= sizeof(tDRVE1TrameEnreg))
				break;
		}
		/* positionnement transfert trame enregistrement (CMDR.XHF = 1) */
		setbits8(&(base_addr->mCMDR), 1 << 3);
		/* positionnement fin transfert trame enregistrement (CMDR.XME = 1) */
		if (i < M_DRV_E1_TAILLE_FIFO)
			setbits8(&(base_addr->mCMDR), 1 << 1);
	}
	/* transfert dans FIFO impossible */
	else
		return -EINVAL;
	
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

	dev_err(priv->dev, "After init_FALC : FMR1=%0x FMR2=%0x, XSP=%0x\n", 
			base_addr->mFMR1, base_addr->mFMR2, base_addr->mXSP);

	if (ret < 0)
		return ret; 
	

	priv->tx_skbuff = NULL;
	priv->rx_len = 0;

	netif_start_queue(netdev);
	netif_carrier_on(netdev);
	return 0;
}


static int pef2256_close(struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	netif_stop_queue(netdev);
	cancel_delayed_work_sync(&priv->rx_timeout_queue);

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
static int pef2256_rx(int irq, void *dev_id)
{
	struct net_device *netdev = (struct net_device *)dev_id;
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	struct sk_buff *skb;

	if (! priv->rx_len)
		schedule_delayed_work(&priv->rx_timeout_queue, RX_TIMEOUT);

	/* Do E1 stuff */

	/* Si trame entierement arrivee */
	cancel_delayed_work_sync(&priv->rx_timeout_queue);
	skb = dev_alloc_skb(priv->rx_len);
	if (! skb) {
		priv->rx_len = 0;
		netdev->stats.rx_dropped++;
		return -ENOMEM;
	}
	memcpy(skb->data, priv->rx_buff, priv->rx_len);
	skb_put(skb, priv->rx_len);
	priv->rx_len = 0;
	skb->protocol = hdlc_type_trans(skb, priv->netdev);
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += skb->len;
	netif_rx(skb);

	return 0;
}


/* Handler IT ecriture */
static int pef2256_xmit(int irq, void *dev_id)
{
	struct net_device *netdev = (struct net_device *)dev_id;
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	/* Do E1 stuff */

	/* Si trame entierement transferee */
	netdev->stats.tx_packets++;
	netdev->stats.tx_bytes += priv->tx_skbuff->len;
	dev_kfree_skb(priv->tx_skbuff);
	priv->tx_skbuff=NULL;
	netif_wake_queue(netdev);

	return 0;
}


/* Handler IRQ */
irqreturn_t pef2256_irq(int irq, void *dev_priv)
{
	struct pef2256_dev_priv *priv = (struct pef2256_dev_priv *)dev_priv;

	/* Do E1 stuff */

	return 0;
}


static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	/* demande emission trame skb->data de longueur skb->len */
	priv->tx_skbuff = skb;

	/* Do E1 stuff */

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

	priv->tx_skbuff = NULL;

	/* Par defaut ; Tx et Rx sur TS 1 */
	priv->Tx_TS = priv->Rx_TS = 0x40000000;

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
	sys_ret |= device_create_file(priv->dev, &dev_attr_master);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Tx_TS);
	sys_ret |= device_create_file(priv->dev, &dev_attr_Rx_TS);

	if (sys_ret) {
		device_remove_file(priv->dev, &dev_attr_master);
		unregister_hdlc_device(priv->netdev);
		free_netdev(priv->netdev);
	}

	dev_err(priv->dev, "Leaving pef2256_probe : FMR1=%0x FMR2=%0x, XSP=%0x\n", 
		base_addr->mFMR1, base_addr->mFMR2, base_addr->mXSP);

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
	device_remove_file(priv->dev, &dev_attr_master);

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
