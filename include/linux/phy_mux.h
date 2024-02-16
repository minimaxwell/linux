
#ifndef __PHY_MUX_H
#define __PHY_MUX_H

struct device;
struct phy_mux;
struct phy_mux_port;
struct phy_device;
struct phylink;

enum phy_mux_port_link {
	PHY_MUX_PORT_LINK_NO_EVT = 0,
	PHY_MUX_PORT_LINK_UP,
	PHY_MUX_PORT_LINK_DOWN,
};

struct phy_mux_port_event {
	enum phy_mux_port_link link;
};

struct phy_mux_ops {
	/* Port control ops */
	int (*select)(struct phy_mux *mux, struct phy_mux_port *port);
	int (*deselect)(struct phy_mux *mux, struct phy_mux_port *port);

	/* Port selection logic */
	struct phy_mux_port *(*port_event)(struct phy_mux *mux,
					   struct phy_mux_port *port,
					   const struct phy_mux_port_event *evt);
};

struct phy_mux_config {
	struct phy_mux_ops ops;
};

struct phy_mux *phy_mux_alloc(struct device *dev);
void phy_mux_destroy(struct phy_mux *mux);

int phy_mux_init(struct phy_mux *mux, struct phy_mux_config *cfg, void *priv);

int phy_mux_phylink_register(struct phy_mux *mux, struct phylink *pl);

void *phy_mux_priv(struct phy_mux *mux);

struct phy_mux_port *phy_mux_port_create(struct phy_device *phydev);
void phy_mux_port_destroy(struct phy_mux_port *port);
struct phy_device *phy_mux_port_get_phy(struct phy_mux_port *port);

int phy_mux_register_port(struct phy_mux *mux, struct phy_mux_port *port);

int phy_mux_select(struct phy_mux *mux, struct phy_mux_port *port);

int phy_mux_port_notify(struct phy_mux *mux, struct phy_mux_port *port,
			const struct phy_mux_port_event *evt);

#endif /* __PHY_MUX_H */
