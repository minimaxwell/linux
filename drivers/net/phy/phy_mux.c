// SPDX-License-Identifier: GPL-2.0+
/*
 * Ethernet PHY generic multiplexing logic
 *
 * Copyright (c) 2024 Maxime Chevallier <maxime.chevallier@bootlin.com>
 */

#include <linux/device.h>
#include <linux/inetdevice.h>
#include <linux/fwnode.h>
#include <linux/if_vlan.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/phy.h>
#include <linux/phy_mux.h>
#include <linux/phy_port.h>
#include <linux/phy_link_topology.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/xarray.h>
#include <net/arp.h>

#define MUX_DEADLINE_MS (1500)
#define MUX_RR_DEADLINE_MS (2000)

static LIST_HEAD(phy_muxes);
static DEFINE_MUTEX(phy_mux_mutex);

enum phy_mux_state {
	PHY_MUX_INIT,
	PHY_MUX_ATTACHED,
	PHY_MUX_STARTING,
	PHY_MUX_ACTIVE_LISTENING,
	PHY_MUX_ESTABLISHING,
	/* For highest-speed selection strategy */
	PHY_MUX_ESTABLISHED_LISTENING,
	PHY_MUX_ESTABLISHED,
	PHY_MUX_FIXED,
};

struct phy_mux {
	struct list_head node;
	struct phy_mux_config cfg;
	struct xarray ports;
	struct device *dev;
	struct net_device *netdev;
	struct phy_mux_port *current_port;
	const struct phy_mux_parent_ops *pops;
	void *mux_priv;
	void *parent_priv;
	struct delayed_work state_queue;
	int n_ports;
	unsigned long max_index;
	bool exclusive;

	enum phy_mux_state state;
};

int phy_mux_attach(struct phy_mux *mux, struct net_device *dev,
		   struct phy_mux_parent_ops *pops, void *priv)
{
	struct phy_mux_port *port;
	unsigned long index;
	int ret;

	if (mux->pops)
		return 0;

	mux->pops = pops;
	mux->parent_priv = priv;
	mux->netdev = dev;

	xa_for_each(&mux->ports, index, port) {
		if (port->type == PHY_MUX_PORT_PHY) {
			ret = phy_link_topo_add_phy(dev, port->phydev,
						    PHY_UPSTREAM_MAC, dev);
			if (ret)
				return ret;
		}
	}

	mux->state = PHY_MUX_ATTACHED;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_attach);

void phy_mux_detach(struct phy_mux *mux)
{
	if (!mux)
		return;

	mux->pops = NULL;
	mux->parent_priv = NULL;
}
EXPORT_SYMBOL_GPL(phy_mux_detach);

void phy_mux_destroy(struct phy_mux *mux)
{
	xa_destroy(&mux->ports);
}
EXPORT_SYMBOL_GPL(phy_mux_destroy);

void *phy_mux_priv(struct phy_mux *mux)
{
	return mux->mux_priv;
}
EXPORT_SYMBOL_GPL(phy_mux_priv);

int phy_mux_register_port(struct phy_mux *mux,
			  struct phy_mux_port *port)
{
	int ret;

	port->mux = mux;

	if ((mux->cfg.max_n_ports != PHY_MUX_NO_LIMIT) &&
	     mux->n_ports == mux->cfg.max_n_ports)
		return -ENOSPC;


	ret = xa_alloc(&mux->ports, &port->id, port, xa_limit_16b, GFP_KERNEL);
	if (ret)
		return ret;

	mux->n_ports++;
	if (port->id > mux->max_index)
		mux->max_index = port->id;

	/* New ports start enabled  */
	port->forced = false;
	port->enabled = true;

	if (port->exclusive)
		mux->exclusive = true;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_register_port);

