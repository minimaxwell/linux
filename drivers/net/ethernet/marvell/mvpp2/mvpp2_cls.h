/* SPDX-License-Identifier: GPL-2.0 */
/*
 * RSS and Classifier definitions for Marvell PPv2 Network Controller
 *
 * Copyright (C) 2014 Marvell
 *
 * Marcin Wojtas <mw@semihalf.com>
 */

#ifndef _MVPP2_CLS_H_
#define _MVPP2_CLS_H_

#include "mvpp2.h"
#include "mvpp2_prs.h"

/* Classifier constants */
#define MVPP2_CLS_FLOWS_TBL_SIZE	512
#define MVPP2_CLS_FLOWS_TBL_DATA_WORDS	3
#define MVPP2_CLS_LKP_TBL_SIZE		64
#define MVPP2_CLS_RX_QUEUES		256

/* Classifier flow constants */

#define MVPP2_FLOW_N_FIELDS		4

enum mvpp2_cls_engine {
	MVPP22_CLS_ENGINE_C2 = 1,
	MVPP22_CLS_ENGINE_C3A,
	MVPP22_CLS_ENGINE_C3B,
	MVPP22_CLS_ENGINE_C4,
	MVPP22_CLS_ENGINE_C3HA = 6,
	MVPP22_CLS_ENGINE_C3HB = 7,
};

#define MVPP22_CLS_HEK_OPT_MAC_DA	BIT(0)
#define MVPP22_CLS_HEK_OPT_VLAN		BIT(1)
#define MVPP22_CLS_HEK_OPT_L3_PROTO	BIT(2)
#define MVPP22_CLS_HEK_OPT_IP4SA	BIT(3)
#define MVPP22_CLS_HEK_OPT_IP4DA	BIT(4)
#define MVPP22_CLS_HEK_OPT_IP6SA	BIT(5)
#define MVPP22_CLS_HEK_OPT_IP6DA	BIT(6)
#define MVPP22_CLS_HEK_OPT_L4SIP	BIT(7)
#define MVPP22_CLS_HEK_OPT_L4DIP	BIT(8)
#define MVPP22_CLS_HEK_N_FIELDS		9

#define MVPP22_CLS_HEK_L4_OPTS	(MVPP22_CLS_HEK_OPT_L4SIP | \
				 MVPP22_CLS_HEK_OPT_L4DIP)

#define MVPP22_CLS_HEK_IP4_2T	(MVPP22_CLS_HEK_OPT_IP4SA | \
				 MVPP22_CLS_HEK_OPT_IP4DA)

#define MVPP22_CLS_HEK_IP6_2T	(MVPP22_CLS_HEK_OPT_IP6SA | \
				 MVPP22_CLS_HEK_OPT_IP6DA)

/* The fifth tuple in "5T" is the L4_Info field */
#define MVPP22_CLS_HEK_IP4_5T	(MVPP22_CLS_HEK_IP4_2T | \
				 MVPP22_CLS_HEK_L4_OPTS)

#define MVPP22_CLS_HEK_IP6_5T	(MVPP22_CLS_HEK_IP6_2T | \
				 MVPP22_CLS_HEK_L4_OPTS)

enum mvpp2_cls_field_id {
	MVPP22_CLS_FIELD_MAC_DA = 0x03,
	MVPP22_CLS_FIELD_VLAN = 0x06,
	MVPP22_CLS_FIELD_L3_PROTO = 0x0f,
	MVPP22_CLS_FIELD_IP4SA = 0x10,
	MVPP22_CLS_FIELD_IP4DA = 0x11,
	MVPP22_CLS_FIELD_IP6SA = 0x17,
	MVPP22_CLS_FIELD_IP6DA = 0x1a,
	MVPP22_CLS_FIELD_L4SIP = 0x1d,
	MVPP22_CLS_FIELD_L4DIP = 0x1e,
};

enum mvpp2_cls_flow_seq {
	MVPP2_CLS_FLOW_SEQ_NORMAL = 0,
	MVPP2_CLS_FLOW_SEQ_FIRST1,
	MVPP2_CLS_FLOW_SEQ_FIRST2,
	MVPP2_CLS_FLOW_SEQ_LAST,
	MVPP2_CLS_FLOW_SEQ_MIDDLE
};

