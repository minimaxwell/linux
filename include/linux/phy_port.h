
#ifndef __PHY_PORT_H
#define __PHY_PORT_H

#include <linux/types.h>

struct phy_device;
struct phy_mux_port;
struct sfp;
struct sfp_bus;
struct mutex;

struct phy_port;

enum phy_port_parent {
	PHY_PORT_PHY,
	PHY_PORT_SFP_CAGE,
	PHY_PORT_SFP_MODULE,
};

struct phy_port_state {
	bool enabled;
	bool forced;
	bool link;
	bool speed;
};

struct phy_port_ops {
	int (*get_state)(struct phy_port *port, struct phy_port_state *state);
	int (*set_state)(struct phy_port *port, const struct phy_port_state *state);

	int (*ethtool_ksettings_get)(struct phy_port *port,
				     struct ethtool_link_ksettings *kset);
	int (*ethtool_ksettings_set)(struct phy_port *port,
				     const struct ethtool_link_ksettings *kset);
};

struct phy_port {
	u32 id;
	enum phy_port_parent parent_type;
	union {
		struct phy_device *phy;
		struct sfp_bus *sfp_bus;
		struct sfp *sfp;
	};

	struct phy_mux_port *mux_port;

	const struct phy_port_ops *ops;
	struct mutex lock;
	void *priv;
};

struct phy_port *phy_port_alloc(void);
void phy_port_register(struct phy_port *port, struct phy_port_ops *ops);
void phy_port_unregister(struct phy_port *port);
void phy_port_destroy(struct phy_port *port);

static inline struct phy_device *port_phydev(struct phy_port *port)
{
	return port->phy;
}

static inline struct sfp_bus *port_sfp_bus(struct phy_port *port)
{
	return port->sfp_bus;
}

static inline struct sfp *port_sfp(struct phy_port *port)
{
	return port->sfp;
}

int phy_port_get_state(struct phy_port *port, struct phy_port_state *state);
int phy_port_set_state(struct phy_port *port, const struct phy_port_state *state);
int phy_port_ethtool_ksettings_get(struct phy_port *port,
				   struct ethtool_link_ksettings *kset);
int phy_port_ethtool_ksettings_set(struct phy_port *port,
				   const struct ethtool_link_ksettings *kset);

#endif /* __PHY_PORT_H */
