/* TODO
 *
 */

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <net/arp.h>
#include <linux/inetdevice.h>
#include <linux/if_vlan.h>
#include <linux/syscalls.h>

#include <saf3000/ldb_gpio.h>

#if defined(CONFIG_FS_ENET)
	#include "../ethernet/freescale/fs_enet/fs_enet.h"
#elif defined(CONFIG_UCC_GETH)
	#include "../ethernet/freescale/ucc_geth.h"		
#endif

#include <linux/custom_phy_handling.h>

int active_phy = 0;
int phy_ghost_ready = 0;

//FIXME
static inline int phy_aneg_done(struct phy_device *phydev)
{
	if (phydev->drv->aneg_done)
		return phydev->drv->aneg_done(phydev);

	return genphy_aneg_done(phydev);
}



/* sysfs hook function */
static ssize_t fs_attr_active_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif

	return sprintf(buf, "%d\n",ndev_priv->phydev == ndev_priv->custom_hdlr->phydevs[1]?1:0);
}

static ssize_t fs_attr_active_link_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	int active = simple_strtol(buf, NULL, 10);

	if (active != 1) active = 0;
	if (!ndev_priv->custom_hdlr->phydevs[active]) {
		dev_warn(dev, "PHY on address %d does not exist\n", active);
		return count;
	}
	printk(KERN_ERR"doing stuff in fs_attr_active_link_store\n");
	if (ndev_priv->custom_hdlr->phydevs[active] != ndev_priv->phydev) {
		ndev_priv->custom_hdlr->link_switch(ndev_priv->ndev);
		cancel_delayed_work_sync(&ndev_priv->link_queue);
		if (!ndev_priv->phydev->link && ndev_priv->custom_hdlr->mode == MODE_AUTO) schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
	}
	
	return count;
}
	
static DEVICE_ATTR(active_link, S_IRUGO | S_IWUSR, fs_attr_active_link_show, fs_attr_active_link_store);

static ssize_t fs_attr_phy0_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ctrl;
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif

	ctrl = phy_read(ndev_priv->custom_hdlr->phydevs[0], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : ndev_priv->custom_hdlr->phydevs[0]->link ? 2 : 1);
}

static DEVICE_ATTR(phy0_link, S_IRUGO, fs_attr_phy0_link_show, NULL);

static ssize_t fs_attr_phy1_link_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ctrl;
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	
	ctrl = phy_read(ndev_priv->custom_hdlr->phydevs[1], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : ndev_priv->custom_hdlr->phydevs[1]->link ? 2 : 1);
}

static DEVICE_ATTR(phy1_link, S_IRUGO, fs_attr_phy1_link_show, NULL);

static ssize_t fs_attr_phy0_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct phy_device *phydev = ndev_priv->custom_hdlr->phydevs[0];

	int mode = simple_strtol(buf, NULL, 10);
	
	if (mode && ndev_priv->phydev->drv->resume) phydev->drv->resume(phydev);
	if (!mode && ndev_priv->phydev->drv->suspend) phydev->drv->suspend(phydev);
	
	return count;
}

static DEVICE_ATTR(phy0_mode, S_IWUSR, NULL, fs_attr_phy0_mode_store);

static ssize_t fs_attr_phy1_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct phy_device *phydev = ndev_priv->custom_hdlr->phydevs[1];

	int mode = simple_strtol(buf, NULL, 10);
	
	if (mode && ndev_priv->phydev->drv->resume) phydev->drv->resume(phydev);
	if (!mode && ndev_priv->phydev->drv->suspend) phydev->drv->suspend(phydev);
	
	return count;
}

static DEVICE_ATTR(phy1_mode, S_IWUSR, NULL, fs_attr_phy1_mode_store);

static ssize_t fs_attr_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif

	return sprintf(buf, "%d\n",ndev_priv->custom_hdlr->mode);
}

static ssize_t fs_attr_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct net_device *ndev = dev_get_drvdata(dev);
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	int mode = simple_strtol(buf, NULL, 10);
	
	printk(KERN_ERR"doing stuff in fs_attr_mode_store\n");
	if (ndev_priv->custom_hdlr->mode != mode) {
		ndev_priv->custom_hdlr->mode = mode;
		if (mode == MODE_AUTO) {
			cancel_delayed_work_sync(&ndev_priv->link_queue);
			schedule_delayed_work(&ndev_priv->link_queue, 0);
		}
	}
	
	return count;
}
	
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR, fs_attr_mode_show, fs_attr_mode_store);

/**
 * phy_create_files() - Create files to report phy additional states.
 * @ndev: Pointer to associated net device.
 *
 * Create all files needed to report additionnal states related to
 * the handling two phy associated to only one net device.
 *
 * Return: return an error if one the device_create_file failed.
 */
