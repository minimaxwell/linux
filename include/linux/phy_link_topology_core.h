/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PHY_LINK_TOPOLOGY_CORE_H
#define __PHY_LINK_TOPOLOGY_CORE_H

struct phy_link_topology;

struct phy_link_topology *phy_link_topo_create(struct net_device *dev);
void phy_link_topo_destroy(struct phy_link_topology *topo);

#endif /* __PHY_LINK_TOPOLOGY_CORE_H */
