// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 Bootlin

#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/phy_link_topology.h>
#include <linux/platform_device.h>
#include <linux/swphy.h>

#include "netdevsim.h"

static atomic_t bus_num = ATOMIC_INIT(0);

struct nsim_mdiobus {
	struct platform_device *pdev;
	struct mii_bus *mii;

	struct fixed_phy_status state;
};

struct nsim_phy_device {
	struct phy_device *phy;
	struct fixed_phy_status *state;

	struct dentry *phy_dir;

	/* Used by nsim_dev_port to keep track of its PHYs */
	struct list_head node;
};


static int nsim_mdio_read(struct mii_bus *bus, int phy_addr, int reg_num)
{
	struct nsim_mdiobus *mb = bus->priv;

	pr_info("%s : speed is %d\n", __func__, mb->state.speed);

	return swphy_read_reg(reg_num, &mb->state);
}

static int nsim_mdio_write(struct mii_bus *bus, int phy_addr, int reg_num,
			    u16 val)
{
	return 0;
}

static struct nsim_mdiobus *nsim_mdiobus_create(struct fixed_phy_status *default_state)
{
	struct nsim_mdiobus *mb;

	mb = kzalloc(sizeof(*mb), GFP_KERNEL);
	if (!mb)
		return NULL;

	mb->pdev = platform_device_register_simple("nsim MDIO bus", atomic_read(&bus_num), NULL, 0);
	if (IS_ERR(mb->pdev))
		goto free_mb;

	mb->mii = mdiobus_alloc();
	if (!mb->mii)
		goto free_pdev;

	memcpy(&mb->state, default_state, sizeof(*default_state));

	snprintf(mb->mii->id, MII_BUS_ID_SIZE, "nsim-%d", atomic_read(&bus_num));
	atomic_inc(&bus_num);
	mb->mii->name = "nsim MDIO Bus";
	mb->mii->priv = mb;
	mb->mii->parent = &mb->pdev->dev;
	mb->mii->read = &nsim_mdio_read;
	mb->mii->write = &nsim_mdio_write;
	mb->mii->phy_mask = ~0;

	if (mdiobus_register(mb->mii))
		goto free_mdiobus;

	return mb;

free_mdiobus:
	atomic_dec(&bus_num);
	mdiobus_free(mb->mii);
free_pdev:
	platform_device_unregister(mb->pdev);
free_mb:
	kfree(mb);

	return NULL;
}

static void nsim_mdiobus_destroy(struct nsim_mdiobus *mb)
{
	mdiobus_unregister(mb->mii);
	mdiobus_free(mb->mii);
	atomic_dec(&bus_num);
	platform_device_unregister(mb->pdev);
	kfree(mb);
}

static struct nsim_phy_device *
nsim_phy_register(struct fixed_phy_status *default_state)
{
	struct nsim_phy_device *ns_phy;
	struct nsim_mdiobus *mb;
	int err;

	mb = nsim_mdiobus_create(default_state);
	if (IS_ERR(mb))
		return ERR_CAST(mb);

	ns_phy = kzalloc(sizeof(*ns_phy), GFP_KERNEL);
	if (!ns_phy) {
		err = -ENOMEM;
		goto out_mdio;
	}

	ns_phy->state = &mb->state;

	ns_phy->phy = get_phy_device(mb->mii, 0, false);
	if (IS_ERR(ns_phy->phy)) {
		err = PTR_ERR(ns_phy->phy);
		goto out_phy_free;
	}

	/* propagate the fixed link values to struct phy_device */
	ns_phy->phy->link = default_state->link;
	if (default_state->link) {
		ns_phy->phy->speed = default_state->speed;
		ns_phy->phy->duplex = default_state->duplex;
		ns_phy->phy->pause = default_state->pause;
		ns_phy->phy->asym_pause = default_state->asym_pause;
	}

	ns_phy->phy->is_pseudo_fixed_link = true;

	switch (default_state->speed) {
	case SPEED_1000:
		linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
				 ns_phy->phy->supported);
		linkmode_set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				 ns_phy->phy->supported);
		fallthrough;
	case SPEED_100:
		linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
				 ns_phy->phy->supported);
		linkmode_set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
				 ns_phy->phy->supported);
		fallthrough;
	case SPEED_10:
	default:
		linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
				 ns_phy->phy->supported);
		linkmode_set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
				 ns_phy->phy->supported);
	}

	phy_advertise_supported(ns_phy->phy);

	err = phy_device_register(ns_phy->phy);
	if (err)
		goto out_phy;

	return ns_phy;

out_phy:
	phy_device_free(ns_phy->phy);
out_phy_free:
	kfree(ns_phy);
out_mdio:
	nsim_mdiobus_destroy(mb);
	return ERR_PTR(err);
}

/* TODO */
static void nsim_phy_destroy(struct phy_device *phydev)
{
	struct mii_bus *mii = phydev->mdio.bus;
	struct nsim_mdiobus *mb = mii->priv;
	phy_device_remove(phydev);
	nsim_mdiobus_destroy(mb);
}

