// SPDX-License-Identifier: GPL-2.0+
/*
 * Infrastructure to handle all PHY devices connected to a given netdev,
 * either directly or indirectly attached.
 *
 * Copyright (c) 2023 Maxime Chevallier<maxime.chevallier@bootlin.com>
 */

#include <linux/phy_link_topology.h>
#include <linux/phy.h>
#include <linux/phy_port.h>
#include <linux/rtnetlink.h>
#include <linux/xarray.h>

static int netdev_alloc_phy_link_topology(struct net_device *dev)
{
	struct phy_link_topology *topo;

	topo = kzalloc(sizeof(*topo), GFP_KERNEL);
	if (!topo)
		return -ENOMEM;

	xa_init_flags(&topo->phys, XA_FLAGS_ALLOC1);
	topo->next_phy_index = 1;

	xa_init_flags(&topo->ports, XA_FLAGS_ALLOC1);
	topo->next_port_index = 1;

	dev->link_topo = topo;

	return 0;
}

int phy_link_topo_add_phy(struct net_device *dev,
			  struct phy_device *phy,
			  enum phy_upstream upt, void *upstream)
{
	struct phy_link_topology *topo = dev->link_topo;
	struct phy_device_node *pdn;
	int ret;

	ASSERT_RTNL();

	if (!topo) {
		ret = netdev_alloc_phy_link_topology(dev);
		if (ret)
			return ret;

		topo = dev->link_topo;
	}

	pdn = kzalloc(sizeof(*pdn), GFP_KERNEL);
	if (!pdn)
		return -ENOMEM;

	pdn->phy = phy;
	switch (upt) {
	case PHY_UPSTREAM_MAC:
		pdn->upstream.netdev = (struct net_device *)upstream;
		if (phy_on_sfp(phy))
			pdn->parent_sfp_bus = pdn->upstream.netdev->sfp_bus;
		break;
	case PHY_UPSTREAM_PHY:
		pdn->upstream.phydev = (struct phy_device *)upstream;
		if (phy_on_sfp(phy))
			pdn->parent_sfp_bus = pdn->upstream.phydev->sfp_bus;
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	pdn->upstream_type = upt;

	/* Attempt to re-use a previously allocated phy_index */
	if (phy->phyindex)
		ret = xa_insert(&topo->phys, phy->phyindex, pdn, GFP_KERNEL);
	else
		ret = xa_alloc_cyclic(&topo->phys, &phy->phyindex, pdn,
				      xa_limit_32b, &topo->next_phy_index,
				      GFP_KERNEL);

	if (ret)
		goto err;


	if (phy->phy_port) {
		ret = phy_link_topo_add_port(dev, phy->phy_port);
		if (ret)
			goto remove_phy;
	}

	return 0;
remove_phy:
	xa_erase(&topo->phys, phy->phyindex);
err:
	kfree(pdn);
	return ret;
}
EXPORT_SYMBOL_GPL(phy_link_topo_add_phy);

void phy_link_topo_del_phy(struct net_device *dev,
			   struct phy_device *phy)
{
	struct phy_link_topology *topo = dev->link_topo;
	struct phy_device_node *pdn;

	if (!topo)
		return;

	pdn = xa_erase(&topo->phys, phy->phyindex);

	if (phy->phy_port)
		phy_link_topo_del_port(dev, phy->phy_port);

	/* We delete the PHY from the topology, however we don't re-set the
	 * phy->phyindex field. If the PHY isn't gone, we can re-assign it the
	 * same index next time it's added back to the topology
	 */

	kfree(pdn);
}
EXPORT_SYMBOL_GPL(phy_link_topo_del_phy);

int phy_link_topo_add_port(struct net_device *dev,
			   struct phy_port *port)
{
	struct phy_link_topology *topo = dev->link_topo;
	int ret;

	ASSERT_RTNL();

	if (!topo) {
		ret = netdev_alloc_phy_link_topology(dev);
		if (ret)
			return ret;

		topo = dev->link_topo;
	}

	if (port->id)
		ret = xa_insert(&topo->ports, port->id, port, GFP_KERNEL);
	else
		ret = xa_alloc_cyclic(&topo->ports, &port->id, port,
				      xa_limit_32b, &topo->next_port_index,
				      GFP_KERNEL);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_link_topo_add_port);

void phy_link_topo_del_port(struct net_device *dev,
			    struct phy_port *port)
{
	struct phy_link_topology *topo = dev->link_topo;

	if (!topo)
		return;

	xa_erase(&topo->ports, port->id);
}
EXPORT_SYMBOL_GPL(phy_link_topo_del_port);