enum mvpp22_cls_flow_tag {
	MVPP22_CLS_FLOW_VLAN_ALWAYS_EXEC = 0,	/* Always execute the lookup */
	MVPP22_CLS_FLOW_VLAN_TAGGED1,		/* Exec if 1 tag */
	MVPP22_CLS_FLOW_VLAN_TAGGED2,		/* Exec if 2 tags */
	MVPP22_CLS_FLOW_VLAN_TAGGED3,		/* Exec if 3 tags */
	MVPP22_CLS_FLOW_VLAN_UNTAGGED,		/* Exec if no tag */
	MVPP22_CLS_FLOW_VLAN_TAGGED,		/* Exec if at least 1 tag */
	MVPP22_CLS_FLOW_VLAN_TAGGED01,		/* Exec if 0 or 1 tag */
	MVPP22_CLS_FLOW_VLAN_TAGGED23		/* Exec if 2 or 3 tags */
};

/* Classifier C2 engine constants */
#define MVPP22_CLS_C2_TCAM_EN(data)		((data) << 16)

enum mvpp22_cls_c2_action {
	MVPP22_C2_NO_UPD = 0,
	MVPP22_C2_NO_UPD_LOCK,
	MVPP22_C2_UPD,
	MVPP22_C2_UPD_LOCK,
};

enum mvpp22_cls_c2_fwd_action {
	MVPP22_C2_FWD_NO_UPD = 0,
	MVPP22_C2_FWD_NO_UPD_LOCK,
	MVPP22_C2_FWD_SW,
	MVPP22_C2_FWD_SW_LOCK,
	MVPP22_C2_FWD_HW,
	MVPP22_C2_FWD_HW_LOCK,
	MVPP22_C2_FWD_HW_LOW_LAT,
	MVPP22_C2_FWD_HW_LOW_LAT_LOCK,
};

#define MVPP2_CLS_C2_TCAM_WORDS			5
#define MVPP2_CLS_C2_ATTR_WORDS			5

struct mvpp2_cls_c2_entry {
	u32 index;
	/* TCAM lookup key */
	u32 tcam[MVPP2_CLS_C2_TCAM_WORDS];
	/* QoS table lookup configuration */
	u32 act_table;
	/* Actions to perform upon TCAM match */
	u32 act;
	/* Attributes relative to the actions to perform */
	u32 attr[MVPP2_CLS_C2_ATTR_WORDS];
};

/* Classifier C2 engine entries */
#define MVPP22_CLS_C2_N_ENTRIES		256

/* Number of per-port dedicated entries in the C2 TCAM */
#define MVPP22_CLS_C2_PORT_RANGE	8
#define MVPP22_CLS_C2_PORT_N_RFS	6

#define MVPP22_CLS_C2_PORT_FIRST(p)	(MVPP22_CLS_C2_N_ENTRIES - \
					((p) * MVPP22_CLS_C2_PORT_RANGE))
#define MVPP22_CLS_C2_RSS_ENTRY(p)	(MVPP22_CLS_C2_PORT_FIRST(p) - 1)
#define MVPP22_CLS_C2_QOS_ENTRY(p)	(MVPP22_CLS_C2_RSS_ENTRY(p) - 1)
#define MVPP22_CLS_C2_RFS_LOC(p, loc)	(MVPP22_CLS_C2_QOS_ENTRY(p) - (loc))

/* Packet flow ID */
enum mvpp2_prs_flow {
	MVPP2_FL_START = 8,
	MVPP2_FL_IP4_TCP_NF_UNTAG = MVPP2_FL_START,
	MVPP2_FL_IP4_UDP_NF_UNTAG,
	MVPP2_FL_IP4_TCP_NF_TAG,
	MVPP2_FL_IP4_UDP_NF_TAG,
	MVPP2_FL_IP6_TCP_NF_UNTAG,
	MVPP2_FL_IP6_UDP_NF_UNTAG,
	MVPP2_FL_IP6_TCP_NF_TAG,
	MVPP2_FL_IP6_UDP_NF_TAG,
	MVPP2_FL_IP4_TCP_FRAG_UNTAG,
	MVPP2_FL_IP4_UDP_FRAG_UNTAG,
	MVPP2_FL_IP4_TCP_FRAG_TAG,
	MVPP2_FL_IP4_UDP_FRAG_TAG,
	MVPP2_FL_IP6_TCP_FRAG_UNTAG,
	MVPP2_FL_IP6_UDP_FRAG_UNTAG,
	MVPP2_FL_IP6_TCP_FRAG_TAG,
	MVPP2_FL_IP6_UDP_FRAG_TAG,
	MVPP2_FL_IP4_UNTAG, /* non-TCP, non-UDP, same for below */
	MVPP2_FL_IP4_TAG,
	MVPP2_FL_IP6_UNTAG,
	MVPP2_FL_IP6_TAG,
	MVPP2_FL_NON_IP_UNTAG,
	MVPP2_FL_NON_IP_TAG,
	MVPP2_FL_LAST,
};

