#ifndef __LINKMODE_H
#define __LINKMODE_H

#include <linux/bitmap.h>
#include <linux/ethtool.h>
#include <uapi/linux/ethtool.h>

/**
 * enum phy_interface_t - Interface Mode definitions
 *
 * @PHY_INTERFACE_MODE_NA: Not Applicable - don't touch
 * @PHY_INTERFACE_MODE_INTERNAL: No interface, MAC and PHY combined
 * @PHY_INTERFACE_MODE_MII: Media-independent interface
 * @PHY_INTERFACE_MODE_GMII: Gigabit media-independent interface
 * @PHY_INTERFACE_MODE_SGMII: Serial gigabit media-independent interface
 * @PHY_INTERFACE_MODE_TBI: Ten Bit Interface
 * @PHY_INTERFACE_MODE_REVMII: Reverse Media Independent Interface
 * @PHY_INTERFACE_MODE_RMII: Reduced Media Independent Interface
 * @PHY_INTERFACE_MODE_REVRMII: Reduced Media Independent Interface in PHY role
 * @PHY_INTERFACE_MODE_RGMII: Reduced gigabit media-independent interface
 * @PHY_INTERFACE_MODE_RGMII_ID: RGMII with Internal RX+TX delay
 * @PHY_INTERFACE_MODE_RGMII_RXID: RGMII with Internal RX delay
 * @PHY_INTERFACE_MODE_RGMII_TXID: RGMII with Internal RX delay
 * @PHY_INTERFACE_MODE_RTBI: Reduced TBI
 * @PHY_INTERFACE_MODE_SMII: Serial MII
 * @PHY_INTERFACE_MODE_XGMII: 10 gigabit media-independent interface
 * @PHY_INTERFACE_MODE_XLGMII:40 gigabit media-independent interface
 * @PHY_INTERFACE_MODE_MOCA: Multimedia over Coax
 * @PHY_INTERFACE_MODE_PSGMII: Penta SGMII
 * @PHY_INTERFACE_MODE_QSGMII: Quad SGMII
 * @PHY_INTERFACE_MODE_TRGMII: Turbo RGMII
 * @PHY_INTERFACE_MODE_100BASEX: 100 BaseX
 * @PHY_INTERFACE_MODE_1000BASEX: 1000 BaseX
 * @PHY_INTERFACE_MODE_2500BASEX: 2500 BaseX
 * @PHY_INTERFACE_MODE_5GBASER: 5G BaseR
 * @PHY_INTERFACE_MODE_RXAUI: Reduced XAUI
 * @PHY_INTERFACE_MODE_XAUI: 10 Gigabit Attachment Unit Interface
 * @PHY_INTERFACE_MODE_10GBASER: 10G BaseR
 * @PHY_INTERFACE_MODE_25GBASER: 25G BaseR
 * @PHY_INTERFACE_MODE_USXGMII:  Universal Serial 10GE MII
 * @PHY_INTERFACE_MODE_10GKR: 10GBASE-KR - with Clause 73 AN
 * @PHY_INTERFACE_MODE_QUSGMII: Quad Universal SGMII
 * @PHY_INTERFACE_MODE_1000BASEKX: 1000Base-KX - with Clause 73 AN
 * @PHY_INTERFACE_MODE_10G_QXGMII: 10G-QXGMII - 4 ports over 10G USXGMII
 * @PHY_INTERFACE_MODE_MAX: Book keeping
 *
 * Describes the interface between the MAC and PHY.
 */
typedef enum {
	PHY_INTERFACE_MODE_NA,
	PHY_INTERFACE_MODE_INTERNAL,
	PHY_INTERFACE_MODE_MII,
	PHY_INTERFACE_MODE_GMII,
	PHY_INTERFACE_MODE_SGMII,
	PHY_INTERFACE_MODE_TBI,
	PHY_INTERFACE_MODE_REVMII,
	PHY_INTERFACE_MODE_RMII,
	PHY_INTERFACE_MODE_REVRMII,
	PHY_INTERFACE_MODE_RGMII,
	PHY_INTERFACE_MODE_RGMII_ID,
	PHY_INTERFACE_MODE_RGMII_RXID,
	PHY_INTERFACE_MODE_RGMII_TXID,
	PHY_INTERFACE_MODE_RTBI,
	PHY_INTERFACE_MODE_SMII,
	PHY_INTERFACE_MODE_XGMII,
	PHY_INTERFACE_MODE_XLGMII,
	PHY_INTERFACE_MODE_MOCA,
	PHY_INTERFACE_MODE_PSGMII,
	PHY_INTERFACE_MODE_QSGMII,
	PHY_INTERFACE_MODE_TRGMII,
	PHY_INTERFACE_MODE_100BASEX,
	PHY_INTERFACE_MODE_1000BASEX,
	PHY_INTERFACE_MODE_2500BASEX,
	PHY_INTERFACE_MODE_5GBASER,
	PHY_INTERFACE_MODE_RXAUI,
	PHY_INTERFACE_MODE_XAUI,
	/* 10GBASE-R, XFI, SFI - single lane 10G Serdes */
	PHY_INTERFACE_MODE_10GBASER,
	PHY_INTERFACE_MODE_25GBASER,
	PHY_INTERFACE_MODE_USXGMII,
	/* 10GBASE-KR - with Clause 73 AN */
	PHY_INTERFACE_MODE_10GKR,
	PHY_INTERFACE_MODE_QUSGMII,
	PHY_INTERFACE_MODE_1000BASEKX,
	PHY_INTERFACE_MODE_10G_QXGMII,
	PHY_INTERFACE_MODE_MAX,
} phy_interface_t;



