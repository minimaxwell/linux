/* TODO
 *
 */

#include <linux/platform_device.h>
#include <linux/workqueue.h>

#define PHY_ISOLATE 		1
#define PHY_POWER_DOWN 		2
#define LINK_MONITOR_RETRY	(2*HZ)
#define MODE_MANU		1
#define MODE_AUTO		2

#define PHYA_ADDR		3
#define J1_ADDR			PHYA_ADDR
#define PHYB_ADDR		2
#define J2_ADDR			PHYB_ADDR
#define J6_ADDR			1

#define PHY0_LINK 0
#define PHY1_LINK 1
#define ACTIVE_LINK 2

//struct fs_notify_work {
//	struct work_struct notify_queue;
//	struct kernfs_node *kn;
//};

struct custom_phy_handler {
	int gpio;
	int disable_phy;
	int use_PHY5;

	struct phy_device *phydevs[2];
	//struct delayed_work link_queue;
	//struct work_struct arp_queue;
	//struct fs_notify_work notify_work[3];
	int change_time;
	int mode;

	int phy_oldlinks[2];

	void (*link_switch)(struct net_device *ndev);
	void (*link_monitor)(struct work_struct *work);
	void (*adjust_state)(struct net_device *dev);

};

/**
 * phy_create_files() - Create files to report phy additional states.
 * @ndev: Pointer to associated net device.
 *
 * Create all files needed to report additionnal states related to
 * the handling two phy associated to only one net device.
 *
 * Return: return an error if one the device_create_file failed.
 */
int phy_create_files(struct net_device *ndev);

/**
 * phy_delete_files() - Delete files to report phy additional states.
 * @ndev: Pointer to associated net device.
 *
 * Create all files needed to report additionnal states related to
 * the handling two phy associated to only one net device.
 *
 */
void phy_delete_files(struct net_device *ndev);

void fs_send_gratuitous_arp(struct work_struct *work);
void fs_sysfs_notify(struct work_struct *work);

void fs_link_switch(struct net_device *ndev);
void fs_link_monitor(struct work_struct *work);

void mcr1g_link_switch(struct net_device *ndev);
void mcr1g_link_monitor(struct work_struct *work);
void mcr1g_adjust_state(struct net_device *dev);

void mcr2g_link_switch(struct net_device *ndev);

void miae_link_switch(struct net_device *ndev);

void adjust_state(struct net_device *dev);

void custom_probe(struct net_device *dev, struct platform_device *ofdev);
int custom_init(struct net_device *dev, void (*hndlr)(struct net_device *), phy_interface_t iface);
int custom_open(struct net_device *dev);
int custom_close(struct net_device *dev);
void custom_phy_stop(struct net_device *dev);
void custom_phy_disconnect(struct net_device *dev);
int custom_set_settings(struct net_device *dev, struct ethtool_cmd *cmd);
int custom_fs_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd);