static int nsim_phy_state_speed_set(void *data, u64 val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;
	struct fixed_phy_status *state = ns_phy->state;

	switch (val) {
	case 10:
		state->speed = SPEED_10;
		break;
	case 100:
		state->speed = SPEED_100;
		break;
	case 1000:
		state->speed = SPEED_1000;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int nsim_phy_state_speed_get(void *data, u64 *val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	*val = ns_phy->state->speed;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(nsim_phy_state_speed_fops, nsim_phy_state_speed_get,
			 nsim_phy_state_speed_set, "%llu\n");

static int nsim_phy_state_link_set(void *data, u64 val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	ns_phy->state->link = !!val;

	return 0;
}

static int nsim_phy_state_link_get(void *data, u64 *val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	*val = ns_phy->state->link;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(nsim_phy_state_link_fops, nsim_phy_state_link_get,
			 nsim_phy_state_link_set, "%llu\n");

static int nsim_phy_state_duplex_set(void *data, u64 val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	ns_phy->state->duplex = !!val;

	return 0;
}

static int nsim_phy_state_duplex_get(void *data, u64 *val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	*val = ns_phy->state->duplex;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(nsim_phy_state_duplex_fops, nsim_phy_state_duplex_get,
			 nsim_phy_state_duplex_set, "%llu\n");

static int nsim_phy_state_pause_set(void *data, u64 val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	ns_phy->state->pause = !!val;

	return 0;
}

static int nsim_phy_state_pause_get(void *data, u64 *val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	*val = ns_phy->state->pause;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(nsim_phy_state_pause_fops, nsim_phy_state_pause_get,
			 nsim_phy_state_pause_set, "%llu\n");

static int nsim_phy_state_asym_pause_set(void *data, u64 val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	ns_phy->state->asym_pause = !!val;

	return 0;
}

static int nsim_phy_state_asym_pause_get(void *data, u64 *val)
{
	struct nsim_phy_device *ns_phy = (struct nsim_phy_device *)data;

	*val = ns_phy->state->asym_pause;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(nsim_phy_state_asym_pause_fops, nsim_phy_state_asym_pause_get,
			 nsim_phy_state_asym_pause_set, "%llu\n");

static void nsim_phy_debugfs_create(struct nsim_dev_port *port,
				    struct nsim_phy_device *ns_phy)
{
	/* create debugfs stuff */
	ns_phy->phy_dir = debugfs_create_dir("phy0", port->ddir);
	/* Speed through manual fops for validation */

	debugfs_create_file("speed", 0600, ns_phy->phy_dir, ns_phy, &nsim_phy_state_speed_fops);
	debugfs_create_file("link", 0600, ns_phy->phy_dir, ns_phy, &nsim_phy_state_link_fops);
	debugfs_create_file("full_duplex", 0600, ns_phy->phy_dir, ns_phy, &nsim_phy_state_duplex_fops);
	debugfs_create_file("pause", 0600, ns_phy->phy_dir, ns_phy, &nsim_phy_state_pause_fops);
	debugfs_create_file("asym_pause", 0600, ns_phy->phy_dir, ns_phy, &nsim_phy_state_asym_pause_fops);

}

static ssize_t
nsim_phy_add_write(struct file *file, const char __user *data,
				  size_t count, loff_t *ppos)
{
	struct net_device *dev = file->private_data;
	struct netdevsim *ns = netdev_priv(dev);
	struct fixed_phy_status phy_status = {
		.link = 1,
		.speed = SPEED_1000,
		.duplex = DUPLEX_FULL,
	};
	struct nsim_phy_device *ns_phy;
	struct phy_device *parent;
	u32 parent_id;
	char buf[10];
	ssize_t ret;
	int err;

	if (*ppos != 0)
		return 0;

	if (count >= sizeof(buf))
		return -ENOSPC;

	ret = copy_from_user(buf, data, count);
	if (ret)
		return -EFAULT;
	buf[count] = '\0';

	ret = kstrtouint(buf, 10, &parent_id);
	if (ret)
		return -EINVAL;

	ns_phy = nsim_phy_register(&phy_status);
	if (IS_ERR(ns_phy))
		return PTR_ERR(ns_phy);

	nsim_phy_debugfs_create(ns->nsim_dev_port, ns_phy);

	err = phy_attach_direct(dev, ns_phy->phy, 0, PHY_INTERFACE_MODE_NA);
	if (err)
		return err;

	phy_attached_info(ns_phy->phy);

	return count;
}

static const struct file_operations nsim_phy_add_fops = {
	.open = simple_open,
	.write = nsim_phy_add_write,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

static ssize_t
nsim_phy_del_write(struct file *file, const char __user *data,
				  size_t count, loff_t *ppos)
{
	struct net_device *dev = file->private_data;
	struct netdevsim *ns = netdev_priv(dev);
	struct phy_device *phydev;
	u32 phy_index;
	char buf[10];
	ssize_t ret;

	if (*ppos != 0)
		return 0;

	if (count >= sizeof(buf))
		return -ENOSPC;

	ret = copy_from_user(buf, data, count);
	if (ret)
		return -EFAULT;
	buf[count] = '\0';

	ret = kstrtouint(buf, 10, &phy_index);
	if (ret)
		return -EINVAL;

	phydev = phy_link_topo_get_phy(dev->link_topo, phy_index);
	if (!phydev)
		return -ENODEV;

	/* TODO */
	/*
	 * - Detach PHY if it's attached
	 * - Remove debugfs dir -> need to keep a ref to it
	 */

	phy_link_topo_del_phy(dev->link_topo, phydev);

	return count;
}

static const struct file_operations nsim_phy_del_fops = {
	.open = simple_open,
	.write = nsim_phy_del_write,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

void nsim_phy_init(struct netdevsim *ns)
{
	debugfs_create_file("phy_add", 0200, ns->nsim_dev_port->ddir,
			    ns->netdev, &nsim_phy_add_fops);

	debugfs_create_file("phy_del", 0200, ns->nsim_dev_port->ddir,
			    ns->netdev, &nsim_phy_del_fops);
}

void nsim_phy_teardown(struct netdevsim *ns)
{
	debugfs_remove_recursive(ns->nsim_dev_port->ddir);
}