static int phy_mux_port_send_arps(struct phy_mux_port *port)
{
	struct net_device *port_dev = port->mux->netdev;
	struct net_device *upper;
	struct list_head *iter;
	struct sk_buff *skb;
	__be32 ip4_addr;
	int ret = 0;

	ip4_addr = inet_select_addr(port_dev, 0, 0);

	skb = arp_create(ARPOP_REPLY, ETH_P_ARP, ip4_addr, port_dev, ip4_addr,
			 NULL, port_dev->dev_addr, NULL);
	if (!skb)
		return -EINVAL;

	arp_xmit(skb);

	rcu_read_lock();
	netdev_for_each_upper_dev_rcu(port_dev, upper, iter) {
		if (is_vlan_dev(upper)) {
			ip4_addr = inet_select_addr(upper, 0, 0);
			skb = arp_create(ARPOP_REPLY, ETH_P_ARP, ip4_addr, upper, ip4_addr,
				 NULL, upper->dev_addr, NULL);
			if (!skb) {
				ret = -EINVAL;
				goto out;
			}

			arp_xmit(skb);
		}
	}

out:
	rcu_read_unlock();

	return ret;
}

/* Set the port into listnening mode, if possible */
static int phy_mux_port_start_listening(struct phy_mux_port *port)
{
	int ret = 0;

	if (port->ops->start_listening)
		ret = port->ops->start_listening(port);

	if (!ret)
		port->listening = true;

	return ret;
}

/* Stop listening */
static int phy_mux_port_stop_listening(struct phy_mux_port *port)
{
	int ret;

	if (!port->listening)
		return 0;

	if (port->ops->stop_listening)
		ret = port->ops->stop_listening(port);

	port->listening = false;

	return ret;
}

static int phy_mux_port_select(struct phy_mux_port *port)
{
	if (!port->ops->select)
		return 0;

	return port->ops->select(port);
}

static int phy_mux_port_deselect(struct phy_mux_port *port)
{
	if (!port->ops->deselect)
		return 0;

	return port->ops->deselect(port);
}

/* MUX -> PORT -> PARENT */
static int __phy_mux_deselect_port(struct phy_mux *mux,
				   struct phy_mux_port *port)
{
	const struct phy_mux_parent_ops *pops = mux->pops;
	const struct phy_mux_ops *ops = mux->cfg.ops;
	int ret;

	if (!port->selected)
		return 0;

	/* Disable the port in the mux */
	if (ops->deselect) {
		ret = ops->deselect(mux, port);
		if (ret)
			return ret;
	}

	ret = phy_mux_port_deselect(port);
	if (ret)
		return ret;

	/* Notify the parent that the port was deselected */
	if (pops->deselect) {
		ret = pops->deselect(mux, port, mux->parent_priv);
		if (ret)
			return ret;
	}

	port->selected = false;

	if (mux->state == PHY_MUX_ACTIVE_LISTENING ||
	    mux->state == PHY_MUX_ESTABLISHING ||
	    mux->state == PHY_MUX_ESTABLISHED_LISTENING ||
	    mux->state == PHY_MUX_ESTABLISHED)
		return phy_mux_port_start_listening(port);

	return 0;
}

/* MUX -> PARENT -> PORT */
static int __phy_mux_select_port(struct phy_mux *mux,
				 struct phy_mux_port *port)
{
	const struct phy_mux_parent_ops *pops = mux->pops;
	const struct phy_mux_ops *ops = mux->cfg.ops;
	int ret;

	phy_mux_port_stop_listening(port);

	/* Enable the new port */
	if (ops->select) {
		ret = ops->select(mux, port);
		if (ret)
			return ret;
	}

	if (pops->select) {
		ret = pops->select(mux, port, mux->parent_priv);
		if (ret)
			return ret;
	}

	ret = phy_mux_port_select(port);
	if (ret)
		return ret;

	port->selected = true;

	return 0;
}

static int __phy_mux_select(struct phy_mux *mux, struct phy_mux_port *port)
{
	int ret;

	if (mux->current_port) {
		ret = __phy_mux_deselect_port(mux, mux->current_port);
		if (ret)
			return ret;
	}

	ret = __phy_mux_select_port(mux, port);
	if (ret)
		return ret;

	dev_info(mux->dev, "switching to port %d\n", port->id);

	mux->current_port = port;

	return 0;
}

