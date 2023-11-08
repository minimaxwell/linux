// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Bootlin
 *
 */
#include "common.h"
#include "netlink.h"
#include "bitset.h"

#include <linux/link_topology.h>
#include <linux/phy.h>
#include <linux/phy_port.h>
#include <linux/sfp.h>

struct phy_port_req_info {
	struct ethnl_req_info		base;
	u32				port_index;
};

#define PHY_PORT_REQINFO(__req_base) \
	container_of(__req_base, struct phy_port_req_info, base)

const struct nla_policy ethnl_phy_port_get_policy[ETHTOOL_A_PHY_PORT_INDEX + 1] = {
	[ETHTOOL_A_PHY_PORT_HEADER] = NLA_POLICY_NESTED(ethnl_header_policy),
	[ETHTOOL_A_PHY_PORT_INDEX] = NLA_POLICY_MIN(NLA_U32, 1),
};

const struct nla_policy ethnl_phy_port_set_policy[ETHTOOL_A_PHY_PORT_MAX + 1] = {
	[ETHTOOL_A_PHY_PORT_HEADER] = NLA_POLICY_NESTED(ethnl_header_policy),
};

/* Caller holds rtnl */
static ssize_t
ethnl_phy_port_reply_size(const struct ethnl_req_info *req_base,
			  struct netlink_ext_ack *extack)
{
	struct phy_port_req_info *req_info = PHY_PORT_REQINFO(req_base);
	struct phy_port *port = link_topo_get_port(&req_base->dev->link_topo,
						   req_info->port_index);
	bool compact = req_base->flags & ETHTOOL_FLAG_COMPACT_BITSETS;
	const char *upstream_name = NULL;
	size_t size = 0;
	int ret;

	if (!port)
		return 0;

	size += nla_total_size(sizeof(u32));	/* ETHTOOL_A_PHY_PORT_INDEX */
	size += nla_total_size(sizeof(u8));	/* ETHTOOL_A_PHY_PORT_STATE */
	size += nla_total_size(sizeof(u8));	/* ETHTOOL_A_PHY_PORT_UPSTREAM_TYPE */

	switch(port->cfg.upstream_type)
	{
	case PHY_UPSTREAM_PHY:
		struct phy_device *phydev = port->cfg.phydev;
		upstream_name = dev_name(&phydev->mdio.dev);
		break;
	case PHY_UPSTREAM_MAC:
		struct net_device *netdev = port->cfg.netdev;
		upstream_name = netdev->name;
		break;
	case PHY_UPSTREAM_SFP:
		struct sfp_bus *bus = port->cfg.sfp_bus;
		upstream_name = sfp_get_name(bus);
		break;
	default:
		return 0;
	}
	if (upstream_name)
		size += ethnl_strz_size(upstream_name);	/* ETHTOOL_A_PHY_PORT_UPSTREAM_NAME */
	size += nla_total_size(sizeof(u32));	/* ETHTOOL_A_PHY_PORT_UPSTREAM_INDEX */
	size += nla_total_size(sizeof(u8));	/* ETHTOOL_A_PHY_PORT_TYPE */

	if (port->cfg.ptype == PHY_PORT_MDI)
		ret = ethnl_bitset_size(port->cfg.supported_modes, NULL,
					__ETHTOOL_LINK_MODE_MASK_NBITS,
					link_mode_names, compact);
	else
		ret = ethnl_bitset_size(port->cfg.supported_interfaces, NULL,
					PHY_INTERFACE_MODE_MAX,
					interface_mode_names, compact);

	if (ret < 0)
		return ret;

	size += ret;
	size += nla_total_size(sizeof(u8));	/* ETHTOOL_A_PHY_PORT_LANES */

	return size;
}

static int
ethnl_phy_port_fill_reply(const struct ethnl_req_info *req_base, struct sk_buff *skb)
{
	struct phy_port_req_info *req_info = PHY_PORT_REQINFO(req_base);
	struct phy_port *port = link_topo_get_port(&req_base->dev->link_topo,
						   req_info->port_index);
	bool compact = req_base->flags & ETHTOOL_FLAG_COMPACT_BITSETS;
	const char *upstream_name = NULL;
	u32 upstream_index = 0;
	int ret;

	if (!port)
		return -ENODEV;

	if (nla_put_u32(skb, ETHTOOL_A_PHY_PORT_INDEX, port->index) ||
	    nla_put_u8(skb, ETHTOOL_A_PHY_PORT_STATE, port->state) ||
	    nla_put_u8(skb, ETHTOOL_A_PHY_PORT_UPSTREAM_TYPE, port->cfg.upstream_type))
		return -EMSGSIZE;

	switch(port->cfg.upstream_type)
	{
	case PHY_UPSTREAM_PHY:
		struct phy_device *phydev = port->cfg.phydev;
		upstream_name = dev_name(&phydev->mdio.dev);
		upstream_index = phydev->phyindex;
		break;
	case PHY_UPSTREAM_MAC:
		struct net_device *netdev = port->cfg.netdev;
		upstream_name = netdev->name;
		break;
	case PHY_UPSTREAM_SFP:
		struct sfp_bus *bus = port->cfg.sfp_bus;
		upstream_name = sfp_get_name(bus);
		break;
	default:
		return 0;
	}
	if (upstream_name && ethnl_put_strz(skb, ETHTOOL_A_PHY_PORT_UPSTREAM_NAME, upstream_name))
		return -EMSGSIZE;

	if (nla_put_u32(skb, ETHTOOL_A_PHY_PORT_UPSTREAM_INDEX, upstream_index) ||
	    nla_put_u8(skb, ETHTOOL_A_PHY_PORT_TYPE, port->cfg.ptype))
		return -EMSGSIZE;

	if (port->cfg.ptype == PHY_PORT_MDI)
		ret = ethnl_put_bitset(skb, ETHTOOL_A_PHY_PORT_SUPPORTED,
				       port->cfg.supported_modes, NULL,
				       __ETHTOOL_LINK_MODE_MASK_NBITS,
				       link_mode_names, compact);
	else
		ret = ethnl_put_bitset(skb, ETHTOOL_A_PHY_PORT_INTERFACES,
				       port->cfg.supported_interfaces, NULL,
				       PHY_INTERFACE_MODE_MAX,
				       interface_mode_names, compact);

	if (ret < 0)
		return -EMSGSIZE;

	if (nla_put_u8(skb, ETHTOOL_A_PHY_PORT_LANES, port->cfg.lanes))
		return -EMSGSIZE;

	return 0;
}

