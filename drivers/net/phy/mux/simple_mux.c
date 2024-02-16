
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy_mux.h>
#include <linux/platform_device.h>

static int simple_mux_isolate_select(struct phy_mux *mux,
				     struct phy_mux_port *port)
{
	struct phy_device *phydev;

	phydev = phy_mux_port_get_phy(port);

	return phy_isolate(phydev, true);
}

static int simple_mux_isolate_deselect(struct phy_mux *mux,
				       struct phy_mux_port *port)
{
	struct phy_device *phydev;

	phydev = phy_mux_port_get_phy(port);

	return phy_isolate(phydev, false);
}

static int simple_mux_probe(struct platform_device *pdev)
{
	struct device_node *child;
	struct phy_mux *mux;
	int ret;

	mux = phy_mux_alloc(&pdev->dev);
	if (IS_ERR(mux))
		return PTR_ERR(mux);

	platform_set_drvdata(pdev, mux);

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		struct fwnode_handle *child_fwnode, *phy_fwnode;
		struct phy_device *phydev;
		struct phy_mux_port *port;

		child_fwnode = of_fwnode_handle(child);
		phy_fwnode = fwnode_get_phy_node(child_fwnode);
		if (IS_ERR(phy_fwnode))
			return PTR_ERR(phy_fwnode);

		phydev = fwnode_phy_find_device(phy_fwnode);

		fwnode_handle_put(phy_fwnode);

		if (!phydev)
			return -ENODEV;

		phy_device_free(phydev);

		port = phy_mux_port_create(phydev);
		if (!port)
			return -ENOMEM;

		ret = phy_mux_register_port(mux, port);
		if (ret) {
			phy_mux_port_destroy(port);
			return ret;
		}
	}
	/* Parse DT and register ports */

	return 0;
}

static void simple_mux_remove(struct platform_device *pdev)
{
	struct phy_mux *mux = platform_get_drvdata(pdev);

	phy_mux_destroy(mux);
}

static const struct of_device_id simple_mux_of_match[] = {
	{ .compatible = "simple-phy-mux" },
	{ },
};

static struct platform_driver simple_mux_driver = {
	.probe = simple_mux_probe,
	.remove_new = simple_mux_remove,
	.driver = {
		.name = "simple_phy_mux",
		.of_match_table = simple_mux_of_match,
	},
};

module_platform_driver(simple_mux_driver);
