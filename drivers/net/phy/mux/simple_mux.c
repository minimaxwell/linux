
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/phy_mux.h>
#include <linux/platform_device.h>

static const struct phy_mux_ops simple_mux_ops = { };

static int simple_mux_probe(struct platform_device *pdev)
{
	struct phy_mux_config cfg = {};
	struct phy_mux *mux;
	int ret;

	mux = phy_mux_alloc(&pdev->dev);
	if (IS_ERR(mux))
		return PTR_ERR(mux);

	platform_set_drvdata(pdev, mux);

	cfg.ops = &simple_mux_ops;
	cfg.isolating = false;
	cfg.max_n_ports = PHY_MUX_NO_LIMIT;

	ret = phy_mux_fwnode_create(mux, &cfg, dev_fwnode(&pdev->dev), NULL);
	if (ret)
		goto out;

	return 0;

out:
	phy_mux_destroy(mux);

	return ret;
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
