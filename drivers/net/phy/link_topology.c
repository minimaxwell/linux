// SPDX-License-Identifier: GPL-2.0+
/*
 * Infrastructure to handle all PHY devices connected to a given netdev,
 * either directly or indirectly attached.
 *
 * Copyright (c) 2023 Maxime Chevallier<maxime.chevallier@bootlin.com>
 */

#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/phy_port.h>
#include <linux/xarray.h>
#include <linux/link_topology.h>

struct phy_device *link_topo_get_phy(struct link_topology *lt, int phyindex)
{
	struct phy_device_node *pdn = xa_load(&lt->phys, phyindex);

	if (pdn)
		return pdn->phy;

	return NULL;
}
EXPORT_SYMBOL_GPL(link_topo_get_phy);

int link_topo_add_phy(struct link_topology *lt, struct phy_device *phy,
		      enum phy_upstream upt, void *upstream)
{
	struct phy_device_node *pdn;
	int ret;

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

	ret = xa_alloc_cyclic(&lt->phys, &phy->phyindex, pdn, xa_limit_32b,
			      &lt->next_phy_index, GFP_KERNEL);
	if (ret)
		goto err;

	return 0;

err:
	kfree(pdn);
	return ret;
}
EXPORT_SYMBOL_GPL(link_topo_add_phy);

void link_topo_del_phy(struct link_topology *lt, struct phy_device *phy)
{
	struct phy_device_node *pdn = xa_erase(&lt->phys, phy->phyindex);

	kfree(pdn);
}
EXPORT_SYMBOL_GPL(link_topo_del_phy);

void link_topo_init(struct link_topology *lt)
{
	xa_init_flags(&lt->phys, XA_FLAGS_ALLOC1);
	xa_init_flags(&lt->ports, XA_FLAGS_ALLOC1);
	lt->next_phy_index = 1;
	lt->next_port_index = 1;
}

struct phy_port *phy_port_create(const struct phy_port_config *cfg)
{
	switch (cfg->upstream_type) {
	case PHY_UPSTREAM_MAC:
		if (!cfg->netdev)
			return ERR_PTR(-EINVAL);
		break;
	case PHY_UPSTREAM_PHY:
		if (!cfg->phydev)
			return ERR_PTR(-EINVAL);
		break;
	case PHY_UPSTREAM_SFP:
		if (!cfg->sfp_bus)
			return ERR_PTR(-EINVAL);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	struct phy_port *port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return ERR_PTR(-ENOMEM);

	memcpy(&port->cfg, cfg, sizeof(*cfg));

	port->state = PHY_PORT_UNREGISTERED;

	return port;
}
EXPORT_SYMBOL_GPL(phy_port_create);

void phy_port_destroy(struct phy_port *port)
{
	kfree(port);
}
EXPORT_SYMBOL_GPL(phy_port_destroy);

struct phy_port *link_topo_get_port(struct link_topology *lt, int port_index)
{
	return xa_load(&lt->ports, port_index);
}
EXPORT_SYMBOL_GPL(link_topo_get_port);

void link_topo_del_port(struct phy_port *port)
{
	struct link_topology *lt = port->cfg.lt;

	pr_info("%s : Removed port %d\n", __func__, port->index);

	xa_erase(&lt->ports, port->index);
}
EXPORT_SYMBOL_GPL(link_topo_del_port);

int link_topo_add_port(struct link_topology *lt, struct phy_port *port)
{
	int ret;

	ret = xa_alloc_cyclic(&lt->ports, &port->index, port, xa_limit_32b,
			      &lt->next_port_index, GFP_KERNEL);
	if (ret)
		return ret;

	pr_info("%s : Inserted port %d\n", __func__, port->index);

	port->state = PHY_PORT_STANDBY;

	return 0;
}
EXPORT_SYMBOL_GPL(link_topo_add_port);

void phy_port_set_internal(struct phy_port *port, bool internal)
{
	port->cfg.internal = internal;
}

int phy_port_set_state(struct phy_port *port, enum phy_port_state state)
{
	if ((port->state == PHY_PORT_DISABLED ||
	    port->state == PHY_PORT_UNREGISTERED) && state == PHY_PORT_LINK_UP)
		return -EINVAL;

	port->state = state;

	return 0;
}
