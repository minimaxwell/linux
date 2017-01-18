/* TODO
 *
 */

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

#if defined(CONFIG_FS_ENET)
void fs_link_switch(struct fs_enet_private *fep);
#elif defined(CONFIG_UCC_GETH)
void fs_link_switch(struct ucc_geth_private *ugeth);
#endif


