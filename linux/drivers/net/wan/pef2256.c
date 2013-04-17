/*
 * drivers/net/wan/pef2256.c : a PEF2256 HDLC driver for Linux
 *
 * This software may be used and distributed according to the terms of the
 * GNU General Public License.
 *
 */

#include "pef2256.h"

static int pef2256_open(struct net_device *netdev)
{
	if (hdlc_open(netdev))
		return -EAGAIN;

	/* Do E1 stuff */

	netif_start_queue(netdev);
	netif_carrier_on(netdev);
	return 0;
}


static int pef2256_close(struct net_device *netdev)
{
	netif_stop_queue(netdev);

	/* Do E1 stuff */

	hdlc_close(netdev);
	return 0;
}



/* Handler IT lecture */
static int pef_2256_rx(int irq, void *dev_id)
{
	struct net_device *netdev = (struct net_device *)dev_id;
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;
	struct sk_buff *skb;

	/* Do E1 stuff */

	/* Si trame entierement arrivee */
	skb = dev_alloc_skb(priv->rx_len);
	if (! skb) {
		netdev->stats.rx_dropped++;
		return -ENOMEM;
	}
	memcpy(skb->data, priv->rx_buff, priv->rx_len);
	skb_put(skb, priv->rx_len);
	skb->protocol = hdlc_type_trans(skb, priv->netdev);
	netdev->stats.rx_packets++;
	netdev->stats.rx_bytes += skb->len;
	netif_rx(skb);

	return 0;
}


/* Handler IT ecriture */
static int pef_2256_xmit(int irq, void *dev_id)
{
	struct net_device *netdev = (struct net_device *)dev_id;
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	/* Do E1 stuff */

	/* Si trame entierement transferee */
	netdev->stats.tx_packets++;
	netdev->stats.tx_bytes += priv->tx_skbuff->len;
	dev_kfree_skb(priv->tx_skbuff);
	priv->tx_skbuff=NULL;
	netif_wake_queue(netdev);

	return 0;
}


static netdev_tx_t pef2256_start_xmit(struct sk_buff *skb,
					  struct net_device *netdev)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	/* demande emission trame skb->data de longueur skb->len */
	priv->tx_skbuff = skb;

	/* Do E1 stuff */

	netif_stop_queue(netdev);
	return NETDEV_TX_OK;
}

static const struct net_device_ops pef2256_ops = {
	.ndo_open       = pef2256_open,
	.ndo_stop       = pef2256_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
};


/* A vérifier : Qu'est ce que supporte le composant au juste ? */
static int pef2256_hdlc_attach(struct net_device *netdev, 
				unsigned short encoding, unsigned short parity)
{
	struct pef2256_dev_priv *priv = dev_to_hdlc(netdev)->priv;

	if (encoding != ENCODING_NRZ &&
	    encoding != ENCODING_NRZI &&
	    encoding != ENCODING_FM_MARK &&
	    encoding != ENCODING_FM_SPACE &&
	    encoding != ENCODING_MANCHESTER)
		return -EINVAL;

	if (parity != PARITY_NONE &&
	    parity != PARITY_CRC16_PR0_CCITT &&
	    parity != PARITY_CRC16_PR1_CCITT &&
	    parity != PARITY_CRC32_PR0_CCITT &&
	    parity != PARITY_CRC32_PR1_CCITT)
		return -EINVAL;

	priv->encoding = encoding;
	priv->parity = parity;
	return 0;
}


/*
 * Chargement du module
 */
static const struct of_device_id pef2256_match[];
static int pef2256_probe(struct platform_device *ofdev)
{
	const struct of_device_id *match;
	struct pef2256_dev_priv *priv;
	int ret = -ENOMEM;
	struct net_device *netdev;
	hdlc_device *hdlc;

	match = of_match_device(pef2256_match, &ofdev->dev);
	if (!match)
		return -EINVAL;

	/* Do E1 stuff */

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (! priv)
		return ret;

	priv->tx_skbuff = NULL;

	netdev = priv->netdev;
	hdlc = dev_to_hdlc(netdev);
/* ???
	d->base_addr = ;
	d->irq = ;
*/
	netdev->netdev_ops = &pef2256_ops;
	SET_NETDEV_DEV(netdev, &ofdev->dev);
	hdlc->attach = pef2256_hdlc_attach;
	hdlc->xmit = pef2256_start_xmit;
	
	platform_set_drvdata(ofdev, priv);
	return 0;
}


/*
 * Suppression du module
 */
static int pef2256_remove(struct platform_device *ofdev)
{
	struct pef2256_dev_priv *priv = platform_get_drvdata(ofdev);

	unregister_hdlc_device(priv->netdev);
	free_netdev(priv->netdev);

	/* Do E1 stuff */

	platform_set_drvdata(ofdev, NULL);
	kfree(ofdev);
	return 0;
}

static const struct of_device_id pef2256_match[] = {
	{
		.compatible = "s3k,mcr3000-e1",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pef2256_match);


static struct platform_driver pef2256_driver = {
	.probe		= pef2256_probe,
	.remove		= pef2256_remove,
	.driver		= {
		.name	= "pef2256",
		.owner	= THIS_MODULE,
		.of_match_table	= pef2256_match,
	},
};


static int __init pef2256_init(void)
{
	int ret;
	ret = platform_driver_register(&pef2256_driver);
	return ret;
}
module_init(pef2256_init);


static void __exit pef2256_exit(void)
{
	platform_driver_unregister(&pef2256_driver);
}
module_exit(pef2256_exit);


/* INFORMATIONS GENERALES */
MODULE_AUTHOR(M_DRV_PEF2256_AUTHOR);
MODULE_VERSION(M_DRV_PEF2256_VERSION);
MODULE_DESCRIPTION("Infineon PEF 2256 E1 Controller");
MODULE_LICENSE("GPL");
