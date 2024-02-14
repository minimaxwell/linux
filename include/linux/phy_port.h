#ifndef __PHY_PORT_H
#define __PHY_PORT_H

#include <linux/ethtool.h>
#include <linux/phy.h>

struct phy_port;

enum phy_port_upstream_type {
	PHY_PORT_MAC,
	PHY_PORT_PHY,
	PHY_PORT_SFP,
};

enum phy_port_state {
	PHY_PORT_UNREGISTERED,
	PHY_PORT_DISABLED,
	PHY_PORT_STANDBY,
	PHY_PORT_LINK_UP,
};

enum phy_port_type {
	PHY_PORT_MII,
	PHY_PORT_MDI,
};

struct phy_port;

struct phy_port_ops {
	int (* phy_port_set_active)(struct phy_port *port);
	int (* phy_port_set_inactive)(struct phy_port *port);
	int (* phy_port_get_link_ksettings)(struct phy_port *port,
					     struct ethtool_link_ksettings *ksettings);
	int (* phy_port_set_link_ksettings)(struct phy_port *port,
					     struct ethtool_link_ksettings *ksettings);
};

struct phy_port_config {
	int lanes;
	int port;
	bool internal;

	enum phy_port_type ptype;
	union {
		__ETHTOOL_DECLARE_LINK_MODE_MASK(supported_modes);
		DECLARE_PHY_INTERFACE_MASK(supported_interfaces);
	};

	enum phy_port_upstream_type upstream_type;
	union {
		struct net_device	*netdev;
		struct phy_device	*phydev;
		struct sfp_bus		*sfp_bus;
	};

	struct phy_link_topology *lt;
	const struct phy_port_ops *ops;
};

struct phy_port {
	struct phy_port_config cfg;
	enum phy_port_state state;
	u32 index;

	void *priv;
};

struct phy_port *phy_port_create(const struct phy_port_config *cfg);
void phy_port_destroy(struct phy_port *port);
void phy_port_set_internal(struct phy_port *port, bool internal);
int phy_port_set_state(struct phy_port *port, enum phy_port_state state);

#endif /* __PHY_PORT_H */
