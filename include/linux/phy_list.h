/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PHY device list allow maintaining a list of PHY devices that are
 * part of a netdevice's link topology. PHYs can for example be chained,
 * as is the case when using a PHY that exposes an SFP module, on which an
 * SFP transceiver that embeds a PHY is connected.
 *
 * This list can then be used by userspace to leverage individual PHY
 * capabilities.
 */
#ifndef __PHY_LIST_H
#define __PHY_LIST_H

#include <uapi/linux/ethtool.h>

struct xarray;
struct phy_device;
struct net_device;
struct dsa_port;
struct sfp_bus;

struct phy_device_node {
	enum phy_upstream upstream_type;

	union {
		struct net_device	*netdev;
		struct phy_device	*phydev;
	} upstream;

	struct sfp_bus *parent_sfp_bus;

	struct phy_device *phy;
};

struct phy_device_list {
	struct xarray xa;
	u32 next_index;
};

struct phy_device *phy_list_get_by_index(struct phy_device_list *phy_list,
					 int phyindex);
int phy_list_add(struct phy_device_list *phy_list, struct phy_device *phy,
		 enum phy_upstream upt, void *upstream);

void phy_list_del(struct phy_device_list *phy_list, struct phy_device *phy);
void phy_list_init(struct phy_device_list *phy_list);

#endif /* __PHY_LIST_H */
