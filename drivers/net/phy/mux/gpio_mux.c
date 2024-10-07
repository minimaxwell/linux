
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy_mux.h>
#include <linux/platform_device.h>

struct gpio_mux {
	/* For dual_port mux whose state is selected by toggling a gpio */
	struct gpio_desc *select_gpio;
};

struct gpio_mux_port {
	int gpio_state;
};

static int gpio_mux_select(struct phy_mux *mux, struct phy_mux_port *port)
{
	struct gpio_mux_port *port_priv = port->priv;
	struct gpio_mux *priv = phy_mux_priv(mux);

	gpiod_set_value(priv->select_gpio, port_priv->gpio_state);

	return 0;
}

static const struct phy_mux_ops gpio_mux_ops = {
	.select = gpio_mux_select,
};

static int gpio_mux_probe(struct platform_device *pdev)
{
	struct phy_mux_config cfg = {};
	struct device_node *child;
	struct gpio_desc *desc;
	bool low_found = false;
	struct gpio_mux *priv;
	struct phy_mux *mux;
	int ret;

	mux = phy_mux_alloc(&pdev->dev);
	if (IS_ERR(mux))
		return PTR_ERR(mux);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		goto out_destroy;

	platform_set_drvdata(pdev, mux);

	cfg.ops = &gpio_mux_ops;

	/* This mux can make sure there won't be interferences at the MII level
	 */
	cfg.isolating = true;

	cfg.max_n_ports = 2;

	desc = fwnode_gpiod_get_index(dev_fwnode(&pdev->dev), "select", 0,
				      GPIOD_OUT_LOW, "phy-mux");
	if (IS_ERR(desc)) {
		ret = PTR_ERR(desc);
		goto out_destroy;
	}

	priv->select_gpio = desc;

	ret = phy_mux_create(mux, &cfg, priv);
	if (ret)
		goto out_gpio;

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		struct fwnode_handle *child_fwnode, *phy_fwnode;
		struct gpio_mux_port *port_priv;
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

		port_priv = devm_kzalloc(&pdev->dev, sizeof(*port_priv),
				GFP_KERNEL);
		if (!port_priv)
			goto out;

		port->priv = port_priv;

		if (of_property_read_bool(child, "active-low")) {
			if (low_found) {
				ret = -EINVAL;
				goto out;
			}

			low_found = true;
			port_priv->gpio_state = 0;
		} else {
			port_priv->gpio_state = 1;
		}

		ret = phy_mux_register_port(mux, port);
		if (ret)
			goto out;
	}

	if (!low_found) {
		ret = -EINVAL;
		goto out;
	}

	return 0;

out:
	phy_mux_cleanup(mux);
out_gpio:
	gpiod_put(priv->select_gpio);
out_destroy:
	phy_mux_destroy(mux);

	return ret;
}

static void gpio_mux_remove(struct platform_device *pdev)
{
	struct phy_mux *mux = platform_get_drvdata(pdev);
	struct gpio_mux *priv = phy_mux_priv(mux);

	gpiod_put(priv->select_gpio);
	phy_mux_destroy(mux);
}

static const struct of_device_id gpio_mux_of_match[] = {
	{ .compatible = "gpio-phy-mux" },
	{ },
};

static struct platform_driver gpio_mux_driver = {
	.probe = gpio_mux_probe,
	.remove_new = gpio_mux_remove,
	.driver = {
		.name = "gpio_phy_mux",
		.of_match_table = gpio_mux_of_match,
	},
};

module_platform_driver(gpio_mux_driver);
