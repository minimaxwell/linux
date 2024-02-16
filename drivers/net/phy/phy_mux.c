// SPDX-License-Identifier: GPL-2.0+
/*
 * Ethernet PHY generic multiplexing logic
 *
 * Copyright (c) 2024 Maxime Chevallier <maxime.chevallier@bootlin.com>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/phylink.h>
#include <linux/phy_mux.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>
#include <linux/xarray.h>

struct phy_mux_port {
	int id;
	struct phy_device *phydev;
};

struct phy_mux {
	struct phy_mux_config cfg;
	struct xarray ports;
	struct mutex lock;
	struct device *dev;
	struct phy_mux_port *current_port;
	struct phylink *pl;
	void *priv;
};

struct phy_mux *phy_mux_alloc(struct device *dev)
{
	struct phy_mux *mux;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	xa_init_flags(&mux->ports, XA_FLAGS_ALLOC1);
	mutex_init(&mux->lock);

	mux->dev = dev;

	return mux;
}
EXPORT_SYMBOL_GPL(phy_mux_alloc);

int phy_mux_init(struct phy_mux *mux, struct phy_mux_config *cfg, void *priv)
{
	if (!cfg->ops.select || !cfg->ops.port_event)
		return -EINVAL;

	memcpy(&mux->cfg, cfg, sizeof(mux->cfg));
	mux->priv = priv;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_init);

int phy_mux_phylink_register(struct phy_mux *mux, struct phylink *pl)
{
	if (mux->pl)
		return -EBUSY;

	mux->pl = pl;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mux_phylink_register);

void phy_mux_destroy(struct phy_mux *mux)
{
	xa_destroy(&mux->ports);
}
EXPORT_SYMBOL_GPL(phy_mux_destroy);

void *phy_mux_priv(struct phy_mux *mux)
{
	return mux->priv;
}
EXPORT_SYMBOL_GPL(phy_mux_priv);

int phy_mux_register_port(struct phy_mux *mux,
			  struct phy_mux_port *port)
{
	return xa_alloc(&mux->ports, &port->id, port, xa_limit_16b, GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(phy_mux_register_port);

static int __phy_mux_select(struct phy_mux *mux, struct phy_mux_port *port)
{
	struct phy_mux_ops *ops = &mux->cfg.ops;
	int ret;

	if (mux->current_port && mux->current_port == port)
		return 0;

	if (ops->deselect) {
		ret = ops->deselect(mux, mux->current_port);
		if (ret)
			return ret;
	}

	rtnl_lock();
	phylink_disconnect_phy(mux->pl);
	rtnl_unlock();

	ret = ops->select(mux, port);
	if (ret)
		return ret;

	ret = phylink_connect_phy(mux->pl, port->phydev);
	if (ret) {
		if (ops->deselect)
			ops->deselect(mux, port);
	}

	dev_info(mux->dev, "switching to port %d\n", port->id);

	mux->current_port = port;

	return 0;
}

int phy_mux_port_notify(struct phy_mux *mux, struct phy_mux_port *port,
			const struct phy_mux_port_event *evt)
{
	struct phy_mux_ops *ops = &mux->cfg.ops;
	struct phy_mux_port *selected_port;
	int ret;

	mutex_lock(&mux->lock);

	selected_port = ops->port_event(mux, port, evt);
	if (IS_ERR_OR_NULL(selected_port))
		goto out;

	if (selected_port != mux->current_port)
		ret = __phy_mux_select(mux, selected_port);

out:
	mutex_unlock(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_mux_port_notify);

int phy_mux_select(struct phy_mux *mux, struct phy_mux_port *port)
{
	int ret;

	mutex_lock(&mux->lock);
	ret = __phy_mux_select(mux, port);
	mutex_unlock(&mux->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_mux_select);

struct phy_mux_port *phy_mux_port_create(struct phy_device *phydev)
{
	struct phy_mux_port *port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return NULL;

	port->phydev = phydev;

	return port;
}
EXPORT_SYMBOL_GPL(phy_mux_port_create);

void phy_mux_port_destroy(struct phy_mux_port *port)
{
	kfree(port);
}
EXPORT_SYMBOL_GPL(phy_mux_port_destroy);

struct phy_device *phy_mux_port_get_phy(struct phy_mux_port *port)
{
	return port->phydev;
}
