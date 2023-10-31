// SPDX-License-Identifier: GPL-2.0+
/*
 * Infrastructure to handle all PHY devices connected to a given netdev,
 * either directly or indirectly attached.
 *
 * Copyright (c) 2023 Maxime Chevallier<maxime.chevallier@bootlin.com>
 */

#include <linux/phy.h>
#include <linux/phy_list.h>
#include <linux/xarray.h>

struct phy_device *phy_list_get_by_index(struct phy_device_list *phy_list,
					 int phyindex)
{
	struct phy_device_node *pdn = xa_load(&phy_list->xa, phyindex);

	if (pdn)
		return pdn->phy;

	return NULL;
}
EXPORT_SYMBOL_GPL(phy_list_get_by_index);

int phy_list_add(struct phy_device_list *phy_list, struct phy_device *phy,
		 enum phy_upstream upt, void *upstream)
{
	struct phy_device_node *pdn;
	int ret;

	pdn = kmalloc(sizeof(*pdn), GFP_KERNEL);
	if (!pdn)
		return -ENOMEM;

	pdn->phy = phy;
	switch (upt) {
	case PHY_UPSTREAM_MAC:
		pdn->upstream.netdev = (struct net_device *)upstream;
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

	ret = xa_alloc_cyclic(&phy_list->xa, &phy->phyindex, pdn, xa_limit_32b,
			    &phy_list->next_index, GFP_KERNEL);
	if (ret)
		goto err;

	return 0;

err:
	kfree(pdn);
	return ret;
}
EXPORT_SYMBOL_GPL(phy_list_add);

void phy_list_del(struct phy_device_list *phy_list, struct phy_device *phy)
{
	struct phy_device_node *pdn = xa_erase(&phy_list->xa, phy->phyindex);
	if (pdn)
		kfree(pdn);
}
EXPORT_SYMBOL_GPL(phy_list_del);

void phy_list_init(struct phy_device_list *phy_list)
{
	xa_init_flags(&phy_list->xa, XA_FLAGS_ALLOC1);
	phy_list->next_index = 1;
}
