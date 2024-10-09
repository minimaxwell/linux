// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Bootlin
 *
 */
#include <linux/phy.h>
#include <linux/phy_link_topology.h>
#include <linux/phy_port.h>
#include <linux/sfp.h>
#include "bitset.h"
#include "common.h"
#include "netlink.h"

struct port_req_info {
	struct ethnl_req_info		base;
	u32 port_id;
};

struct port_reply_data {
	struct ethnl_reply_data base;
	struct phy_port_state st;
	struct ethtool_link_ksettings ksettings;
	u32 port_id;
	enum phy_port_parent pt;
};

#define PORT_REQINFO(__req_base) \
	container_of(__req_base, struct port_req_info, base)
#define PORT_REPDATA(__reply_base) \
	container_of(__reply_base, struct port_reply_data, base)

const struct nla_policy ethnl_port_get_policy[ETHTOOL_A_PORT_ID + 1] = {
	[ETHTOOL_A_PORT_HEADER] = NLA_POLICY_NESTED(ethnl_header_policy),
	[ETHTOOL_A_PORT_ID]		= NLA_POLICY_MIN(NLA_U32, 1),
};

/* Caller holds rtnl */
static ssize_t
ethnl_port_reply_size(const struct ethnl_req_info *req_base,
		      const struct port_reply_data *reply)
{
	bool compact = req_base->flags & ETHTOOL_FLAG_COMPACT_BITSETS;
	size_t size = 0;
	int ret;

	/* ETHTOOL_A_PORT_ID */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_TYPE */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_ENABLED */
	size += nla_total_size(sizeof(u8));

	/* ETHTOOL_A_PORT_FORCED */
	size += nla_total_size(sizeof(u8));

	/* ETHTOOL_A_PORT_LINK */
	size += nla_total_size(sizeof(u8));

	/* ETHTOOL_A_PORT_SPEED */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_LANES */
	size += nla_total_size(sizeof(u32));

	/* ETHTOOL_A_PORT_DUPLEX */
	size += nla_total_size(sizeof(u8));

	ret = ethnl_bitset_size(reply->ksettings.link_modes.advertising,
				reply->ksettings.link_modes.supported,
				__ETHTOOL_LINK_MODE_MASK_NBITS,
				link_mode_names, compact);
	if (ret < 0)
		return ret;

	size += ret;

	return size;
}

static int ethnl_port_prepare_data(const struct ethnl_req_info *req_base,
				   struct ethnl_reply_data *reply_base,
				   const struct genl_info *info)
{
	struct port_reply_data *reply = PORT_REPDATA(reply_base);
	struct port_req_info *req_info = PORT_REQINFO(req_base);
	struct nlattr **tb = info->attrs;
	struct phy_device *phydev;
	struct phy_port *port;
	int ret;

	ASSERT_RTNL();

	phydev = ethnl_req_get_phydev(req_base, tb[ETHTOOL_A_PORT_HEADER],
				      info->extack);

	if (req_info->port_id) {
		port = phy_link_topo_get_port(req_base->dev->link_topo,
					      req_info->port_id);
	} else if (!IS_ERR_OR_NULL(phydev)) {
		port = phy_get_single_port(phydev);
	} else if (req_base->dev->sfp_bus) {
		port = sfp_get_port(req_base->dev->sfp_bus);
	} else {
		port = NULL;
		/* TODO : phy-less dev port (through SFP ?) */
	}

	if (!port)
		return -ENODEV;

	ret = phy_port_get_state(port, &reply->st);
	if (ret)
		return ret;

	ret = phy_port_ethtool_ksettings_get(port, &reply->ksettings);
	if (ret)
		return ret;

	reply->port_id = port->id;
	reply->pt = port->parent_type;

	return 0;
}