int phy_create_files(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	int ret = 0; 

	ret |= device_create_file(ndev_priv->dev, &dev_attr_active_link);
	ret |= device_create_file(ndev_priv->dev, &dev_attr_phy0_link);
#if defined(CONFIG_FS_ENET)
	if (ndev_priv->fpi->phy_node2)
#elif defined(CONFIG_UCC_GETH)
	if (ndev_priv->ug_info->phy_node2)
#endif
		ret |= device_create_file(ndev_priv->dev, &dev_attr_phy1_link);
	ret |= device_create_file(ndev_priv->dev, &dev_attr_phy0_mode);
#if defined(CONFIG_FS_ENET)
	if (ndev_priv->fpi->phy_node2)
#elif defined(CONFIG_UCC_GETH)
	if (ndev_priv->ug_info->phy_node2)
#endif
		ret |= device_create_file(ndev_priv->dev, &dev_attr_phy1_mode);
	ret |= device_create_file(ndev_priv->dev, &dev_attr_mode);

	return ret;
}
EXPORT_SYMBOL(phy_create_files);

/**
 * phy_delete_files() - Delete files to report phy additional states.
 * @ndev: Pointer to associated net device.
 *
 * Create all files needed to report additionnal states related to
 * the handling two phy associated to only one net device.
 *
 */
void phy_delete_files(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif

	device_remove_file(ndev_priv->dev, &dev_attr_active_link);
	device_remove_file(ndev_priv->dev, &dev_attr_phy0_link);
	device_remove_file(ndev_priv->dev, &dev_attr_phy1_link);
	device_remove_file(ndev_priv->dev, &dev_attr_phy0_mode);
	device_remove_file(ndev_priv->dev, &dev_attr_phy1_mode);
	device_remove_file(ndev_priv->dev, &dev_attr_mode);
}
EXPORT_SYMBOL(phy_delete_files);

void fs_sysfs_notify(struct work_struct *work)
{
	struct fs_notify_work *notify =
			container_of(work, struct fs_notify_work, notify_queue);

	sysfs_notify_dirent(notify->kn);
}
EXPORT_SYMBOL(fs_sysfs_notify);

void fs_send_gratuitous_arp(struct work_struct *work)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv =
			container_of(work, struct fs_enet_private, arp_queue);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv =
			container_of(work, struct ucc_geth_private, arp_queue);
#endif

	__be32 ip_addr;
	struct sk_buff *skb;

	int fd, i = 0, nb_line = 0, ret, j, k = 0;
	char buf[300];
	char lines[10][50];
	char iface_and_vlan[10];
	int id;
	char iface[10];
	char sanitized_iface[5] ;

	ip_addr = inet_select_addr(ndev_priv->ndev, 0, 0);
	skb = arp_create(ARPOP_REPLY, ETH_P_ARP, ip_addr, ndev_priv->ndev, ip_addr, NULL,
			ndev_priv->ndev->dev_addr, NULL);
	if (skb == NULL)
		printk("arp_create failure -> gratuitous arp not sent\n");
	else {
		mm_segment_t old_fs = get_fs();
		//send skb on eth1
		arp_xmit(skb);
		//dev_err(ndev_priv->dev, "gratuitous arp sent\n");

		//find all vlan on eth1
		//FIXME : is there a clean way ? 
	
		set_fs(KERNEL_DS);

		fd = sys_open("/proc/net/vlan/config", O_RDONLY, 0);
		if (fd >= 0) {
			while (sys_read(fd, &buf[i], 1) == 1 ) {
				//remove header (2 lines)
				//VLAN Dev name    | VLAN ID \n
				//Name-Type: VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD \n
				if (buf[i] == '\n')
					nb_line++;
				if (nb_line >= 2)
					i++;
			}
			//split on each line
			nb_line = 0;
			for (j=1;j<i;j++) {
				if (buf[j] == '\n') {
					lines[nb_line][k] = '\n';
					nb_line++;
					k = 0;
				}
				lines[nb_line][k] = buf[j];
				k++;
			}
			
			//each line is something like :
			//eth1.64        | 64  | eth1\n
			for (i = 0; i < nb_line; i++) {
				//dev_err(ndev_priv->dev, lines[i]);
				ret = sscanf(lines[i], "\n%s %*s %d %*s %s", iface_and_vlan, &id, iface); 
				if (ret == 3) {
					//dev_err(ndev_priv->dev, "\n----\nret : %d\n----\niface_and_vlan : *%s*\nid: *%d*\n\niface *%s*\n----\n",
					//		 ret, iface_and_vlan, id, iface);
					//if iface = eth1, send arp on corresponding vlan
					strcpy(sanitized_iface,iface);
					sanitized_iface[4] = '\0';
					if (strcmp("eth1", sanitized_iface) == 0) {
						skb = arp_create(ARPOP_REPLY, ETH_P_ARP, ip_addr,
								ndev_priv->ndev, ip_addr,
								NULL,
								ndev_priv->ndev->dev_addr,
								NULL);
						skb = vlan_insert_tag_set_proto(skb, htons(ETH_P_8021Q), id);
						if (!skb) { 
							dev_err(ndev_priv->dev, "failed to insert VLAN tag -> gratuitous arp not sent\n");
						}
						arp_xmit(skb);
						//dev_err(ndev_priv->dev, "gratuitous arp sent on %s.%d\n", iface, id);
					}
				}
			}
			sys_close(fd);
		}
		set_fs(old_fs);
	}
}
EXPORT_SYMBOL(fs_send_gratuitous_arp);