static int phy_port_select_unisolate(struct phy_mux_port *port)
{
	if (port->type != PHY_MUX_PORT_PHY)
		return -EINVAL;

	return phy_isolate(port->phydev, false);
}

static int phy_port_deselect_isolate(struct phy_mux_port *port)
{
	if (port->type != PHY_MUX_PORT_PHY)
		return -EINVAL;

	return phy_isolate(port->phydev, true);
}

static int phy_port_generic_start_listening(struct phy_mux_port *port)
{
	struct phy_device *phy;

	if (port->type != PHY_MUX_PORT_PHY)
		return -EINVAL;

	phy = port->phydev;

	port->phydev->phy_link_change = phy_mux_notify;
	phy->state = PHY_READY;

	phy->interrupts = PHY_INTERRUPT_DISABLED;

	if (phy->dev_flags & PHY_F_NO_IRQ)
		phy->irq = PHY_POLL;

	if (!(phy->drv && phy->drv->config_intr && phy->drv->handle_interrupt) &&
	    phy_interrupt_is_valid(phy))
		phy->irq = PHY_POLL;

	phy_init_hw(phy);
	phy_request_interrupt(phy);
	phy_start(port->phydev);

	return 0;
}

static int phy_port_generic_stop_listening(struct phy_mux_port *port)
{
	if (port->type != PHY_MUX_PORT_PHY)
		return -EINVAL;

	if (phy_is_started(port->phydev)) {
		phy_stop(port->phydev);

		if (phy_interrupt_is_valid(port->phydev))
			phy_free_interrupt(port->phydev);
	}

	return 0;
}

static const struct phy_mux_port_ops phy_isolate_ops = {
	.select = phy_port_select_unisolate,
	.deselect = phy_port_deselect_isolate,

	.start_listening = phy_port_generic_start_listening,
	.stop_listening = phy_port_generic_stop_listening,
};

static int phy_port_deselect_stop(struct phy_mux_port *port)
{
	struct phy_device *phy;

	if (port->type != PHY_MUX_PORT_PHY)
		return -EINVAL;

	phy = port->phydev;

	return genphy_suspend(phy);
}

static const struct phy_mux_port_ops phy_power_ops = {
	.deselect = phy_port_deselect_stop,
};

static void phy_mux_queue_state_machine(struct phy_mux *mux,
					unsigned long when)
{
	queue_delayed_work(system_power_efficient_wq, &mux->state_queue, when);
}

static void phy_mux_trigger(struct phy_mux *mux)
{
	phy_mux_queue_state_machine(mux, 0);
}

int mux_get_state(struct phy_port *port, struct phy_port_state *state)
{
	struct phy_mux_port *mux_port = port->mux_port;
	ASSERT_RTNL();

	state->enabled = mux_port->enabled;
	state->forced = mux_port->forced;

	return 0;
}

int mux_set_state(struct phy_port *phy_port, const struct phy_port_state *state)
{
	struct phy_mux_port *port = phy_port->mux_port;
	struct phy_mux *mux = port->mux;
	struct phy_mux_port *it;
	unsigned long index;
	bool force_disable = false;
	bool force_enable = false;

	ASSERT_RTNL();

	if (state->forced && !state->enabled)
		return -EINVAL;

	/* We can't do anything on a port that is disabled and forced.
	 * TODO : Clear all forced status if the force-enable port is
	 * deregistered
	 */
	if (port->forced && !port->enabled)
		return 0;

	/* If we are no longer forcing the enabled port, release all ports
	 * from their forced state
	 */
	if (port->forced && !state->forced && port->enabled)
		force_disable = true;
	if (!port->forced && state->forced)
		force_enable = true;

	port->enabled = state->enabled;
	port->forced = state->forced;

	if (port->forced) {
		xa_for_each(&mux->ports, index, it) {
			if (it == port)
				continue;

			it->forced = true;
			it->enabled = false;
		}
	}

	if (force_disable) {
		xa_for_each(&mux->ports, index, it) {
			if (it == port)
				continue;

			it->forced = false;
		}
	}

	if (force_disable)
		mux->state = PHY_MUX_ACTIVE_LISTENING;
	if (force_enable)
		mux->state = PHY_MUX_FIXED;

	phy_mux_trigger(mux);

	return 0;
}

