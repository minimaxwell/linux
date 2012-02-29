#ifndef _SAF3000_H
#define _SAF3000_H

#define SAF3000_TFTP_PORT	7883

#define SAF3000_SIP1_PORT	5064
#define SAF3000_SIP2_PORT	5068
#define SAF3000_SIP3_PORT	5072
#define SAF3000_SIP4_PORT	5076
#define SAF3000_SIP5_PORT	5080
#define SAF3000_SIP6_PORT	5084
#define SAF3000_SIP7_PORT	5088

#include <linux/device.h>

extern struct class *saf3000_class_get(void);

extern void __init u16_gpiochip_init(const char *);

extern void fpga_clk_set_brg(void);
		
#include <linux/ioctl.h>

#define SAF3000_PCM_IOC_MAGIC		0x90

#define SAF3000_PCM_TIME_IDENT		0
#define SAF3000_PCM_LOST_IDENT		1
#define SAF3000_PCM_SILENT_IDENT	2

/* pour lire le temps de fonctionnement en ms sur /dev/pcm */
#define SAF3000_PCM_TIME			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_TIME_IDENT, \
						unsigned long)

/* pour lire le nombre de paquets reception de 5ms perdus sur /dev/pcm */
#define SAF3000_PCM_LOST			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_LOST_IDENT, \
						unsigned long)

/* pour lire le nombre de paquets silence de 1ms emis sur /dev/pcm */
#define SAF3000_PCM_SILENT			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_SILENT_IDENT, \
						unsigned long)

#endif