void fs_link_switch(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct phy_device *phydev = ndev_priv->phydev;
	unsigned long flags;
	int value;

	/* Do not suspend the PHY, but isolate it. We can't get a powered 
	   down PHY's link status */
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) | BMCR_ISOLATE));

	if (phydev == ndev_priv->custom_hdlr->phydevs[0]) {
		/* MCR3000_2G Front side eth connector */
		if (ndev_priv->custom_hdlr->use_PHY5) {
			/* Switch off PHY 1 */
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
			/* Unisolate PHY 5 */
			phydev=ndev_priv->custom_hdlr->phydevs[1];
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, 
				((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
		} 

		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 1);

		phydev = ndev_priv->custom_hdlr->phydevs[1];
		dev_err(ndev_priv->dev, "Switch to PHY B\n");
		/* In the open function, autoneg was disabled for PHY B.
		   It must be enabled when PHY B is activated for the 
		   first time. */
		phydev->autoneg = AUTONEG_ENABLE;
	}
	else {
		/* MCR3000_2G Front side eth connector */
		if (ndev_priv->custom_hdlr->use_PHY5) {
			/* isolate PHY 5 */
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
			/* Switch on PHY 1 */
			phydev=ndev_priv->custom_hdlr->phydevs[0];
			value = phy_read(phydev, MII_BMCR);
			phy_write(phydev, MII_BMCR, 
				((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
		}
		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 0);
		phydev = ndev_priv->custom_hdlr->phydevs[0];
		dev_err(ndev_priv->dev, "Switch to PHY A\n");
	}

	/* Active phy has changed -> notify user space */
	schedule_work(&ndev_priv->notify_work[ACTIVE_LINK].notify_queue);

	spin_lock_irqsave(&ndev_priv->lock, flags);
	ndev_priv->phydev = phydev;
	ndev_priv->custom_hdlr->change_time = jiffies;
	spin_unlock_irqrestore(&ndev_priv->lock, flags);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
	if (phydev->drv->config_aneg)
		phydev->drv->config_aneg(phydev);

	if (ndev_priv->phydev->link)
	{
		netif_carrier_on(ndev_priv->phydev->attached_dev);

		/* Send gratuitous ARP */
		printk(KERN_ERR"arp stuff\n");
		schedule_work(&ndev_priv->arp_queue);
	}
}
EXPORT_SYMBOL(fs_link_switch);

// #define DOUBLE_ATTACH_DEBUG 1

void fs_link_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
#if defined(CONFIG_FS_ENET)	
	struct fs_enet_private *ndev_priv =
			container_of(dwork, struct fs_enet_private, link_queue);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv =
                     container_of(dwork, struct ucc_geth_private, link_queue);
#endif
	struct phy_device *phydev = ndev_priv->phydev;
	struct phy_device *changed_phydevs[2] = {NULL, NULL};
	int nb_changed_phydevs = 0;

	#ifdef DOUBLE_ATTACH_DEBUG
	int value;	
	printk("Current PHY : %s\n", ndev_priv->phydev==ndev_priv->custom_hdlr->phydevs[0] ? "PHY 0": "PHY 1");
	printk("PHY 0 (addr=%d) : %s ", ndev_priv->custom_hdlr->phydevs[0]->addr, ndev_priv->custom_hdlr->phydevs[0]->link ? "Up": "Down");
	value = phy_read(ndev_priv->custom_hdlr->phydevs[0], MII_BMSR);
	if (value & 0x0004)
		printk ("(Up in MII_BMSR) ");
	else
		printk ("(Down in MII_BMSR) ");
	value = phy_read(ndev_priv->custom_hdlr->phydevs[0], MII_BMCR);
	if (value & 0x0400) 
		printk ("isolated\n");
	else
		printk ("not isolated\n");
	#endif
	if (ndev_priv->custom_hdlr->phydevs[0]->state != PHY_RUNNING && ndev_priv->custom_hdlr->phydevs[1] && ndev_priv->custom_hdlr->phydevs[1]->state == PHY_DOUBLE_ATTACHEMENT) {
		ndev_priv->custom_hdlr->phydevs[1]->state = PHY_RUNNING;
	#ifdef DOUBLE_ATTACH_DEBUG
		dev_err(ndev_priv->dev, "PHY is now running \n");
	#endif
	}

	/* If we are not in AUTO mode, don't do anything */
	if (ndev_priv->custom_hdlr->mode != MODE_AUTO) return;
	
	/* If elapsed time since last change is too small, wait for a while */
	if (jiffies - ndev_priv->custom_hdlr->change_time < LINK_MONITOR_RETRY) {
		schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
		return;
	}

	/* Which phydev(s) has changed ? */
	if (ndev_priv->custom_hdlr->phydevs[0]->link != ndev_priv->custom_hdlr->phy_oldlinks[0]) {
		changed_phydevs[nb_changed_phydevs++] = ndev_priv->custom_hdlr->phydevs[0];
		ndev_priv->custom_hdlr->phy_oldlinks[0] = ndev_priv->custom_hdlr->phydevs[0]->link;
		/* phyA link status has changed -> notify user space */
		schedule_work(&ndev_priv->notify_work[PHY0_LINK].notify_queue);

	} 
	if (ndev_priv->custom_hdlr->phydevs[1]->link != ndev_priv->custom_hdlr->phy_oldlinks[1]) {
		changed_phydevs[nb_changed_phydevs++] = ndev_priv->custom_hdlr->phydevs[1];
		ndev_priv->custom_hdlr->phy_oldlinks[1] = ndev_priv->custom_hdlr->phydevs[1]->link;
		/* phyB link status has changed -> notify user space */
		schedule_work(&ndev_priv->notify_work[PHY1_LINK].notify_queue);
	}

	/* If nothing has changed, exit */
	if (! nb_changed_phydevs) {
		/* Check the consistency of the carrier before exiting */
		if (ndev_priv->phydev->link && ! netif_carrier_ok(ndev_priv->phydev->attached_dev))
			netif_carrier_on(ndev_priv->phydev->attached_dev);
		return;
	}


	/*
	 * If no one was in charge (no active link) PHY0 become the 
	 * active link but if either PHY0 or PHY1 was already active,
	 * don't change anything, we leave him in charge
	 */		
	if (ndev_priv->custom_hdlr->phydevs[1] && !ndev_priv->custom_hdlr->phy_oldlinks[1]) {
		if (ndev_priv->phydev != ndev_priv->custom_hdlr->phydevs[0] && ndev_priv->custom_hdlr->phydevs[1]->link && ndev_priv->custom_hdlr->phydevs[0]->link) {
			ndev_priv->custom_hdlr->link_switch(ndev_priv->ndev);
		}
	}
		
	
	/* If the active PHY has a link and carrier is off, 
	   call netif_carrier_on */
	if (phydev->link) {
		if (! netif_carrier_ok(ndev_priv->phydev->attached_dev))
			netif_carrier_on(ndev_priv->phydev->attached_dev);
		return;
	}

	/* If we reach this point, the active PHY has lost its link */
	/* If the other PHY has a link -> switch */
	if ((phydev == ndev_priv->custom_hdlr->phydevs[0] && ndev_priv->custom_hdlr->phydevs[1]->link) ||
	    (phydev == ndev_priv->custom_hdlr->phydevs[1] && ndev_priv->custom_hdlr->phydevs[0]->link))
	{
		ndev_priv->custom_hdlr->link_switch(ndev_priv->ndev);
	} else {
		ndev_priv->custom_hdlr->change_time = jiffies;
		schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
	}
}


/* MCR1G handlers */

#if defined(CONFIG_FS_ENET)
void mcr1g_link_switch(struct net_device *ndev)
{
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev_priv->phydev;
	unsigned long flags;

	/* The PHY must be powered down to disable it (mcr3000 1G) */
	if (phydev->drv->suspend) phydev->drv->suspend(phydev);

	if (phydev == ndev_priv->custom_hdlr->phydevs[0]) {
		phydev = ndev_priv->custom_hdlr->phydevs[1];
		dev_err(ndev_priv->dev, "Switch to PHYB\n");
	}
	else {
		phydev = ndev_priv->custom_hdlr->phydevs[0];
		dev_err(ndev_priv->dev, "Switch to PHYA\n");
	}

	/* Active phy has changed -> notify user space */
	schedule_work(&ndev_priv->notify_work[ACTIVE_LINK].notify_queue);

	spin_lock_irqsave(&ndev_priv->lock, flags);
	ndev_priv->custom_hdlr->change_time = jiffies;
	ndev_priv->phydev = phydev;
	spin_unlock_irqrestore(&ndev_priv->lock, flags);

	if (phydev->drv->resume) phydev->drv->resume(phydev);

	return;
}
EXPORT_SYMBOL(mcr1g_link_switch);

void mcr1g_link_monitor(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct fs_enet_private *ndev_priv =
                     container_of(dwork, struct fs_enet_private, link_queue);
	struct phy_device *phydev = ndev_priv->phydev;

	if (ndev_priv->custom_hdlr->phydevs[0]->state != PHY_RUNNING && ndev_priv->custom_hdlr->phydevs[1] && ndev_priv->custom_hdlr->phydevs[1]->state == PHY_DOUBLE_ATTACHEMENT) {
		ndev_priv->custom_hdlr->phydevs[1]->state = PHY_RUNNING;
	}

	/* If there's only one PHY -> Nothing to do */
	if (! ndev_priv->custom_hdlr->phydevs[1]) {
		if (!phydev->link && ndev_priv->custom_hdlr->mode == MODE_AUTO) 
			schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
		return;
	}

	if (!phydev->link && ndev_priv->custom_hdlr->mode == MODE_AUTO) {
		if (ndev_priv->custom_hdlr->phydevs[1] && 
		    (jiffies - ndev_priv->custom_hdlr->change_time >= LINK_MONITOR_RETRY))
			ndev_priv->custom_hdlr->link_switch(ndev_priv->ndev);
		schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
	}
	return;
}
EXPORT_SYMBOL(mcr1g_link_monitor);

void mcr1g_adjust_state(struct net_device *ndev)
{
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev_priv->phydev;
	int err = 0;
	
	switch(phydev->state) {
	case PHY_DOWN:
	case PHY_STARTING:
	case PHY_READY:
	case PHY_PENDING:
	case PHY_UP:
		break;
	case PHY_AN:
		err = phy_read_status(phydev);
		if (err < 0)
			break;
			
		/* Check if negotiation is done.  Break if there's an error */
		err = phy_aneg_done(phydev);
		if (err < 0)
			break;

		if (err > 0) 
			phydev->state = PHY_DOUBLE_ATTACHEMENT;
		break;
	case PHY_NOLINK:
	case PHY_FORCING:
	case PHY_RUNNING:
	case PHY_DOUBLE_ATTACHEMENT:
		phydev->state = PHY_CHANGELINK;
		break;
	case PHY_CHANGELINK:
	case PHY_HALTED:
	case PHY_RESUMING:
		break;
	}
}
EXPORT_SYMBOL(mcr1g_adjust_state);
#endif

#if defined(CONFIG_FS_ENET)
/* MCR2G handlers, not needed on mcrpro*/

void mcr2g_link_switch(struct net_device *ndev)
{
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct phy_device *phydev = ndev_priv->phydev;
	unsigned long flags;
	int value;

	/* Do not suspend the PHY, but isolate it. We can't get a powered 
	   down PHY's link status */
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) | BMCR_ISOLATE));

	if (phydev == ndev_priv->custom_hdlr->phydevs[0]) {
		/* MCR3000_2G Front side eth connector */
		/* Switch off PHY 1 */
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
		/* Unisolate PHY 5 */
		phydev=ndev_priv->custom_hdlr->phydevs[1];
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, 
			((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));

		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 1);

		phydev = ndev_priv->custom_hdlr->phydevs[1];
		dev_err(ndev_priv->dev, "Switch to PHY B\n");
		/* In the open function, autoneg was disabled for PHY B.
		   It must be enabled when PHY B is activated for the 
		   first time. */
		phydev->autoneg = AUTONEG_ENABLE;
	}
	else {
		/* MCR3000_2G Front side eth connector */
		/* isolate PHY 5 */
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, value | BMCR_ISOLATE);
		/* Switch on PHY 1 */
		phydev=ndev_priv->custom_hdlr->phydevs[0];
		value = phy_read(phydev, MII_BMCR);
		phy_write(phydev, MII_BMCR, 
			((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 0);
		phydev = ndev_priv->custom_hdlr->phydevs[0];
		dev_err(ndev_priv->dev, "Switch to PHY A\n");
	}

	/* Active phy has changed -> notify user space */
	schedule_work(&ndev_priv->notify_work[ACTIVE_LINK].notify_queue);

	spin_lock_irqsave(&ndev_priv->lock, flags);
	ndev_priv->phydev = phydev;
	ndev_priv->custom_hdlr->change_time = jiffies;
	spin_unlock_irqrestore(&ndev_priv->lock, flags);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
	if (phydev->drv->config_aneg)
		phydev->drv->config_aneg(phydev);

	if (ndev_priv->phydev->link)
	{
		netif_carrier_on(ndev_priv->phydev->attached_dev);

		/* Send gratuitous ARP */
		schedule_work(&ndev_priv->arp_queue);
	}
}
EXPORT_SYMBOL(mcr2g_link_switch);
#endif
	