enum mvpp2_cls_lu_type {
	MVPP2_CLS_LU_ALL = 0,
	MVPP2_CLS_LU_TAGGED_ONLY,
};

/* LU Type defined for all engines, and specified in the flow table */
#define MVPP2_CLS_LU_TYPE_MASK			0x3f

#define MVPP2_N_FLOWS		(MVPP2_FL_LAST - MVPP2_FL_START)

struct mvpp2_cls_flow {
	/* The L2-L4 traffic flow type */
	int flow_type;

	/* The first id in the flow table for this flow */
	u16 flow_id;

	/* The supported HEK fields for this flow */
	u16 supported_hash_opts;

	/* The Header Parser result_info that matches this flow */
	struct mvpp2_prs_result_info prs_ri;
};

#define MVPP2_ENTRIES_PER_FLOW			(MVPP2_MAX_PORTS + 8)
#define MVPP2_FLOW_FIRST(id)			(((id) - MVPP2_FL_START) * MVPP2_ENTRIES_PER_FLOW)
#define MVPP2_FLOW_C2_QOS_ENTRY(id)		(MVPP2_FLOW_FIRST(id))
#define MVPP2_FLOW_C2_RSS_ENTRY(id)		(MVPP2_FLOW_C2_QOS_ENTRY(id) + 1)
#define MVPP2_PORT_FLOW_HASH_ENTRY(port, id)	(MVPP2_FLOW_C2_RSS_ENTRY(id) + \
						(port) + MVPP2_FLOW_C2_RSS_ENTRY(id))
#define MVPP2_FLOW_C2_RFS(id, rfs_n)		(MVPP2_PORT_FLOW_HASH_ENTRY((MVPP2_MAX_PORTS - 1), (id)) + (rfs_n) + 1)

struct mvpp2_cls_flow_entry {
	u32 index;
	u32 data[MVPP2_CLS_FLOWS_TBL_DATA_WORDS];
};

struct mvpp2_cls_lookup_entry {
	u32 lkpid;
	u32 way;
	u32 data;
};

int mvpp22_port_rss_init(struct mvpp2_port *port);

int mvpp22_port_rss_enable(struct mvpp2_port *port);
int mvpp22_port_rss_disable(struct mvpp2_port *port);

int mvpp22_port_rss_ctx_create(struct mvpp2_port *port, u32 *rss_ctx);
int mvpp22_port_rss_ctx_delete(struct mvpp2_port *port, u32 rss_ctx);

int mvpp22_port_rss_ctx_indir_set(struct mvpp2_port *port, u32 rss_ctx,
				  const u32 *indir);
int mvpp22_port_rss_ctx_indir_get(struct mvpp2_port *port, u32 rss_ctx,
				  u32 *indir);

int mvpp2_ethtool_rxfh_get(struct mvpp2_port *port, struct ethtool_rxnfc *info);
int mvpp2_ethtool_rxfh_set(struct mvpp2_port *port, struct ethtool_rxnfc *info);

void mvpp2_cls_init(struct mvpp2 *priv);

void mvpp2_cls_port_config(struct mvpp2_port *port);

void mvpp2_cls_oversize_rxq_set(struct mvpp2_port *port);

int mvpp2_cls_flow_eng_get(struct mvpp2_cls_flow_entry *fe);

u16 mvpp2_flow_get_hek_fields(struct mvpp2_cls_flow_entry *fe);

const struct mvpp2_cls_flow *mvpp2_cls_flow_get(int flow);

u32 mvpp2_cls_flow_hits(struct mvpp2 *priv, int index);

void mvpp2_cls_flow_read(struct mvpp2 *priv, int index,
			 struct mvpp2_cls_flow_entry *fe);

u32 mvpp2_cls_lookup_hits(struct mvpp2 *priv, int index);

void mvpp2_cls_lookup_read(struct mvpp2 *priv, int lkpid, int way,
			   struct mvpp2_cls_lookup_entry *le);

u32 mvpp2_cls_c2_hit_count(struct mvpp2 *priv, int c2_index);

void mvpp2_cls_c2_read(struct mvpp2 *priv, int index,
		       struct mvpp2_cls_c2_entry *c2);

int mvpp2_ethtool_cls_rule_ins(struct mvpp2_port *port,
			       struct ethtool_rxnfc *info);

int mvpp2_ethtool_cls_rule_del(struct mvpp2_port *port,
			       struct ethtool_rxnfc *info);

#endif