static int
ethnl_port_fill_reply(const struct ethnl_req_info *req_base,
		      const struct port_reply_data *reply, struct sk_buff *skb)
{
	const struct ethtool_link_ksettings *ksettings = &reply->ksettings;
	int ret;

	/* FIXME pass req_base from DUMP context */
	bool compact = req_base ?
		       (req_base->flags & ETHTOOL_FLAG_COMPACT_BITSETS) :
		       false;

	if(nla_put_u8(skb, ETHTOOL_A_PORT_ENABLED, reply->st.enabled) ||
	   nla_put_u8(skb, ETHTOOL_A_PORT_FORCED, reply->st.forced) ||
	   nla_put_u8(skb, ETHTOOL_A_PORT_LINK, reply->st.link) ||
	   nla_put_u32(skb, ETHTOOL_A_PORT_SPEED, reply->st.speed))
		return -EMSGSIZE;

	if (nla_put_u32(skb, ETHTOOL_A_PORT_ID, reply->port_id) ||
	    nla_put_u32(skb, ETHTOOL_A_PORT_TYPE, reply->pt))
		return -EMSGSIZE;

	ret = ethnl_put_bitset(skb, ETHTOOL_A_PORT_LINKMODES,
			       ksettings->link_modes.advertising,
			       ksettings->link_modes.supported,
			       __ETHTOOL_LINK_MODE_MASK_NBITS, link_mode_names,
			       compact);
	if (ret < 0)
		return -EMSGSIZE;

	/* TODO : Duplex
	if (nla_put_u8(skb, ETHTOOL_A_LINKMODES_DUPLEX, ksettings->duplex))
		return -EMSGSIZE;
	*/

	if (ksettings->lanes &&
	    nla_put_u32(skb, ETHTOOL_A_LINKMODES_LANES, ksettings->lanes))
		return -EMSGSIZE;

	return 0;
}

static int ethnl_port_parse_request(struct ethnl_req_info *req_base,
				    struct nlattr **tb)
{
	struct port_req_info *req_info = PORT_REQINFO(req_base);

	if (tb[ETHTOOL_A_PORT_ID])
		req_info->port_id = nla_get_u32(tb[ETHTOOL_A_PORT_ID]);

	return 0;
}

int ethnl_port_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct port_req_info req_info = {};
	struct port_reply_data reply = {};
	struct nlattr **tb = info->attrs;
	struct sk_buff *rskb;
	void *reply_payload;
	int reply_len;
	int ret;

	ret = ethnl_parse_header_dev_get(&req_info.base,
					 tb[ETHTOOL_A_PORT_HEADER],
					 genl_info_net(info), info->extack,
					 true);
	if (ret < 0)
		return ret;

	rtnl_lock();

	ret = ethnl_port_parse_request(&req_info.base, tb);
	if (ret < 0)
		goto err_unlock_rtnl;

	/* No port, return early */
	if (!req_info.port_id)
		goto err_unlock_rtnl;

	ret =  ethnl_port_prepare_data(&req_info.base, &reply.base, info);
	if (ret)
		goto err_unlock_rtnl;

	ret = ethnl_port_reply_size(&req_info.base, &reply);
	if (ret < 0)
		goto err_unlock_rtnl;
	reply_len = ret + ethnl_reply_header_size();

	rskb = ethnl_reply_init(reply_len, req_info.base.dev,
				ETHTOOL_MSG_PORT_GET_REPLY,
				ETHTOOL_A_PORT_HEADER,
				info, &reply_payload);
	if (!rskb) {
		ret = -ENOMEM;
		goto err_unlock_rtnl;
	}

	ret = ethnl_port_fill_reply(&req_info.base, &reply, rskb);
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
}

struct ethnl_port_dump_ctx {
	struct port_req_info	*port_req_info;
	unsigned long ifindex;
	unsigned long port_id;
};

int ethnl_port_start(struct netlink_callback *cb)
{
	const struct genl_dumpit_info *info = genl_dumpit_info(cb);
	struct ethnl_port_dump_ctx *ctx = (void *)cb->ctx;
	struct nlattr **tb = info->info.attrs;
	int ret;

	BUILD_BUG_ON(sizeof(*ctx) > sizeof(cb->ctx));

	ctx->port_req_info = kzalloc(sizeof(*ctx->port_req_info), GFP_KERNEL);
	if (!ctx->port_req_info)
		return -ENOMEM;

	ret = ethnl_parse_header_dev_get(&ctx->port_req_info->base,
					 tb[ETHTOOL_A_PORT_HEADER],
					 sock_net(cb->skb->sk), cb->extack,
					 false);
	ctx->ifindex = 0;
	ctx->port_id = 0;
	return ret;
}

int ethnl_port_done(struct netlink_callback *cb)
{
	struct ethnl_port_dump_ctx *ctx = (void *)cb->ctx;

	ethnl_parse_header_dev_put(&ctx->port_req_info->base);
	kfree(ctx->port_req_info);

	return 0;
}

