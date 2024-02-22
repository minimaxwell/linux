
#include <linux/phy.h>
#include <linux/phy_mux.h>
#include <linux/phy_port.h>

struct phy_port *phy_port_alloc(void)
{
	struct phy_port *port;

	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port)
		return NULL;

	mutex_init(&port->lock);

	return port;
}
EXPORT_SYMBOL_GPL(phy_port_alloc);

void phy_port_destroy(struct phy_port *port)
{
	kfree(port);
}
EXPORT_SYMBOL_GPL(phy_port_destroy);


void phy_port_register(struct phy_port *port, struct phy_port_ops *ops)
{
	mutex_lock(&port->lock);
	port->ops = ops;
	mutex_unlock(&port->lock);
}
EXPORT_SYMBOL_GPL(phy_port_register);

void phy_port_unregister(struct phy_port *port)
{
	mutex_lock(&port->lock);
	port->ops = NULL;
	mutex_unlock(&port->lock);
}
EXPORT_SYMBOL_GPL(phy_port_unregister);

int phy_port_get_state(struct phy_port *port, struct phy_port_state *state)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&port->lock);
	if (port->ops && port->ops->get_state)
		ret = port->ops->get_state(port, state);

	mutex_unlock(&port->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_port_get_state);

int phy_port_set_state(struct phy_port *port,
		       const struct phy_port_state *state)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&port->lock);
	if (port->ops && port->ops->set_state)
		ret = port->ops->set_state(port, state);

	mutex_unlock(&port->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_port_set_state);

int phy_port_ethtool_ksettings_get(struct phy_port *port,
				   struct ethtool_link_ksettings *kset)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&port->lock);
	if (port->ops && &port->ops->ethtool_ksettings_get)
		ret = port->ops->ethtool_ksettings_get(port, kset);

	mutex_unlock(&port->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_port_ethtool_ksettings_get);

int phy_port_ethtool_ksettings_set(struct phy_port *port,
				   const struct ethtool_link_ksettings *kset)
{
	int ret = -EOPNOTSUPP;

	mutex_lock(&port->lock);
	if (port->ops && port->ops->ethtool_ksettings_set)
		ret = port->ops->ethtool_ksettings_set(port, kset);

	mutex_unlock(&port->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(phy_port_ethtool_ksettings_set);
