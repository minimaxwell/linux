// SPDX-License-Identifier: GPL-2.0+
/*
 * Infrastructure to handle all PHY devices connected to a given netdev,
 * either directly or indirectly attached.
 *
 * Copyright (c) 2023 Maxime Chevallier<maxime.chevallier@bootlin.com>
 */

#include <linux/ethtool_netlink.h>
#include <linux/list.h>
#include <linux/phy_link_topology.h>
#include <linux/phy.h>
#include <linux/phy_port.h>
#include <linux/rtnetlink.h>
#include <linux/xarray.h>

static void phy_link_topo_port_sm(struct work_struct *work);

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
	topo->dev = dev;
	mutex_init(&topo->lock);
	INIT_DELAYED_WORK(&topo->state_queue, phy_link_topo_port_sm);

	return 0;
}

static int phy_link_topo_add_all_ports(struct net_device *dev,
				       struct phy_device *phy)
{
	struct phy_port *port;
	int ret;

	list_for_each_entry(port, &phy->ports, head) {
		ret = phy_link_topo_add_port(dev, port);
		if (ret)
			goto cleanup;
	}

	return 0;

cleanup:
	list_for_each_entry(port, &phy->ports, head)
		phy_link_topo_del_port(dev, port);

	return ret;
}

int phy_link_topo_add_phy(struct net_device *dev,
			  struct phy_device *phy,
			  enum phy_upstream upt, void *upstream)
{
	struct phy_link_topology *topo = dev->link_topo;
	struct phy_device_node *pdn;
	int ret;

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

	if (ret < 0)
		goto err;

	ret = phy_link_topo_add_all_ports(dev, phy);
	if (ret)
		goto err_remove;

	return 0;

err_remove:
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

	/* We delete the PHY from the topology, however we don't re-set the
	 * phy->phyindex field. If the PHY isn't gone, we can re-assign it the
	 * same index next time it's added back to the topology
	 */

	kfree(pdn);
}
EXPORT_SYMBOL_GPL(phy_link_topo_del_phy);

int phy_link_topo_add_port(struct net_device *dev, struct phy_port *port)
{
	struct phy_link_topology *topo = dev->link_topo;
	int ret;

	if (!topo) {
		ret = netdev_alloc_phy_link_topology(dev);
		if (ret)
			return ret;

		topo = dev->link_topo;
	}

	if (port->port_index)
		ret = xa_insert(&topo->ports, port->port_index, port, GFP_KERNEL);
	else
		ret = xa_alloc_cyclic(&topo->ports, &port->port_index, port,
				      xa_limit_32b, &topo->next_port_index,
				      GFP_KERNEL);

	port->topo = topo;

	return ret;
}
EXPORT_SYMBOL_GPL(phy_link_topo_add_port);

void phy_link_topo_del_port(struct net_device *dev, struct phy_port *port)
{
	struct phy_link_topology *topo = dev->link_topo;

	if (!topo || !port->port_index)
		return;

	xa_erase(&topo->ports, port->port_index);

	port->topo = NULL;
}
EXPORT_SYMBOL_GPL(phy_link_topo_del_port);

static void phy_link_topo_port_sm(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct phy_link_topology *topo = container_of(dwork,
						      struct phy_link_topology,
						      state_queue);
	unsigned long port_index;
	struct phy_port *p;

	mutex_lock(&topo->lock);
	switch (topo->state) {
	case PORT_SM_LISTENING:
		xa_for_each(&topo->ports, port_index, p) {
			if (!p->enabled)
				continue;

			if (p->link) {
				p->active = true;
				topo->state = PORT_SM_ESTABLISHED;
				topo->active_port = p;
				ethnl_port_notify(topo->dev, p);
				goto out;
			}
		}
		break;
	case PORT_SM_ESTABLISHED:
		/* If the active port is still has link, nothing to do */
		if (topo->active_port->link && topo->active_port->enabled)
			break;

		/* Active port has lost link, notify that */
		topo->active_port->active = false;

		ethnl_port_notify(topo->dev, topo->active_port);

		/* Let's see if other ports have link */
		xa_for_each(&topo->ports, port_index, p) {
			if (p->enabled && p->link) {
				p->active = true;
				topo->active_port = p;
				ethnl_port_notify(topo->dev, p);
				goto out;
			}
		}

		/* No enabled port has link */
		topo->active_port = NULL;
		topo->state = PORT_SM_LISTENING;
	}
out:
	mutex_unlock(&topo->lock);
}

/**
 * phy_port_state_change() - Notify that a port has changed state
 * @port: The port whose state changed
 *
 * This helper must be called by the port driver to notify status changes.
 */
void phy_link_topo_update(struct phy_link_topology *topo)
{
	queue_delayed_work(system_power_efficient_wq, &topo->state_queue, 0);
}
EXPORT_SYMBOL_GPL(phy_link_topo_update);
