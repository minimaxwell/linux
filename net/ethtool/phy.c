// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Bootlin
 *
 */
#include "common.h"
#include "netlink.h"

#include <linux/link_topology.h>
#include <linux/phy.h>
#include <linux/sfp.h>

struct phy_req_info {
	struct ethnl_req_info		base;
	struct phy_device_node		pdn;
};

#define PHY_REQINFO(__req_base) \
	container_of(__req_base, struct phy_req_info, base)

const struct nla_policy ethnl_phy_get_policy[ETHTOOL_A_PHY_INDEX + 1] = {
	[ETHTOOL_A_PHY_HEADER] = NLA_POLICY_NESTED(ethnl_header_policy),
};

/* Caller holds rtnl */
static ssize_t
ethnl_phy_reply_size(const struct ethnl_req_info *req_base,
		     struct netlink_ext_ack *extack)
{
	struct phy_device_node *pdn;
	struct phy_device *phydev;
	struct link_topology *lt;
	long unsigned int index;
	size_t size = 0;

	lt = &req_base->dev->link_topo;

	size = nla_total_size(0);

	xa_for_each(&lt->phys, index, pdn) {
		phydev = pdn->phy;

		size += nla_total_size(sizeof(u32));	/* ETHTOOL_A_PHY_INDEX */
		size += ethnl_strz_size(phydev->drv->name);	/* ETHTOOL_A_DRVNAME */
		size += ethnl_strz_size(dev_name(&phydev->mdio.dev));	/* ETHTOOL_A_NAME */
		size += nla_total_size(sizeof(u8));	/* ETHTOOL_A_PHY_UPSTREAM_TYPE */
		size += nla_total_size(sizeof(u32));	/* ETHTOOL_A_PHY_ID */

		if (phy_on_sfp(phydev)) {
			if (sfp_get_name(phydev->sfp_bus))
				size += ethnl_strz_size(sfp_get_name(phydev->sfp_bus)); /* ETHTOOL_A_PHY_UPSTREAM_SFP_NAME */
			size += nla_total_size(sizeof(u32)); /* ETHTOOL_A_PHY_UPSTREAM_INDEX */
		}
	}

	return size;
}

static int
ethnl_phy_fill_reply(const struct ethnl_req_info *req_base, struct sk_buff *skb)
{
	struct phy_req_info *req_info = PHY_REQINFO(req_base);
	struct phy_device_node *pdn = &req_info->pdn;
	struct phy_device *phydev = pdn->phy;
	enum phy_upstream ptype;
	struct nlattr *nest;

	if (phy_on_sfp(phydev))
		ptype = PHY_UPSTREAM_SFP;
	else if (phydev->attached_dev)
		ptype = PHY_UPSTREAM_MAC;
	else
		ptype = PHY_UPSTREAM_PHY;

	if (nla_put_u32(skb, ETHTOOL_A_PHY_INDEX, phydev->phyindex) ||
	    ethnl_put_strz(skb, ETHTOOL_A_PHY_DRVNAME, phydev->drv->name) ||
	    ethnl_put_strz(skb, ETHTOOL_A_PHY_NAME, dev_name(&phydev->mdio.dev) ? dev_name(&phydev->mdio.dev) : "noname") ||
	    nla_put_u8(skb, ETHTOOL_A_PHY_UPSTREAM_TYPE, ptype) ||
	    nla_put_u32(skb, ETHTOOL_A_PHY_ID, phydev->phy_id))
		return -EMSGSIZE;

	if (ptype == PHY_UPSTREAM_PHY || ptype == PHY_UPSTREAM_SFP) {
		struct phy_device *upstream = pdn->upstream.phydev;
		nest = nla_nest_start(skb, ETHTOOL_A_PHY_UPSTREAM);
		if (!nest)
			return -EMSGSIZE;
		/* Parent index */
		if (nla_put_u32(skb, ETHTOOL_A_PHY_UPSTREAM_INDEX, upstream->phyindex))
			return -EMSGSIZE;


		if (pdn->parent_sfp_bus && sfp_get_name(pdn->parent_sfp_bus) &&
		    ethnl_put_strz(skb, ETHTOOL_A_PHY_UPSTREAM_SFP_NAME,
				   sfp_get_name(pdn->parent_sfp_bus)))
			return -EMSGSIZE;

		nla_nest_end(skb, nest);
	}

	if (phydev->sfp_bus)
		pr_info("%s : inserting downstream sfp name (%s)\n", __func__, sfp_get_name(phydev->sfp_bus));

	if (phydev->sfp_bus &&
	    ethnl_put_strz(skb, ETHTOOL_A_PHY_DOWNSTREAM_SFP_NAME, sfp_get_name(phydev->sfp_bus)))
		return -EMSGSIZE;

	return 0;
}