static const struct phy_port_ops phy_on_mux_ops = {
	.get_state = mux_get_state,
	.set_state = mux_set_state,
};

struct phy_mux_port *phy_mux_port_create_from_phy(struct phy_device *phy)
{
	struct phy_mux_port *port;

	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return NULL;

	port->type = PHY_MUX_PORT_PHY;
	port->phydev = phy;

	if (phy_can_isolate(phy))
		port->ops = &phy_isolate_ops;
	else
		port->ops = &phy_power_ops;

	/* Need a mechanisme to cope with the fact that, at this point, the phydev
	 * might not have it's driver bound yet. Maybe re-do that check upon
	 * mux_attach
	 */
	port->exclusive = !phy_can_isolate(phy);

	phy->mux_port = port;

	if (phy->phy_port) {
		phy->phy_port->mux_port = port;
		/* TODO - FIXME C'est pas bon ça, il faut un phy_get_state, qui
		 * va lui aller forwarder vers le mux.
		 */
		//phy->phy_port->ops = &phy_on_mux_ops;
	}

	return port;
}
EXPORT_SYMBOL_GPL(phy_mux_port_create_from_phy);

void phy_mux_port_destroy(struct phy_mux_port *port)
{
	kfree(port);
}
EXPORT_SYMBOL_GPL(phy_mux_port_destroy);

struct phy_mux *fwnode_phy_mux_get(const struct fwnode_handle *fwnode)
{
	struct phy_mux *mux;

	mutex_lock(&phy_mux_mutex);
	list_for_each_entry(mux, &phy_muxes, node) {
		if (dev_fwnode(mux->dev) == fwnode) {
			mutex_unlock(&phy_mux_mutex);
			return mux;
		}
	}
	mutex_unlock(&phy_mux_mutex);

	return NULL;

}
EXPORT_SYMBOL_GPL(fwnode_phy_mux_get);

static void phy_mux_select_next_port(struct phy_mux *mux)
{
	struct phy_mux_port *next_port;
	unsigned long id = 0;

	if (mux->current_port)
		id = mux->current_port->id;

	next_port = xa_find_after(&mux->ports, &id, mux->max_index, XA_PRESENT);
	if (!next_port) {
		/* Wrap around to the beginning of the ports array */
		id = 0;
		next_port = xa_find_after(&mux->ports, &id, mux->max_index,
					  XA_PRESENT);
	} else

	if (mux->current_port)
		mux->current_port->establishing = false;

	if (mux->current_port != next_port)
		__phy_mux_select(mux, next_port);

	next_port->establishing = true;
	next_port->establishing_deadline = jiffies +
					   msecs_to_jiffies(MUX_RR_DEADLINE_MS);
}

/* When ports are attached to the mux, we might not be able yet to tell if the
 * port has proper isolation or not.
 *
 * This function walks through all ports to check that, and can be called after
 * all ports are registered.
 */
static void phy_mux_check_exclusive(struct phy_mux *mux)
{
	struct phy_mux_port *port;
	unsigned long index;

	xa_for_each(&mux->ports, index, port) {
		struct phy_device *phydev;

		if (port->type != PHY_MUX_PORT_PHY)
			continue;

		phydev = port->phydev;

		port->exclusive = !phy_can_isolate(phydev);

		if (port->exclusive)
			mux->exclusive = true;
	}
}