/* MIAE handlers */

void miae_link_switch(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct phy_device *phydev = ndev_priv->phydev;
	unsigned long flags;
	int value;

	/* Do not suspend the PHY, but isolate it. We can't get a powered 
	   down PHY's link status */
	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) | BMCR_ISOLATE));

	if (phydev == ndev_priv->custom_hdlr->phydevs[0]) {
		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 1);

		phydev = ndev_priv->custom_hdlr->phydevs[1];
		dev_err(ndev_priv->dev, "Switch to PHY B\n");
		/* In the open function, autoneg was disabled for PHY B.
		   It must be enabled when PHY B is activated for the 
		   first time. */
		phydev->autoneg = AUTONEG_ENABLE;
	}
	else {
		if (ndev_priv->custom_hdlr->gpio != -1) ldb_gpio_set_value(ndev_priv->custom_hdlr->gpio, 0);
		phydev = ndev_priv->custom_hdlr->phydevs[0];
		dev_err(ndev_priv->dev, "Switch to PHY A\n");
	}

	/* Active phy has changed -> notify user space */
	schedule_work(&ndev_priv->notify_work[ACTIVE_LINK].notify_queue);

	spin_lock_irqsave(&ndev_priv->lock, flags);
	ndev_priv->phydev = phydev;
	ndev_priv->custom_hdlr->change_time = jiffies;
	spin_unlock_irqrestore(&ndev_priv->lock, flags);

	value = phy_read(phydev, MII_BMCR);
	phy_write(phydev, MII_BMCR, ((value & ~BMCR_PDOWN) & ~BMCR_ISOLATE));
	if (phydev->drv->config_aneg)
		phydev->drv->config_aneg(phydev);

	if (ndev_priv->phydev->link)
	{
		netif_carrier_on(ndev_priv->phydev->attached_dev);

		/* Send gratuitous ARP */
		schedule_work(&ndev_priv->arp_queue);
	}
}
EXPORT_SYMBOL(miae_link_switch);