enum {
	/* MAC_SYM_PAUSE and MAC_ASYM_PAUSE are used when configuring our
	 * autonegotiation advertisement. They correspond to the PAUSE and
	 * ASM_DIR bits defined by 802.3, respectively.
	 *
	 * The following table lists the values of tx_pause and rx_pause which
	 * might be requested in mac_link_up. The exact values depend on either
	 * the results of autonegotation (if MLO_PAUSE_AN is set) or user
	 * configuration (if MLO_PAUSE_AN is not set).
	 *
	 * MAC_SYM_PAUSE MAC_ASYM_PAUSE MLO_PAUSE_AN tx_pause/rx_pause
	 * ============= ============== ============ ==================
	 *             0              0            0 0/0
	 *             0              0            1 0/0
	 *             0              1            0 0/0, 0/1, 1/0, 1/1
	 *             0              1            1 0/0,      1/0
	 *             1              0            0 0/0,           1/1
	 *             1              0            1 0/0,           1/1
	 *             1              1            0 0/0, 0/1, 1/0, 1/1
	 *             1              1            1 0/0, 0/1,      1/1
	 *
	 * If you set MAC_ASYM_PAUSE, the user may request any combination of
	 * tx_pause and rx_pause. You do not have to support these
	 * combinations.
	 *
	 * However, you should support combinations of tx_pause and rx_pause
	 * which might be the result of autonegotation. For example, don't set
	 * MAC_SYM_PAUSE unless your device can support tx_pause and rx_pause
	 * at the same time.
	 */
	MAC_SYM_PAUSE	= BIT(0),
	MAC_ASYM_PAUSE	= BIT(1),
	MAC_10HD	= BIT(2),
	MAC_10FD	= BIT(3),
	MAC_10		= MAC_10HD | MAC_10FD,
	MAC_100HD	= BIT(4),
	MAC_100FD	= BIT(5),
	MAC_100		= MAC_100HD | MAC_100FD,
	MAC_1000HD	= BIT(6),
	MAC_1000FD	= BIT(7),
	MAC_1000	= MAC_1000HD | MAC_1000FD,
	MAC_2500FD	= BIT(8),
	MAC_5000FD	= BIT(9),
	MAC_10000FD	= BIT(10),
	MAC_20000FD	= BIT(11),
	MAC_25000FD	= BIT(12),
	MAC_40000FD	= BIT(13),
	MAC_50000FD	= BIT(14),
	MAC_56000FD	= BIT(15),
	MAC_100000FD	= BIT(16),
	MAC_200000FD	= BIT(17),
	MAC_400000FD	= BIT(18),
};

static inline void linkmode_zero(unsigned long *dst)
{
	bitmap_zero(dst, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline void linkmode_fill(unsigned long *dst)
{
	bitmap_fill(dst, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline void linkmode_copy(unsigned long *dst, const unsigned long *src)
{
	bitmap_copy(dst, src, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline void linkmode_and(unsigned long *dst, const unsigned long *a,
				const unsigned long *b)
{
	bitmap_and(dst, a, b, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline void linkmode_or(unsigned long *dst, const unsigned long *a,
				const unsigned long *b)
{
	bitmap_or(dst, a, b, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline bool linkmode_empty(const unsigned long *src)
{
	return bitmap_empty(src, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline bool linkmode_andnot(unsigned long *dst,
				   const unsigned long *src1,
				   const unsigned long *src2)
{
	return bitmap_andnot(dst, src1, src2,  __ETHTOOL_LINK_MODE_MASK_NBITS);
}

#define linkmode_test_bit	test_bit
#define linkmode_set_bit	__set_bit
#define linkmode_clear_bit	__clear_bit
#define linkmode_mod_bit	__assign_bit

static inline void linkmode_set_bit_array(const int *array, int array_size,
					  unsigned long *addr)
{
	int i;

	for (i = 0; i < array_size; i++)
		linkmode_set_bit(array[i], addr);
}

static inline int linkmode_equal(const unsigned long *src1,
				 const unsigned long *src2)
{
	return bitmap_equal(src1, src2, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline int linkmode_intersects(const unsigned long *src1,
				      const unsigned long *src2)
{
	return bitmap_intersects(src1, src2, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

static inline int linkmode_subset(const unsigned long *src1,
				  const unsigned long *src2)
{
	return bitmap_subset(src1, src2, __ETHTOOL_LINK_MODE_MASK_NBITS);
}

unsigned long linkmode_caps_max_speed(int speed);
int linkmode_interface_max_speed(phy_interface_t interface);

void linkmode_resolve_pause(const unsigned long *local_adv,
			    const unsigned long *partner_adv,
			    bool *tx_pause, bool *rx_pause);

void linkmode_set_pause(unsigned long *advertisement, bool tx, bool rx);

void linkmodes_from_caps(unsigned long *linkmodes, unsigned long caps);

unsigned long linkmode_cap_from_speed_duplex(int speed, unsigned int duplex);

unsigned long linkmode_interface_get_capabilities(phy_interface_t interface,
						  unsigned long mac_capabilities,
						  int rate_matching);

void linkmodes_from_interface(phy_interface_t interface, int rate_matching,
			      unsigned long *linkmodes);

#endif /* __LINKMODE_H */
