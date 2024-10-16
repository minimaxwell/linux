// SPDX-License-Identifier: GPL-2.0+
#include <linux/linkmode.h>

/**
 * linkmode_resolve_pause - resolve the allowable pause modes
 * @local_adv: local advertisement in ethtool format
 * @partner_adv: partner advertisement in ethtool format
 * @tx_pause: pointer to bool to indicate whether transmit pause should be
 * enabled.
 * @rx_pause: pointer to bool to indicate whether receive pause should be
 * enabled.
 *
 * Flow control is resolved according to our and the link partners
 * advertisements using the following drawn from the 802.3 specs:
 *  Local device  Link partner
 *  Pause AsymDir Pause AsymDir Result
 *    0     X       0     X     Disabled
 *    0     1       1     0     Disabled
 *    0     1       1     1     TX
 *    1     0       0     X     Disabled
 *    1     X       1     X     TX+RX
 *    1     1       0     1     RX
 */
void linkmode_resolve_pause(const unsigned long *local_adv,
			    const unsigned long *partner_adv,
			    bool *tx_pause, bool *rx_pause)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(m);

	linkmode_and(m, local_adv, partner_adv);
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT, m)) {
		*tx_pause = true;
		*rx_pause = true;
	} else if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, m)) {
		*tx_pause = linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
					      partner_adv);
		*rx_pause = linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
					      local_adv);
	} else {
		*tx_pause = false;
		*rx_pause = false;
	}
}
EXPORT_SYMBOL_GPL(linkmode_resolve_pause);

/**
 * linkmode_set_pause - set the pause mode advertisement
 * @advertisement: advertisement in ethtool format
 * @tx: boolean from ethtool struct ethtool_pauseparam tx_pause member
 * @rx: boolean from ethtool struct ethtool_pauseparam rx_pause member
 *
 * Configure the advertised Pause and Asym_Pause bits according to the
 * capabilities of provided in @tx and @rx.
 *
 * We convert as follows:
 *  tx rx  Pause AsymDir
 *  0  0   0     0
 *  0  1   1     1
 *  1  0   0     1
 *  1  1   1     0
 *
 * Note: this translation from ethtool tx/rx notation to the advertisement
 * is actually very problematical. Here are some examples:
 *
 * For tx=0 rx=1, meaning transmit is unsupported, receive is supported:
 *
 *  Local device  Link partner
 *  Pause AsymDir Pause AsymDir Result
 *    1     1       1     0     TX + RX - but we have no TX support.
 *    1     1       0     1	Only this gives RX only
 *
 * For tx=1 rx=1, meaning we have the capability to transmit and receive
 * pause frames:
 *
 *  Local device  Link partner
 *  Pause AsymDir Pause AsymDir Result
 *    1     0       0     1     Disabled - but since we do support tx and rx,
 *				this should resolve to RX only.
 *
 * Hence, asking for:
 *  rx=1 tx=0 gives Pause+AsymDir advertisement, but we may end up
 *            resolving to tx+rx pause or only rx pause depending on
 *            the partners advertisement.
 *  rx=0 tx=1 gives AsymDir only, which will only give tx pause if
 *            the partners advertisement allows it.
 *  rx=1 tx=1 gives Pause only, which will only allow tx+rx pause
 *            if the other end also advertises Pause.
 */
void linkmode_set_pause(unsigned long *advertisement, bool tx, bool rx)
{
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Pause_BIT, advertisement, rx);
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, advertisement,
			 rx ^ tx);
}
EXPORT_SYMBOL_GPL(linkmode_set_pause);

/**
 * phylink_interface_max_speed() - get the maximum speed of a phy interface
 * @interface: phy interface mode defined by &typedef phy_interface_t
 *
 * Determine the maximum speed of a phy interface. This is intended to help
 * determine the correct speed to pass to the MAC when the phy is performing
 * rate matching.
 *
 * Return: The maximum speed of @interface
 */