static void phy_mux_state_machine(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct phy_mux *mux =
			container_of(dwork, struct phy_mux, state_queue);
	struct phy_mux_port *port;
	unsigned long index;

	rtnl_lock();
	switch (mux->state) {
	case PHY_MUX_INIT:
		break;
	case PHY_MUX_ATTACHED:
		break;
	case PHY_MUX_STARTING:
		phy_mux_check_exclusive(mux);

		if (mux->exclusive) {
			xa_for_each(&mux->ports, index, port)
				phy_mux_port_deselect(port);

			mux->state = PHY_MUX_ESTABLISHING;
			phy_mux_select_next_port(mux);
		} else {
			mux->state = PHY_MUX_ACTIVE_LISTENING;
			xa_for_each(&mux->ports, index, port) {
				phy_mux_port_deselect(port);
				phy_mux_port_start_listening(port);
			}
		}

		if (mux->exclusive)
			phy_mux_queue_state_machine(mux, msecs_to_jiffies(MUX_RR_DEADLINE_MS));
		else
			phy_mux_trigger(mux);
		break;
	case PHY_MUX_ACTIVE_LISTENING:
		if (mux->exclusive) {
			mux->state = PHY_MUX_ESTABLISHING;
			phy_mux_trigger(mux);
			goto out;
		}

		xa_for_each(&mux->ports, index, port) {
			if (!port->link)
				continue;

			if (!port->enabled)
				continue;

			if (__phy_mux_select(mux, port)) {
				pr_err("%s : Error switching ports\n", __func__);
				break;
			}

			mux->state = PHY_MUX_ESTABLISHING;

			port->establishing = true;
			port->establishing_deadline = jiffies + msecs_to_jiffies(MUX_DEADLINE_MS);
			mux->current_port = port;

			/* TODO Schedule event after deadline */
			phy_mux_queue_state_machine(mux, msecs_to_jiffies(MUX_DEADLINE_MS) + 1);

			goto out;
		}
		break;
	case PHY_MUX_ESTABLISHING:
		port = mux->current_port;
		if (!port->enabled) {
			port->establishing = false;
			mux->state = PHY_MUX_ACTIVE_LISTENING;
		}

		if (port->establishing_deadline > jiffies) {
			phy_mux_queue_state_machine(mux, port->establishing_deadline - jiffies + 1);
			goto out;
		}

		if (!port->link) {
			if (mux->exclusive) {
				phy_mux_select_next_port(mux);
				phy_mux_trigger(mux);
				goto out;
			} else {
				port->establishing = false;
				mux->state = PHY_MUX_ACTIVE_LISTENING;
				goto out;

			}
		}

		port->establishing = false;
		mux->state = PHY_MUX_ESTABLISHED;

		phy_mux_port_send_arps(port);

		goto out;
	case PHY_MUX_ESTABLISHED_LISTENING:
		if (mux->current_port->link) {
			xa_for_each(&mux->ports, index, port) {
				if (!port->link || (port->speed <= mux->current_port->speed))
					continue;

				if (!port->enabled)
					continue;

				if (__phy_mux_select(mux, port)) {
					pr_err("%s : Error switching ports\n", __func__);
					break;
				}
				mux->state = PHY_MUX_ESTABLISHING;
				phy_mux_queue_state_machine(mux, msecs_to_jiffies(MUX_DEADLINE_MS) + 1);
			}
		}
		fallthrough;
	case PHY_MUX_ESTABLISHED:
		/* We lost link */
		if (!mux->current_port->link) {

			if (mux->exclusive) {
				phy_mux_select_next_port(mux);
				mux->state = PHY_MUX_ESTABLISHING;
				phy_mux_trigger(mux);
				goto out;
			}

			xa_for_each(&mux->ports, index, port) {
				if (port == mux->current_port)
					continue;

				if (!port->link)
					continue;

				if (!port->enabled)
					continue;

				if (__phy_mux_select(mux, port)) {
					pr_err("%s : Error switching ports\n", __func__);
					mux->state = PHY_MUX_ACTIVE_LISTENING;
				}
				goto out;
			}
			mux->state = PHY_MUX_ACTIVE_LISTENING;
			break;
		}
		break;
	case PHY_MUX_FIXED:
		if (mux->current_port && mux->current_port->enabled &&
		    mux->current_port->forced)
			goto out;

		xa_for_each(&mux->ports, index, port) {
			if (port->forced && port->enabled) {
				__phy_mux_select(mux, port);

				/* Even if the select failed, we really want that
				 * port.
				 */
				mux->current_port = port;
			}
		}
		break;
	}
out:
	rtnl_unlock();
}

/* Mux handler, used for non-actively linked PHYs, will be called into by
 * phylink / phylib if active
 */
