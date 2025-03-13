// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2025 Maxime Chevallier <maxime.chevallier@bootlin.com>
 *
 */
#include <linux/phy.h>
#include <linux/phy_link_topology.h>
#include <linux/xarray.h>

#include "bitset.h"
#include "common.h"
#include "netlink.h"

struct port_req_info {
	struct ethnl_req_info base;
	unsigned long port_index;
};

struct port_reply_data {
	struct ethnl_reply_data	base;
	unsigned long port_index;
	u32 mediums;
	int parent_type;
	char *parent_name;
	bool active;
	bool selected;
};

#define PORT_REQINFO(__req_base) \
	container_of(__req_base, struct port_req_info, base)
#define PORT_REPDATA(__reply_base) \
	container_of(__reply_base, struct port_reply_data, base)

const struct nla_policy ethnl_port_get_policy[ETHTOOL_A_PORT_INDEX + 1] = {
	[ETHTOOL_A_PORT_HEADER] = NLA_POLICY_NESTED(ethnl_header_policy),
	[ETHTOOL_A_PORT_INDEX] = NLA_POLICY_MIN(NLA_U32, 1),
};

static int port_parse_request(struct ethnl_req_info *req_info,
			      struct nlattr **tb,
			      struct netlink_ext_ack *extack)
{
	struct port_req_info *req = PORT_REQINFO(req_info);

	if (!tb[ETHTOOL_A_PORT_INDEX])
		return -EINVAL;

	req->port_index = nla_get_u32(tb[ETHTOOL_A_PORT_INDEX]);

	return 0;
}

static int port_reply_size(const struct ethnl_req_info *req_info,
			   const struct ethnl_reply_data *reply_data)
{
	bool compact = req_info->flags & ETHTOOL_FLAG_COMPACT_BITSETS;
	struct port_reply_data *rep_data = PORT_REPDATA(reply_data);
	size_t size = 0;

	/* ETHTOOL_A_PORT_INDEX */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_MEDIUMS */
	size += ethnl_bitset32_size(&rep_data->mediums, NULL,
				    __ETHTOOL_LINK_MEDIUM_LAST,
				    ethtool_link_medium_names, compact);

	/* ETHTOOL_A_PORT_PARENT_TYPE */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_ACTIVE */
	size += nla_total_size(sizeof(u8));

	/* ETHTOOL_A_PORT_SELECTED */
	size += nla_total_size(sizeof(u8));

	/* ETHTOOL_A_PORT_PARENT_NAME */
	if (rep_data->parent_name)
		size += nla_total_size(strlen(rep_data->parent_name) + 1);

	return size;
}

static int port_prepare_data(const struct ethnl_req_info *req_info,
			     struct ethnl_reply_data *reply_data,
			     const struct genl_info *info)
{
	struct port_reply_data *rep_data = PORT_REPDATA(reply_data);
	struct port_req_info *req = PORT_REQINFO(req_info);
	struct net_device *dev = reply_data->dev;
	struct phy_port *port;

	if (!req->port_index || !dev->link_topo)
		return -EINVAL;

	/* caller holds rtnl */
	port = phy_link_topo_get_port(dev, req->port_index);
	if (!port)
		return -ENODEV;

	rep_data->port_index = req->port_index;
	rep_data->mediums = port->mediums;
	rep_data->parent_type = port->parent_type;
	rep_data->active = port->active;

	//rep_data->selected = port->selected;
	rep_data->selected = false;

	if (port->parent_type == PHY_PORT_PHY)
		rep_data->parent_name = kstrdup(phydev_name(port->phy), GFP_KERNEL);

	return 0;
}

static int port_fill_reply(struct sk_buff *skb,
			  const struct ethnl_req_info *req_info,
			  const struct ethnl_reply_data *reply_data)
{
	bool compact = req_info->flags & ETHTOOL_FLAG_COMPACT_BITSETS;
	struct port_reply_data *rep_data = PORT_REPDATA(reply_data);

	if (nla_put_u32(skb, ETHTOOL_A_PORT_INDEX, rep_data->port_index) ||
	    nla_put_u32(skb, ETHTOOL_A_PORT_PARENT_TYPE, rep_data->parent_type) ||
	    nla_put_u32(skb, ETHTOOL_A_PORT_ACTIVE, rep_data->active) ||
	    nla_put_u32(skb, ETHTOOL_A_PORT_SELECTED, rep_data->selected))
		return -EMSGSIZE;

	if (rep_data->parent_name &&
	    nla_put_string(skb, ETHTOOL_A_PORT_PARENT_NAME,
		           rep_data->parent_name))
		return -EMSGSIZE;

	return ethnl_put_bitset32(skb, ETHTOOL_A_PORT_MEDIUMS,
				  &rep_data->mediums, NULL,
				  __ETHTOOL_LINK_MEDIUM_LAST,
				  ethtool_link_medium_names, compact);
}

static void port_cleanup_data(struct ethnl_reply_data *reply_data)
{
	struct port_reply_data *rep_data = PORT_REPDATA(reply_data);

	kfree(rep_data->parent_name);
}

struct port_dump_ctx {
	unsigned long port_idx;
};

static int port_dump_start(struct ethnl_dump_ctx *ctx)
{
	struct port_dump_ctx *dump_ctx;

	dump_ctx = kzalloc(sizeof(*dump_ctx), GFP_KERNEL);
	if (!dump_ctx)
		return -ENOMEM;

	ctx->cmd_ctx = dump_ctx;

	return 0;
}

static int port_dump_one_dev(struct sk_buff *skb, struct ethnl_dump_ctx *ctx,
			      const struct genl_info *info)
{
	struct port_req_info *req_info = PORT_REQINFO(ctx->req_info);
	struct net_device *dev = ctx->reply_data->dev;
	struct port_dump_ctx *dump_ctx = ctx->cmd_ctx;
	struct phy_port *port;
	int ret;

	if (!dev->link_topo)
		return 0;

	xa_for_each_start(&dev->link_topo->ports, dump_ctx->port_idx,
			  port, dump_ctx->port_idx) {

		req_info->port_index = dump_ctx->port_idx;

		ret = ethnl_default_dump_one(skb, ctx, info);
		if (ret)
			break;
	}

	return ret;
}

static void port_dump_done(struct ethnl_dump_ctx *ctx)
{
	kfree(ctx->cmd_ctx);
}

void ethnl_port_notify(struct net_device *dev, struct phy_port *port)
{
	pr_info("%s : port %d active [%s] link [%s]\n", __func__, port->port_index,
		port->active ? "yes" : "no",
		port->link ? "up" : "down");
}

const struct ethnl_request_ops ethnl_port_request_ops = {
	.request_cmd		= ETHTOOL_MSG_PORT_GET,
	.reply_cmd		= ETHTOOL_MSG_PORT_GET_REPLY,
	.hdr_attr		= ETHTOOL_A_PORT_HEADER,
	.req_info_size		= sizeof(struct port_req_info),
	.reply_data_size	= sizeof(struct port_reply_data),

	.parse_request		= port_parse_request,
	.prepare_data		= port_prepare_data,
	.reply_size		= port_reply_size,
	.fill_reply		= port_fill_reply,
	.cleanup_data		= port_cleanup_data,

	/* Need custom DUMP */
	.dump_start		= port_dump_start,
	.dump_one_dev		= port_dump_one_dev,
	.dump_done		= port_dump_done,

	.allow_pernetdev_dump	= true,
};