int linkmode_interface_max_speed(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_100BASEX:
	case PHY_INTERFACE_MODE_REVRMII:
	case PHY_INTERFACE_MODE_RMII:
	case PHY_INTERFACE_MODE_SMII:
	case PHY_INTERFACE_MODE_REVMII:
	case PHY_INTERFACE_MODE_MII:
		return SPEED_100;

	case PHY_INTERFACE_MODE_TBI:
	case PHY_INTERFACE_MODE_MOCA:
	case PHY_INTERFACE_MODE_RTBI:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_1000BASEKX:
	case PHY_INTERFACE_MODE_TRGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_PSGMII:
	case PHY_INTERFACE_MODE_QSGMII:
	case PHY_INTERFACE_MODE_QUSGMII:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
		return SPEED_1000;

	case PHY_INTERFACE_MODE_2500BASEX:
	case PHY_INTERFACE_MODE_10G_QXGMII:
		return SPEED_2500;

	case PHY_INTERFACE_MODE_5GBASER:
		return SPEED_5000;

	case PHY_INTERFACE_MODE_XGMII:
	case PHY_INTERFACE_MODE_RXAUI:
	case PHY_INTERFACE_MODE_XAUI:
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10GKR:
	case PHY_INTERFACE_MODE_USXGMII:
		return SPEED_10000;

	case PHY_INTERFACE_MODE_25GBASER:
		return SPEED_25000;

	case PHY_INTERFACE_MODE_XLGMII:
		return SPEED_40000;

	case PHY_INTERFACE_MODE_INTERNAL:
	case PHY_INTERFACE_MODE_NA:
	case PHY_INTERFACE_MODE_MAX:
		/* No idea! Garbage in, unknown out */
		return SPEED_UNKNOWN;
	}

	/* If we get here, someone forgot to add an interface mode above */
	WARN_ON_ONCE(1);
	return SPEED_UNKNOWN;
}
EXPORT_SYMBOL_GPL(linkmode_interface_max_speed);

/**
 * phylink_caps_to_linkmodes() - Convert capabilities to ethtool link modes
 * @linkmodes: ethtool linkmode mask (must be already initialised)
 * @caps: bitmask of MAC capabilities
 *
 * Set all possible pause, speed and duplex linkmodes in @linkmodes that are
 * supported by the @caps. @linkmodes must have been initialised previously.
 */
void linkmodes_from_caps(unsigned long *linkmodes, unsigned long caps)
{
	if (caps & MAC_SYM_PAUSE)
		__set_bit(ETHTOOL_LINK_MODE_Pause_BIT, linkmodes);

	if (caps & MAC_ASYM_PAUSE)
		__set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, linkmodes);

	if (caps & MAC_10HD) {
		__set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_P2MP_Half_BIT, linkmodes);
	}

	if (caps & MAC_10FD) {
		__set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_Full_BIT, linkmodes);
	}

	if (caps & MAC_100HD) {
		__set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseFX_Half_BIT, linkmodes);
	}

	if (caps & MAC_100FD) {
		__set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseFX_Full_BIT, linkmodes);
	}

	if (caps & MAC_1000HD)
		__set_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT, linkmodes);

	if (caps & MAC_1000FD) {
		__set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseX_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, linkmodes);
	}

	if (caps & MAC_2500FD) {
		__set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_2500baseX_Full_BIT, linkmodes);
	}

	if (caps & MAC_5000FD)
		__set_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT, linkmodes);

	if (caps & MAC_10000FD) {
		__set_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseR_FEC_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseLR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseER_Full_BIT, linkmodes);
	}

	if (caps & MAC_25000FD) {
		__set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_25000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, linkmodes);
	}

	if (caps & MAC_40000FD) {
		__set_bit(ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT, linkmodes);
	}

	if (caps & MAC_50000FD) {
		__set_bit(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseDR_Full_BIT, linkmodes);
	}

	if (caps & MAC_56000FD) {
		__set_bit(ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT, linkmodes);
	}

	if (caps & MAC_100000FD) {
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, linkmodes);
	}

	if (caps & MAC_200000FD) {
		__set_bit(ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseLR2_ER2_FR2_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseDR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseCR2_Full_BIT, linkmodes);
	}

	if (caps & MAC_400000FD) {
		__set_bit(ETHTOOL_LINK_MODE_400000baseKR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseSR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseLR8_ER8_FR8_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseDR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseCR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseLR4_ER4_FR4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseDR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseCR4_Full_BIT, linkmodes);
	}
}
EXPORT_SYMBOL_GPL(linkmodes_from_caps);

