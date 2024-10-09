
#ifndef __PHY_MUX_H
#define __PHY_MUX_H

struct device;
struct phy_mux;
struct phy_mux_port;
struct phy_device;
struct phylink;
struct phy_port_state;

enum phy_mux_port_link {
	PHY_MUX_PORT_LINK_NO_EVT = 0,
	PHY_MUX_PORT_LINK_UP,
	PHY_MUX_PORT_LINK_DOWN,
};

struct phy_mux_port_event {
	enum phy_mux_port_link link;
};

struct phy_mux_port_ops {
	int (*deselect)(struct phy_mux_port *port);
	int (*select)(struct phy_mux_port *port);

	/* A listening port is inactive on the link, but can report link
	 * events. Exclusive ports can't listen without being selected first.
	 */
	int (*start_listening)(struct phy_mux_port *port);
	int (*stop_listening)(struct phy_mux_port *port);
};

enum phy_mux_port_type {
	PHY_MUX_PORT_PHY,
};

struct phy_mux_port {
	int id;

	struct phy_mux *mux;
	enum phy_mux_port_type type;
	union {
		struct phy_device *phydev;
	};
	void *priv;

	const struct phy_mux_port_ops *ops;

	phy_interface_t interface;

	/* The port can't listen to its own state without monopolizing the mii bus.
	 * Multiple exclusive ports can't perform link detection unless they sit
	 * on an isolating bus.
	 */
	bool exclusive;

	bool selected;
	bool listening;

	/* For internal use only */
	bool link;
	int speed;
	bool enabled;

	/* Force that port to be use. Automatically disables all other ports. */
	bool forced;

	/* Track when a port is being reconfigured */
	bool establishing;

	/* When a port is selected, we give it some time to establish the link.
	 * in that time, the port might generate transient down/up events.
	 * If after the deadline the link isn't up on the port, the mux
	 * falls-back to listening.
	 */
	unsigned long establishing_deadline;
};

struct phy_mux_ops {
	/* Port control ops */
	int (*deselect)(struct phy_mux *mux, struct phy_mux_port *port);
	int (*select)(struct phy_mux *mux, struct phy_mux_port *port);

	/* Port selection logic */
	struct phy_mux_port *(*port_event)(struct phy_mux *mux,
					   struct phy_mux_port *port,
					   const struct phy_mux_port_event *evt);
};

struct phy_mux_parent_ops {
	int (*deselect)(struct phy_mux *mux, struct phy_mux_port *port, void *priv);
	int (*select)(struct phy_mux *mux, struct phy_mux_port *port, void *priv);
};

#define PHY_MUX_NO_LIMIT (-1)

struct phy_mux_config {
	const struct phy_mux_ops *ops;
	int max_n_ports;

	u32 isolating:1;
};

/* MUX driver API */
struct phy_mux *phy_mux_alloc(struct device *dev);
void phy_mux_destroy(struct phy_mux *mux);

int phy_mux_create(struct phy_mux *mux, struct phy_mux_config *cfg, void *priv);
int phy_mux_fwnode_create(struct phy_mux *mux, struct phy_mux_config *cfg,
			  const struct fwnode_handle *fwnode, void *priv);
int phy_mux_cleanup(struct phy_mux *mux);
void *phy_mux_priv(struct phy_mux *mux);

struct phy_mux_port *phy_mux_port_create_from_phy(struct phy_device *phydev);
int phy_mux_register_port(struct phy_mux *mux, struct phy_mux_port *port);
void phy_mux_port_destroy(struct phy_mux_port *port);

/* MUX parent API */
int phy_mux_attach(struct phy_mux *mux, struct net_device *dev,
		   struct phy_mux_parent_ops *pops, void *priv);
void phy_mux_detach(struct phy_mux *mux);

void phy_mux_start(struct phy_mux *mux);
void phy_mux_stop(struct phy_mux *mux);

int mux_get_state(struct phy_port *port, struct phy_port_state *state);
int mux_set_state(struct phy_port *phy_port, const struct phy_port_state *state);

struct phy_mux *fwnode_phy_mux_get(const struct fwnode_handle *fwnode);

/* Can be called by the MUX driver, the parent or the ports, depending on
 * who can detect mux status change
 */
void phy_mux_notify(struct phy_device *dev, bool on);

#endif /* __PHY_MUX_H */