void adjust_state(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct phy_device *phydev = ndev_priv->phydev;
	int err = 0;
	
	switch(phydev->state) {
	case PHY_DOWN:
	case PHY_STARTING:
	case PHY_READY:
	case PHY_PENDING:
	case PHY_UP:
		break;
	case PHY_AN:
		err = phy_read_status(phydev);
		if (err < 0)
			break;

		if (!phydev->link) {
			if (phydev->addr == PHYB_ADDR )
				phy_write(phydev, MII_BMCR, phy_read(phydev, MII_BMCR) | BMCR_ISOLATE);
			break;
		} 
			
		/* Check if negotiation is done.  Break if there's an error */
		err = phy_aneg_done(phydev);
		if (err < 0)
			break;

		if (err > 0) 
			phydev->state = PHY_CHANGELINK;
		break;
	case PHY_NOLINK:
		if (phy_interrupt_is_valid(phydev))
			break;

		err = phy_read_status(phydev);
		if (err)
			break;

		if (phydev->link) {
			if (active_phy == 0 || active_phy == phydev->addr)
			{
				if (active_phy == 0 && phydev->addr != 1)
					active_phy = phydev->addr;
			} else {
				phydev->state = PHY_DOUBLE_ATTACHEMENT;
			}
		}
		break;
	case PHY_FORCING:
		err = genphy_update_link(phydev);
		if (err)
			break;

		if (phydev->link) {
			phydev->state = PHY_CHANGELINK;
		}
		break;
	case PHY_RUNNING:
	case PHY_DOUBLE_ATTACHEMENT:
		if (phydev->addr != 1) {
			err = phy_read_status(phydev);

			if (err)
				break;

			if (!phydev->link) {
				phydev->state = PHY_NOLINK;
				break;
			}
		}
		/* Only register a CHANGE if we are
		 * polling or ignoring interrupts
		 */
		if (! (phy_read(phydev, MII_BMSR) & BMSR_ANEGCOMPLETE)) {
			phydev->autoneg = AUTONEG_ENABLE;
			phydev->state = PHY_AN;
		} else if (phy_ghost_ready == 0) { 
			/* set phy_ghost_ready to tell	  */
			/* back up PHY is ready (AN done) */	
			phy_ghost_ready = 1;
		}
		break;
	case PHY_CHANGELINK:
		err = phy_read_status(phydev);
		if (err)
			break;
		if (phydev->link) {
			if (active_phy == 0 || active_phy == phydev->addr || phydev->addr == 1) {
				if (active_phy == 0 && phydev->addr != 1)
					/* there was no active PHY,   */ 
					/* we become the active PHY   */
					active_phy = phydev->addr;
				if (phydev->addr != active_phy)
					 /* we are not the active_phy,  */ 
					 /* go to DOUBLE_ATTACHEMENT    */
					phydev->state = PHY_DOUBLE_ATTACHEMENT;
					netif_carrier_on(phydev->attached_dev);
				} 
		} else {
			if (active_phy == phydev->addr) {
				active_phy = 0;
				if (phy_ghost_ready)
					active_phy = (phydev->addr == PHYB_ADDR ? PHYA_ADDR : PHYB_ADDR);
			}
			/* if we lost link on PHY1 or/and PHY2,		*/
			/* change phy_ghost_ready to 0, since we'll use */
			/* back up PHY (if he has a link ...)		*/
			phy_ghost_ready = 0;
		}
		break;
	case PHY_HALTED:
	case PHY_RESUMING:
		break;
	}

}
EXPORT_SYMBOL(adjust_state);