static struct {
	unsigned long mask;
	int speed;
	unsigned int duplex;
} linkmode_caps_params[] = {
	{ MAC_400000FD, SPEED_400000, DUPLEX_FULL },
	{ MAC_200000FD, SPEED_200000, DUPLEX_FULL },
	{ MAC_100000FD, SPEED_100000, DUPLEX_FULL },
	{ MAC_56000FD,  SPEED_56000,  DUPLEX_FULL },
	{ MAC_50000FD,  SPEED_50000,  DUPLEX_FULL },
	{ MAC_40000FD,  SPEED_40000,  DUPLEX_FULL },
	{ MAC_25000FD,  SPEED_25000,  DUPLEX_FULL },
	{ MAC_20000FD,  SPEED_20000,  DUPLEX_FULL },
	{ MAC_10000FD,  SPEED_10000,  DUPLEX_FULL },
	{ MAC_5000FD,   SPEED_5000,   DUPLEX_FULL },
	{ MAC_2500FD,   SPEED_2500,   DUPLEX_FULL },
	{ MAC_1000FD,   SPEED_1000,   DUPLEX_FULL },
	{ MAC_1000HD,   SPEED_1000,   DUPLEX_HALF },
	{ MAC_100FD,    SPEED_100,    DUPLEX_FULL },
	{ MAC_100HD,    SPEED_100,    DUPLEX_HALF },
	{ MAC_10FD,     SPEED_10,     DUPLEX_FULL },
	{ MAC_10HD,     SPEED_10,     DUPLEX_HALF },
};

/**
 * phylink_cap_from_speed_duplex - Get mac capability from speed/duplex
 * @speed: the speed to search for
 * @duplex: the duplex to search for
 *
 * Find the mac capability for a given speed and duplex.
 *
 * Return: A mask with the mac capability patching @speed and @duplex, or 0 if
 *         there were no matches.
 */
unsigned long linkmode_cap_from_speed_duplex(int speed, unsigned int duplex)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(linkmode_caps_params); i++) {
		if (speed == linkmode_caps_params[i].speed &&
		    duplex == linkmode_caps_params[i].duplex)
			return linkmode_caps_params[i].mask;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(linkmode_cap_from_speed_duplex);

unsigned long linkmode_caps_max_speed(int speed)
{
	unsigned long caps = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(linkmode_caps_params); i++)
		if (linkmode_caps_params[i].speed <= speed)
			caps |= linkmode_caps_params[i].mask;

	return caps;
}

/**
 * phylink_get_capabilities() - get capabilities for a given MAC
 * @interface: phy interface mode defined by &typedef phy_interface_t
 * @mac_capabilities: bitmask of MAC capabilities
 * @rate_matching: type of rate matching being performed
 *
 * Get the MAC capabilities that are supported by the @interface mode and
 * @mac_capabilities.
 */
