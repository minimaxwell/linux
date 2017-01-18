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

	return sprintf(buf, "%d\n",ndev_priv->phydev == ndev_priv->phydevs[1]?1:0);
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
	if (!ndev_priv->phydevs[active]) {
		dev_warn(dev, "PHY on address %d does not exist\n", active);
		return count;
	}
	if (ndev_priv->phydevs[active] != ndev_priv->phydev) {
		fs_link_switch(ndev_priv);
		cancel_delayed_work_sync(&ndev_priv->link_queue);
		if (!ndev_priv->phydev->link && ndev_priv->mode == MODE_AUTO) schedule_delayed_work(&ndev_priv->link_queue, LINK_MONITOR_RETRY);
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

	ctrl = phy_read(ndev_priv->phydevs[0], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : ndev_priv->phydevs[0]->link ? 2 : 1);
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
	
	ctrl = phy_read(ndev_priv->phydevs[1], MII_BMCR);
	return sprintf(buf, "%d\n", ctrl & BMCR_PDOWN ? 0 : ndev_priv->phydevs[1]->link ? 2 : 1);
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
	struct phy_device *phydev = ndev_priv->phydevs[0];

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
	struct phy_device *phydev = ndev_priv->phydevs[1];

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

	return sprintf(buf, "%d\n",ndev_priv->mode);
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
	
	if (ndev_priv->mode != mode) {
		ndev_priv->mode = mode;
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