/**
 * custom_probe() - setup custom handlers according to dtb.
 * @ndev: Pointer to associated net device.
 *
 * Check if the interface as registered two phydevices.
 * If this is the case, set up link_monitor,
 * link_switch and adjust_state function of the 
 * netdev accroding to the dtb. 
 *
 * If associated netdevice as to handle two phy_node 
 * set up custom_hdlr pointer in net_device according to
 * information contained in dtb (settings, associated board handler)
 * pointer will be NULL otherwise 
 */
void custom_probe(struct net_device *ndev, struct platform_device *ofdev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	int ngpios = of_gpio_count(ofdev->dev.of_node);
	int gpio = -1;
	char *Disable_PHY;
	char *use_PHY5;
	struct custom_phy_handler *custom_hdlr;

	custom_hdlr = kzalloc(sizeof(*custom_hdlr), GFP_KERNEL);
	if (!custom_hdlr)
		return -ENOMEM;

	if (ngpios == 1) 
		gpio = ldb_gpio_init(ofdev->dev.of_node, &ofdev->dev, 0, 1);

	custom_hdlr->gpio = gpio;
	
	custom_hdlr->link_monitor = fs_link_monitor;
	custom_hdlr->link_switch = fs_link_switch;
	custom_hdlr->disable_phy = 0;
	Disable_PHY = (char *)of_get_property(ofdev->dev.of_node, 
		"PHY-disable", NULL);
	if (Disable_PHY) {
		printk("PHY-disable = %s\n", Disable_PHY);
		if (! strcmp(Disable_PHY, "power-down")) {
			/* on MCR1G */
			custom_hdlr->disable_phy = PHY_POWER_DOWN;
			custom_hdlr->link_monitor = mcr1g_link_monitor;
			custom_hdlr->link_switch = mcr1g_link_switch;
			custom_hdlr->adjust_state = mcr1g_adjust_state;
		}
		else if (! strcmp(Disable_PHY, "isolate")) {
			custom_hdlr->disable_phy = PHY_ISOLATE;
			custom_hdlr->link_switch = miae_link_switch;
			custom_hdlr->adjust_state = adjust_state;
		}
	}

	custom_hdlr->use_PHY5 = 0;
	use_PHY5 = (char *)of_get_property(ofdev->dev.of_node, 
		"use-PHY5", NULL);
	if (use_PHY5) { 
		custom_hdlr->use_PHY5=1;
		custom_hdlr->link_switch = mcr2g_link_switch;
	}

	ndev_priv->custom_hdlr = custom_hdlr;
	return;
} 
EXPORT_SYMBOL(custom_probe);