unsigned long linkmode_interface_get_capabilities(phy_interface_t interface,
						  unsigned long mac_capabilities,
						  int rate_matching)
{
	int max_speed = linkmode_interface_max_speed(interface);
	unsigned long caps = MAC_SYM_PAUSE | MAC_ASYM_PAUSE;
	unsigned long matched_caps = 0;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		caps |= MAC_10000FD | MAC_5000FD;
		fallthrough;

	case PHY_INTERFACE_MODE_10G_QXGMII:
		caps |= MAC_2500FD;
		fallthrough;

	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_PSGMII:
	case PHY_INTERFACE_MODE_QSGMII:
	case PHY_INTERFACE_MODE_QUSGMII:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
		caps |= MAC_1000HD | MAC_1000FD;
		fallthrough;

	case PHY_INTERFACE_MODE_REVRMII:
	case PHY_INTERFACE_MODE_RMII:
	case PHY_INTERFACE_MODE_SMII:
	case PHY_INTERFACE_MODE_REVMII:
	case PHY_INTERFACE_MODE_MII:
		caps |= MAC_10HD | MAC_10FD;
		fallthrough;

	case PHY_INTERFACE_MODE_100BASEX:
		caps |= MAC_100HD | MAC_100FD;
		break;

	case PHY_INTERFACE_MODE_TBI:
	case PHY_INTERFACE_MODE_MOCA:
	case PHY_INTERFACE_MODE_RTBI:
	case PHY_INTERFACE_MODE_1000BASEX:
		caps |= MAC_1000HD;
		fallthrough;
	case PHY_INTERFACE_MODE_1000BASEKX:
	case PHY_INTERFACE_MODE_TRGMII:
		caps |= MAC_1000FD;
		break;

	case PHY_INTERFACE_MODE_2500BASEX:
		caps |= MAC_2500FD;
		break;

	case PHY_INTERFACE_MODE_5GBASER:
		caps |= MAC_5000FD;
		break;

	case PHY_INTERFACE_MODE_XGMII:
	case PHY_INTERFACE_MODE_RXAUI:
	case PHY_INTERFACE_MODE_XAUI:
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10GKR:
		caps |= MAC_10000FD;
		break;

	case PHY_INTERFACE_MODE_25GBASER:
		caps |= MAC_25000FD;
		break;

	case PHY_INTERFACE_MODE_XLGMII:
		caps |= MAC_40000FD;
		break;

	case PHY_INTERFACE_MODE_INTERNAL:
		caps |= ~0;
		break;

	case PHY_INTERFACE_MODE_NA:
	case PHY_INTERFACE_MODE_MAX:
		break;
	}

	switch (rate_matching) {
	case RATE_MATCH_OPEN_LOOP:
		/* TODO */
		fallthrough;
	case RATE_MATCH_NONE:
		matched_caps = 0;
		break;
	case RATE_MATCH_PAUSE: {
		/* The MAC must support asymmetric pause towards the local
		 * device for this. We could allow just symmetric pause, but
		 * then we might have to renegotiate if the link partner
		 * doesn't support pause. This is because there's no way to
		 * accept pause frames without transmitting them if we only
		 * support symmetric pause.
		 */
		if (!(mac_capabilities & MAC_SYM_PAUSE) ||
		    !(mac_capabilities & MAC_ASYM_PAUSE))
			break;

		/* We can't adapt if the MAC doesn't support the interface's
		 * max speed at full duplex.
		 */
		if (mac_capabilities &
		    linkmode_cap_from_speed_duplex(max_speed, DUPLEX_FULL)) {
			/* Although a duplex-matching phy might exist, we
			 * conservatively remove these modes because the MAC
			 * will not be aware of the half-duplex nature of the
			 * link.
			 */
			matched_caps = GENMASK(__fls(caps), __fls(MAC_10HD));
			matched_caps &= ~(MAC_1000HD | MAC_100HD | MAC_10HD);
		}
		break;
	}
	case RATE_MATCH_CRS:
		/* The MAC must support half duplex at the interface's max
		 * speed.
		 */
		if (mac_capabilities &
		    linkmode_cap_from_speed_duplex(max_speed, DUPLEX_HALF)) {
			matched_caps = GENMASK(__fls(caps), __fls(MAC_10HD));
			matched_caps &= mac_capabilities;
		}
		break;
	}

	return (caps & mac_capabilities) | matched_caps;
}
EXPORT_SYMBOL_GPL(linkmode_interface_get_capabilities);

/* TODO : Move that out from phylink, this is generic linkmode handling */
void linkmodes_from_interface(phy_interface_t interface,
			      int rate_matching,
			      unsigned long *linkmodes)
{
	unsigned long caps;

	caps = ~(MAC_SYM_PAUSE | MAC_ASYM_PAUSE);
	caps = linkmode_interface_get_capabilities(interface, caps, RATE_MATCH_NONE);
	linkmodes_from_caps(linkmodes, caps);
}
EXPORT_SYMBOL_GPL(linkmodes_from_interface);