static int ethnl_port_dump_one_dev(struct sk_buff *skb, struct net_device *dev,
				   struct netlink_callback *cb)
{
	struct ethnl_port_dump_ctx *ctx = (void *)cb->ctx;
	struct port_req_info *pri = ctx->port_req_info;
	struct phy_port *port;
	int ret = 0;
	void *ehdr;

	pri->base.dev = dev;
	if (!dev->link_topo)
		return 0;

	xa_for_each_start(&dev->link_topo->ports, ctx->port_id, port, ctx->port_id) {
		struct port_reply_data reply = {};

		pri->port_id = ctx->port_id;

		ehdr = ethnl_dump_put(skb, cb, ETHTOOL_MSG_PORT_GET_REPLY);
		if (!ehdr) {
			ret = -EMSGSIZE;
			break;
		}

		/*
		ret =  ethnl_port_prepare_data(&pri->base, &reply.base, info);
		if (ret) {
			genlmsg_cancel(skb, ehdr);
			break;
		}
		*/

		ret = ethnl_fill_reply_header(skb, dev, ETHTOOL_A_PORT_HEADER);
		if (ret < 0) {
			genlmsg_cancel(skb, ehdr);
			break;
		}

		ret = ethnl_port_fill_reply(&pri->base, &reply, skb);
		if (ret) {
			genlmsg_cancel(skb, ehdr);
			break;
		}

		genlmsg_end(skb, ehdr);
	}

	return ret;
}

int ethnl_port_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct ethnl_port_dump_ctx *ctx = (void *)cb->ctx;
	struct net *net = sock_net(skb->sk);
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();

	if (ctx->port_req_info->base.dev) {
		ret = ethnl_port_dump_one_dev(skb, ctx->port_req_info->base.dev, cb);
	} else {
		for_each_netdev_dump(net, dev, ctx->ifindex) {
			ret = ethnl_port_dump_one_dev(skb, dev, cb);
			if (ret)
				break;

			ctx->port_id = 0;
		}
		ctx->port_req_info->base.dev = NULL;
	}
	rtnl_unlock();

	if (ret == -EMSGSIZE && skb->len)
		return skb->len;

	return ret;
}

const struct nla_policy ethnl_port_set_policy[] = {
	[ETHTOOL_A_PORT_HEADER]		=
		NLA_POLICY_NESTED(ethnl_header_policy),
	[ETHTOOL_A_PORT_ID]		= NLA_POLICY_MIN(NLA_U32, 1),
	[ETHTOOL_A_PORT_ENABLED]	= NLA_POLICY_MAX(NLA_U8, 1),
	[ETHTOOL_A_PORT_FORCED]		= NLA_POLICY_MAX(NLA_U8, 1),
};

static int
ethnl_set_port_validate(struct ethnl_req_info *req_base, struct genl_info *info)
{
	return 1;
}

static int ethnl_set_port(struct ethnl_req_info *req_info, struct genl_info *info)
{
	struct phy_port_state state = {};
	struct nlattr **tb = info->attrs;
	struct phy_port *port = NULL;
	struct phy_device *phydev;
	bool mod = false;
	u32 port_id = 0;

	phydev = ethnl_req_get_phydev(req_info, tb[ETHTOOL_A_PHY_HEADER],
				      info->extack);

	if (tb[ETHTOOL_A_PORT_ID])
		port_id = nla_get_u32(tb[ETHTOOL_A_PORT_ID]);

	if (port_id) {
		port = phy_link_topo_get_port(req_info->dev->link_topo, port_id);
	} else if (!IS_ERR_OR_NULL(phydev)) {
		port = phy_get_single_port(phydev);
	} else if (req_info->dev->sfp_bus) {
		port = sfp_get_port(req_info->dev->sfp_bus);
	} else {
		/* TODO PHY-less SFP-less ports */
		port = NULL;
	}

	if (!port)
		return -ENODEV;

	if (phy_port_get_state(port, &state))
		return -EOPNOTSUPP;

	ethnl_update_bool(&state.forced, tb[ETHTOOL_A_PHY_ISOLATE], &mod);

	if (!mod)
		return 0;

	return phy_port_set_state(port, &state);
}

const struct ethnl_request_ops ethnl_port_request_ops = {

	.hdr_attr		= ETHTOOL_A_PORT_HEADER,
	/* the GET/DUMP operations are implemented separately due to the
	 * ability to filter DUMP requests per netdev
	 */
	.set_validate		= ethnl_set_port_validate,
	.set			= ethnl_set_port,
	.set_ntf_cmd		= ETHTOOL_MSG_PORT_NTF,
};