/**
 * custom_init() - init second phy if needed.
 * @ndev: Pointer to associated net device.
 *
 * Initialise second phy on interface that use custom handlers
 * for networking
 * 	
 * return: in case of success, return 0 and ENODEV
 * if attaching to phy failed
 */
int custom_init(struct net_device *ndev, void (*hndlr)(struct net_device *), phy_interface_t iface)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct fs_platform_info *fpi = ndev_priv->fpi;
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
	struct ucc_geth_info *fpi = ndev_priv->ug_info;
#endif
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;

	/* if phy_node2 is NULL, leave the interface in normal operation */
	if (!fpi->phy_node2)
		return 0;

	custom_hdlr->change_time = jiffies;
	custom_hdlr->mode = MODE_AUTO;

	ndev_priv->phydev->adjust_state = custom_hdlr->adjust_state;
	
	custom_hdlr->phydevs[0] = ndev_priv->phydev;
	custom_hdlr->phy_oldlinks[0] = 0;
	
	custom_hdlr->phydevs[1] = of_phy_connect(ndev, ndev_priv->fpi->phy_node2, 
				hndlr, 0, iface);
	if (!custom_hdlr->phydevs[1]) {
		dev_err(&ndev->dev, "Could not attach to PHY\n");
		return -ENODEV;
	}

	custom_hdlr->phy_oldlinks[1] = 0;

	if (custom_hdlr->disable_phy == PHY_POWER_DOWN) {
		if (custom_hdlr->phydevs[1]->drv->suspend) 
			custom_hdlr->phydevs[1]->drv->suspend(custom_hdlr->phydevs[1]);
	}
	return 0;
} 
EXPORT_SYMBOL(custom_init);

int custom_open(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;
	int ret;
	int value;
	int idx;

	ret = phy_create_files(ndev);
	if (ret) {
		phy_delete_files(ndev);
		unregister_netdev(ndev_priv->ndev);
		return ret;
	}

	ndev_priv->notify_work[PHY0_LINK].kn = 
		sysfs_get_dirent(ndev_priv->dev->kobj.sd, "phy0_link");
	ndev_priv->notify_work[PHY1_LINK].kn = 
		sysfs_get_dirent(ndev_priv->dev->kobj.sd, "phy1_link");
	ndev_priv->notify_work[ACTIVE_LINK].kn = 
		sysfs_get_dirent(ndev_priv->dev->kobj.sd, "active_link");

	phy_start(custom_hdlr->phydevs[1]);
	/* If the PHY must be isolated to disable it (MIAe) */
	if (custom_hdlr->disable_phy == PHY_ISOLATE) {
		value = phy_read(custom_hdlr->phydevs[1], MII_BMCR);
		phy_write(custom_hdlr->phydevs[1], MII_BMCR, 
			((value & ~BMCR_PDOWN) | BMCR_ISOLATE));
		if (custom_hdlr->gpio != -1) {
			ldb_gpio_set_value(custom_hdlr->gpio, 0);
		}
		/* autoneg must be disabled at this point. Otherwise, 
		the driver will remove the ISOLATE bit in the command
		register and break networking */
		custom_hdlr->phydevs[1]->autoneg = AUTONEG_DISABLE;
	}


	INIT_DELAYED_WORK(&ndev_priv->link_queue, custom_hdlr->link_monitor);
	INIT_WORK(&ndev_priv->arp_queue, fs_send_gratuitous_arp);
	for (idx=0; idx<3; idx++)
		INIT_WORK(&ndev_priv->notify_work[idx].notify_queue, fs_sysfs_notify);
	schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);

	return 0;
} 
EXPORT_SYMBOL(custom_open);

void custom_phy_stop(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;

	/* if custom_hdlr is NULL, leave the interface in normal operation */
	if (!custom_hdlr) {
		phy_stop(ndev_priv->phydev);	
		return ;
	}

	/* remove sysfs files */
	phy_delete_files(ndev);

	cancel_delayed_work_sync(&ndev_priv->link_queue);
	phy_stop(custom_hdlr->phydevs[0]);
	phy_stop(custom_hdlr->phydevs[1]);
} 
EXPORT_SYMBOL(custom_phy_stop);