static int ethnl_phy_port_parse_request(struct ethnl_req_info *req_base,
					struct nlattr **tb)
{
	struct phy_port_req_info *req_info = PHY_PORT_REQINFO(req_base);

	if (tb[ETHTOOL_A_PHY_PORT_INDEX])
		req_info->port_index = nla_get_u32(tb[ETHTOOL_A_PHY_PORT_INDEX]);

	return 0;
}

int ethnl_phy_port_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct phy_port_req_info req_info = {};
	struct nlattr **tb = info->attrs;
	struct sk_buff *rskb;
	void *reply_payload;
	int reply_len;
	int ret;

	ret = ethnl_parse_header_dev_get(&req_info.base,
					 tb[ETHTOOL_A_PHY_PORT_HEADER],
					 genl_info_net(info), info->extack,
					 true);
	if (ret < 0)
		return ret;

	rtnl_lock();

	ret = ethnl_phy_port_parse_request(&req_info.base, tb);
	if (ret < 0)
		goto err_unlock_rtnl;

	/* No Port, return early */
	if (!req_info.port_index)
		goto err_unlock_rtnl;

	ret = ethnl_phy_port_reply_size(&req_info.base, info->extack);
	if (ret < 0)
		goto err_unlock_rtnl;

	reply_len = ret + ethnl_reply_header_size();

	rskb = ethnl_reply_init(reply_len, req_info.base.dev,
				ETHTOOL_MSG_PHY_PORT_GET_REPLY,
				ETHTOOL_A_PHY_PORT_HEADER,
				info, &reply_payload);
	if (!rskb) {
		ret = -ENOMEM;
		goto err_unlock_rtnl;
	}

	ret = ethnl_phy_port_fill_reply(&req_info.base, rskb);
	if (ret)
		goto err_free_msg;

	rtnl_unlock();
	ethnl_parse_header_dev_put(&req_info.base);
	genlmsg_end(rskb, reply_payload);

	return genlmsg_reply(rskb, info);

err_free_msg:
	nlmsg_free(rskb);
err_unlock_rtnl:
	rtnl_unlock();
	ethnl_parse_header_dev_put(&req_info.base);
	return ret;

	return 0;
}

struct ethnl_phy_port_dump_ctx {
	struct ethnl_req_info	req_info;
};

int ethnl_phy_port_start(struct netlink_callback *cb)
{
	const struct genl_dumpit_info *info = genl_dumpit_info(cb);
	struct ethnl_phy_port_dump_ctx *ctx = (void *)cb->ctx;
	struct nlattr **tb = info->info.attrs;
	int ret;

	BUILD_BUG_ON(sizeof(*ctx) > sizeof(cb->ctx));

	memset(ctx, 0, sizeof(*ctx));

	ret = ethnl_parse_header_dev_get(&ctx->req_info,
					 tb[ETHTOOL_A_PHY_PORT_HEADER],
					 sock_net(cb->skb->sk), cb->extack,
					 false);
	return ret;
}

int ethnl_phy_port_dump_one_dev(struct sk_buff *skb, struct net_device *dev,
				struct netlink_callback *cb)
{
	struct ethnl_phy_port_dump_ctx *ctx = (void *)cb->ctx;
	struct phy_port_req_info *pri = PHY_PORT_REQINFO(&ctx->req_info);
	long unsigned int index = 1;
	struct phy_port *port;
	void *ehdr;
	int ret;

	ctx->req_info.dev = dev;

	xa_for_each(&dev->link_topo.ports, index, port) {
		ehdr = ethnl_dump_put(skb, cb,
				      ETHTOOL_MSG_PHY_PORT_GET_REPLY);
		if (!ehdr) {
			ret = -EMSGSIZE;
			break;
		}

		ret = ethnl_fill_reply_header(skb, dev,
					      ETHTOOL_A_PHY_PORT_HEADER);
		if (ret < 0) {
			genlmsg_cancel(skb, ehdr);
			break;
		}

		pri->port_index = index;
		ret = ethnl_phy_port_fill_reply(&ctx->req_info, skb);

		genlmsg_end(skb, ehdr);
	}

	return ret;
}

int ethnl_phy_port_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct ethnl_phy_port_dump_ctx *ctx = (void *)cb->ctx;
	struct net *net = sock_net(skb->sk);
	long unsigned int ifindex = 1;
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();

	if (ctx->req_info.dev) {
		ret = ethnl_phy_port_dump_one_dev(skb, ctx->req_info.dev, cb);
	} else {
		for_each_netdev_dump(net, dev, ifindex) {
			ret = ethnl_phy_port_dump_one_dev(skb, dev, cb);
			if (ret)
				break;
		}
	}
	rtnl_unlock();

	if (ret == -EMSGSIZE && skb->len)
		return skb->len;
	return ret;
}