static int ethnl_phy_parse_request(struct ethnl_req_info *req_base,
				   struct nlattr **tb)
{
	struct phy_req_info *req_info = PHY_REQINFO(req_base);
	struct link_topology *lt = &req_base->dev->link_topo;
	struct phy_device_node *pdn;

	if (!req_base->phydev)
		return -EINVAL;

	pdn = xa_load(&lt->phys, req_base->phydev->phyindex);
	memcpy(&req_info->pdn, pdn, sizeof(*pdn));

	return 0;
}

int ethnl_phy_doit(struct sk_buff *skb, struct genl_info *info)
{
	struct phy_req_info req_info = {};
	struct nlattr **tb = info->attrs;
	struct sk_buff *rskb;
	void *reply_payload;
	int reply_len;
	int ret;

	ret = ethnl_parse_header_dev_get(&req_info.base,
					 tb[ETHTOOL_A_PHY_HEADER],
					 genl_info_net(info), info->extack,
					 true, true);
	if (ret < 0) {
		pr_info("%s : Error parsing header\n", __func__);
		return ret;
	}

	rtnl_lock();

	ret = ethnl_phy_parse_request(&req_info.base, tb);
	if (ret < 0) {
		pr_info("%s : Error parsing request\n", __func__);
		goto err_unlock_rtnl;
	}

	/* No PHY, return early */
	if (!req_info.pdn.phy) {
		pr_info("%s : No phydev\n", __func__);
		goto err_unlock_rtnl;
	}

	ret = ethnl_phy_reply_size(&req_info.base, info->extack);
	if (ret < 0) {
		pr_info("%s : Error building size\n", __func__);
		goto err_unlock_rtnl;
	}
	reply_len = ret + ethnl_reply_header_size();

	rskb = ethnl_reply_init(reply_len, req_info.base.dev,
				ETHTOOL_MSG_PHY_GET_REPLY,
				ETHTOOL_A_PHY_HEADER,
				info, &reply_payload);
	if (!rskb) {
		pr_info("%s : Error building reply skb\n", __func__);
		ret = -ENOMEM;
		goto err_unlock_rtnl;
	}

	ret = ethnl_phy_fill_reply(&req_info.base, rskb);
	if (ret) {
		pr_info("%s : Error filling reply\n", __func__);
		goto err_free_msg;
	}

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

struct ethnl_phy_dump_ctx {
	struct ethnl_req_info	req_info;
};

int ethnl_phy_start(struct netlink_callback *cb)
{
	const struct genl_dumpit_info *info = genl_dumpit_info(cb);
	struct ethnl_phy_dump_ctx *ctx = (void *)cb->ctx;
	struct nlattr **tb = info->info.attrs;
	int ret;

	BUILD_BUG_ON(sizeof(*ctx) > sizeof(cb->ctx));

	memset(ctx, 0, sizeof(*ctx));

	ret = ethnl_parse_header_dev_get(&ctx->req_info,
					 tb[ETHTOOL_A_PHY_HEADER],
					 sock_net(cb->skb->sk), cb->extack,
					 false, false);
	return ret;
}

int ethnl_phy_dump_one_dev(struct sk_buff *skb, struct net_device *dev,
			   struct netlink_callback *cb)
{
	struct ethnl_phy_dump_ctx *ctx = (void *)cb->ctx;
	struct phy_req_info *pri = PHY_REQINFO(&ctx->req_info);
	struct phy_device_node *pdn;
	long unsigned int index = 1;
	void *ehdr;
	int ret;

	ctx->req_info.dev = dev;

	xa_for_each(&dev->link_topo.phys, index, pdn) {
		ehdr = ethnl_dump_put(skb, cb,
				      ETHTOOL_MSG_PHY_GET_REPLY);
		if (!ehdr) {
			ret = -EMSGSIZE;
			break;
		}

		ret = ethnl_fill_reply_header(skb, dev,
					      ETHTOOL_A_PHY_HEADER);
		if (ret < 0) {
			genlmsg_cancel(skb, ehdr);
			break;
		}

		memcpy(&pri->pdn, pdn, sizeof(*pdn));
		ret = ethnl_phy_fill_reply(&ctx->req_info, skb);

		genlmsg_end(skb, ehdr);
	}

	return ret;
}


int ethnl_phy_dumpit(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct ethnl_phy_dump_ctx *ctx = (void *)cb->ctx;
	struct net *net = sock_net(skb->sk);
	long unsigned int ifindex = 1;
	struct net_device *dev;
	int ret = 0;

	rtnl_lock();

	if (ctx->req_info.dev) {
		ret = ethnl_phy_dump_one_dev(skb, ctx->req_info.dev, cb);
	} else {
		for_each_netdev_dump(net, dev, ifindex) {
			ret = ethnl_phy_dump_one_dev(skb, dev, cb);
			if (ret)
				break;
		}
	}
	rtnl_unlock();

	if (ret == -EMSGSIZE && skb->len)
		return skb->len;
	return ret;
}