void custom_phy_disconnect(struct net_device *ndev)
{
#if defined(CONFIG_FS_ENET)
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
#elif defined(CONFIG_UCC_GETH)
	struct ucc_geth_private *ndev_priv = netdev_priv(ndev);
#endif
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;

	/* if custom_hdlr is NULL, leave the interface in normal operation */
	if (!custom_hdlr) {
		phy_disconnect(ndev_priv->phydev);
		ndev_priv->phydev = NULL;
		return ;
	}

	phy_disconnect(custom_hdlr->phydevs[0]);
	phy_disconnect(custom_hdlr->phydevs[1]);
	
} 
EXPORT_SYMBOL(custom_phy_disconnect);

#if defined(CONFIG_FS_ENET)
int custom_set_settings(struct net_device *ndev, struct ethtool_cmd *cmd)
{
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;
	int ret;
	int value;

	if (!custom_hdlr) {
		return phy_ethtool_sset(ndev_priv->phydev, cmd);
	}

	if (cmd->phy_address == custom_hdlr->phydevs[1]->addr) 
		ret = phy_ethtool_sset(custom_hdlr->phydevs[1], cmd);
	else 
		ret = phy_ethtool_sset(custom_hdlr->phydevs[0], cmd);

	/* Only one PHY or no PIO E 18 -> exit */
	if (custom_hdlr->gpio == -1) 
		return ret;

	/* If PHY B is isolated or in power down, Switch to PHY A */
	value = phy_read(custom_hdlr->phydevs[1], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (ndev_priv->phydev == custom_hdlr->phydevs[1]) {
			dev_err(ndev_priv->dev, "Switch to PHY A\n");
			ndev_priv->phydev = custom_hdlr->phydevs[0];
			if (custom_hdlr->gpio != -1) {
				ldb_gpio_set_value(custom_hdlr->gpio, 0);
			}
		}
		return ret;
	}

	/* If we reach this point, PHY B is eligible as active PHY */

	/* If PHY A is isolated or in power down, Switch to PHY B */
	value = phy_read(custom_hdlr->phydevs[0], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (ndev_priv->phydev == custom_hdlr->phydevs[0]) {
			dev_err(ndev_priv->dev, "Switch to PHY B\n");
			ndev_priv->phydev = custom_hdlr->phydevs[1];
			if (custom_hdlr->gpio != -1) {
				ldb_gpio_set_value(custom_hdlr->gpio, 1);
			}
		}
		return ret;
	}

	/* If we reach this point, both PHYs are Powered up and not isolated. */
	/* This is an error. The network will certainly not work as expected. */

	return ret;
} 
EXPORT_SYMBOL(custom_set_settings);

int custom_fs_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	struct fs_enet_private *ndev_priv = netdev_priv(ndev);
	struct custom_phy_handler *custom_hdlr = ndev_priv->custom_hdlr;
	struct mii_ioctl_data *mii = (struct mii_ioctl_data *)&rq->ifr_data;
	int ret, value;
 
	if (!custom_hdlr) 
		return phy_mii_ioctl(ndev_priv->phydev, rq, cmd);

	if (custom_hdlr->phydevs[0]->addr == mii->phy_id)
		ret = phy_mii_ioctl(custom_hdlr->phydevs[0], rq, cmd);
	else if (custom_hdlr->phydevs[1]->addr == mii->phy_id)
		ret = phy_mii_ioctl(custom_hdlr->phydevs[1], rq, cmd);
	else
		return -EINVAL;

	/* Only one PHY or no PIO E 18 -> exit */
	if (custom_hdlr->gpio == -1 || ! custom_hdlr->phydevs[1]) 
		return ret;

	/* If PHY B is isolated or in power down, Switch to PHY A */
	value = phy_read(custom_hdlr->phydevs[1], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (ndev_priv->phydev == custom_hdlr->phydevs[1]) {
			dev_err(ndev_priv->dev, "Switch to PHY A\n");
			ndev_priv->phydev = custom_hdlr->phydevs[0];
			if (custom_hdlr->gpio != -1) {
				ldb_gpio_set_value(custom_hdlr->gpio, 0);
			}
		}
		return ret;
	}

	/* If we reach this point, PHY B is eligible as active PHY */

	/* If PHY A is isolated or in power down, Switch to PHY B */
	value = phy_read(custom_hdlr->phydevs[0], MII_BMCR);
	if (value & (BMCR_PDOWN | BMCR_ISOLATE)) {
		if (ndev_priv->phydev == custom_hdlr->phydevs[0]) {
			dev_err(ndev_priv->dev, "Switch to PHY B\n");
			ndev_priv->phydev = custom_hdlr->phydevs[1];
			if (custom_hdlr->gpio != -1) {
				ldb_gpio_set_value(custom_hdlr->gpio, 1);
			}
		}
		return ret;
	}

	/* If we reach this point, both PHYs are Powered up and not isolated. */
	/* This is an error, the network will certainly not work as expected. */

	return ret;
} 
EXPORT_SYMBOL(custom_fs_ioctl);
#endif
