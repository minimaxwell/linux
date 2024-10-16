
#include <linux/of.h>
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
	linkmode_zero(port->supported);
	INIT_LIST_HEAD(&port->head);

	return port;
}
EXPORT_SYMBOL_GPL(phy_port_alloc);

void phy_port_destroy(struct phy_port *port)
{
	kfree(port);
}
EXPORT_SYMBOL_GPL(phy_port_destroy);

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

static void ethtool_medium_get_supported(unsigned long *supported,
					enum ethtool_link_medium medium,
					int lanes)
{
	int i;

	for (i = 0; i < __ETHTOOL_LINK_MODE_MASK_NBITS; i++) {

		/* Special bits such as Autoneg, Pause, Asym_pause, etc. are
		 * set and will be masked away by the port parent.
		 */
		if (link_mode_params[i].medium == ETHTOOL_LINK_MEDIUM_NONE) {
			linkmode_set_bit(i, supported);
			continue;
		}

		/* For most cases, min_lanes == lanes, except for 10/100BaseT that work
		 * on 2 lanes but are compatible with 4 lanes mediums
		 */
		if (link_mode_params[i].medium == medium &&
		    link_mode_params[i].lanes >= lanes &&
		    link_mode_params[i].min_lanes <= lanes ) {
			linkmode_set_bit(i, supported);
		}
	}
}

static enum ethtool_link_medium ethtool_str_to_medium(const char *str)
{
	int i;

	for (i = 0; i < __ETHTOOL_LINK_MEDIUM_LAST; i++)
		if (!strcmp(phy_mediums(i), str))
			return i;

	return ETHTOOL_LINK_MEDIUM_NONE;
}

struct phy_port *phy_of_parse_port(struct device_node *dn)
{
	struct fwnode_handle *fwnode = of_fwnode_handle(dn);
	enum ethtool_link_medium medium;
	struct phy_port *port;
	const char *med_str;
	u32 lanes;
	int ret;

	ret = fwnode_property_read_u32(fwnode, "lanes", &lanes);
	if (ret)
		return ERR_PTR(ret);


	ret = fwnode_property_read_string(fwnode, "media", &med_str);
	if (ret)
		return ERR_PTR(ret);

	medium = ethtool_str_to_medium(med_str);
	if (medium == ETHTOOL_LINK_MEDIUM_NONE)
		return ERR_PTR(-EINVAL);

	port = phy_port_alloc();
	if (!port)
		return ERR_PTR(-ENOMEM);

	port->lanes = lanes;
	port->medium = medium;

	ethtool_medium_get_supported(port->supported, medium, lanes);

	return port;
}
EXPORT_SYMBOL_GPL(phy_of_parse_port);