void phy_mux_notify(struct phy_device *phydev, bool on)
{
	struct phy_mux_port*port = phydev->mux_port;
	struct phy_mux *mux;

	if (!port)
		return;

	mux = port->mux;

	port->link = on;
	port->speed = phydev->speed;

	phy_mux_trigger(mux);
}
EXPORT_SYMBOL_GPL(phy_mux_notify);

void phy_mux_start(struct phy_mux *mux)
{
	if (mux->state != PHY_MUX_ATTACHED)
		return;

	/* TODO: Make that operation atomic. */
	mux->state = PHY_MUX_STARTING;
	phy_mux_trigger(mux);
}
EXPORT_SYMBOL_GPL(phy_mux_start);

void phy_mux_stop(struct phy_mux *mux)
{
	struct phy_mux_port *port;
	unsigned long index;

	ASSERT_RTNL();

	cancel_delayed_work_sync(&mux->state_queue);

	mux->state = PHY_MUX_ATTACHED;

	xa_for_each(&mux->ports, index, port) {
		phy_mux_port_deselect(port);
		phy_mux_port_stop_listening(port);
	}
}
EXPORT_SYMBOL_GPL(phy_mux_stop);

struct phy_mux *phy_mux_alloc(struct device *dev)
{
	struct phy_mux *mux;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	xa_init_flags(&mux->ports, XA_FLAGS_ALLOC1);
	INIT_LIST_HEAD(&mux->node);
	INIT_DELAYED_WORK(&mux->state_queue, phy_mux_state_machine);

	mux->dev = dev;

	return mux;
}
EXPORT_SYMBOL_GPL(phy_mux_alloc);

int phy_mux_create(struct phy_mux *mux, struct phy_mux_config *cfg, void *priv)
{
	memcpy(&mux->cfg, cfg, sizeof(mux->cfg));
	mux->mux_priv = priv;

	mux->state = PHY_MUX_INIT;

	mutex_lock(&phy_mux_mutex);
	list_add(&phy_muxes, &mux->node);
	mutex_unlock(&phy_mux_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_create);

int phy_mux_fwnode_create(struct phy_mux *mux, struct phy_mux_config *cfg,
			  const struct fwnode_handle *fwnode, void *priv)
{
	struct device_node *child;
	int ret;

	memcpy(&mux->cfg, cfg, sizeof(mux->cfg));
	mux->mux_priv = priv;

	mux->state = PHY_MUX_INIT;

	for_each_available_child_of_node(to_of_node(fwnode), child) {
		struct fwnode_handle *child_fwnode, *phy_fwnode;
		struct phy_device *phydev;
		struct phy_mux_port *port;
		int mode;

		child_fwnode = of_fwnode_handle(child);
		phy_fwnode = fwnode_get_phy_node(child_fwnode);
		if (IS_ERR(phy_fwnode)) {
			ret = PTR_ERR(phy_fwnode);
			goto out;
		}

		phydev = fwnode_phy_find_device(phy_fwnode);

		fwnode_handle_put(phy_fwnode);

		if (!phydev) {
			ret = -EPROBE_DEFER;
			goto out;
		}

		phy_device_free(phydev);

		port = phy_mux_port_create_from_phy(phydev);
		if (!port) {
			ret = -ENOMEM;
			goto out;
		}

		mode = fwnode_get_phy_mode(child_fwnode);
		if (mode < 0) {
			ret = -EINVAL;
			goto out;
		}

		port->interface = mode;

		ret = phy_mux_register_port(mux, port);
		if (ret)
			goto out;
	}

	mutex_lock(&phy_mux_mutex);
	list_add(&phy_muxes, &mux->node);
	mutex_unlock(&phy_mux_mutex);

	return 0;

out:
	return ret;
}
EXPORT_SYMBOL_GPL(phy_mux_fwnode_create);

int phy_mux_cleanup(struct phy_mux *mux)
{
	mutex_lock(&phy_mux_mutex);
	list_del(&mux->node);
	mutex_unlock(&phy_mux_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_cleanup);


