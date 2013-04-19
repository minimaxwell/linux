/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/cache.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/hdlc.h>
#include <linux/mutex.h>
#include <linux/of_device.h>

#define M_DRV_PEF2256_VERSION	"0.1"
#define M_DRV_PEF2256_AUTHOR	"CHANTELAUZE Jerome - Avril 2013"

/* A preciser */
#define RX_TIMEOUT 500

struct pef2256_dev_priv {
	struct sk_buff *tx_skbuff;

	u16 rx_len;
	u8 rx_buff[2048];

	unsigned short encoding;
	unsigned short parity;
       	struct net_device *netdev;

	struct delayed_work rx_timeout_queue;
};
