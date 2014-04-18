#ifndef _SAF3000_H
#define _SAF3000_H

#ifdef __KERNEL__

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
extern struct device *saf3000_gpio_device_get(void);

extern void __init u16_gpiochip_init(const char *);

extern void fpga_clk_set_brg(void);

extern void gest_led_debug(int led, int cmde);
extern int type_fav(void);
extern int fpga_read_channel(int channel, int *val);

#define FAV_NVCS_LEMO		6
#define FAV_NVCS_FISCHER	5

#define PCM_CODEC_MINOR		240	/* tdm codecs et E1 MIAe */
#define PCM_VOIE_MINOR		241	/* tdm voies MCR3K-2G */
#define PCM_RETARD_MINOR	242	/* tdm retard MCR3K-2G */
#define WB_MINOR_SCC2		243	/* crypto wide-band SCC2 */
#define WB_MINOR_SCC3		244	/* crypto wide-band SCC3 */
#define NAME_CODEC_DEVICE	"pcm"
#define NAME_VOIE_DEVICE	"pcm_voie"
#define NAME_RETARD_DEVICE	"pcm_retard"
#define NAME_WB_SCC2_DEVICE	"wb_scc2"
#define NAME_WB_SCC3_DEVICE	"wb_scc3"

#endif /* __KERNEL__ */
		
#include <linux/ioctl.h>

#define SAF3000_PCM_IOC_MAGIC		0x90

#define SAF3000_PCM_TIME_IDENT		0
#define SAF3000_PCM_LOST_IDENT		1
#define SAF3000_PCM_SILENT_IDENT	2
#define SAF3000_PCM_DELAY_EM_IDENT	3
#define SAF3000_PCM_DELAY_REC_IDENT	4
#define SAF3000_PCM_READTIME_IDENT	5

/* pour lire le temps de fonctionnement en ms sur /dev/pcm... */
#define SAF3000_PCM_TIME			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_TIME_IDENT, \
						unsigned long)

/* pour lire le nombre de paquets reception de 5ms perdus sur /dev/pcm... */
#define SAF3000_PCM_LOST			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_LOST_IDENT, \
						unsigned long)

/* pour lire le nombre de paquets silence de 1ms emis sur /dev/pcm... */
#define SAF3000_PCM_SILENT			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_SILENT_IDENT, \
						unsigned long)

/* pour lire le temps de la derniere lecture en ms sur /dev/pcm... */
#define SAF3000_PCM_READTIME			_IOR(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_READTIME_IDENT, \
						unsigned long)

struct pose_delay {
	unsigned short	ident;
	unsigned short	delay;
};

/* pour ecrire le retard d'une ligne emission sur /dev/pcm... */
#define SAF3000_PCM_DELAY_EM			_IOW(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_DELAY_EM_IDENT, \
						struct pose_delay)

/* pour ecrire le retard d'une ligne reception sur /dev/pcm... */
#define SAF3000_PCM_DELAY_REC			_IOW(SAF3000_PCM_IOC_MAGIC, \
						SAF3000_PCM_DELAY_REC_IDENT, \
						struct pose_delay)

/* @brief Structure pour le mmap() sur les devices /dev/tdm_xxx */
struct tdm_map {
	unsigned long current_time; /**< @brief Donne l'heure TDM correspondant a la derniere mise a jour du compteur (en ms) */
	unsigned long last_read_time; /**< @brief Donne l'heure TDM correspondant a la derniere lecture */
	unsigned long current_tbl; /**< @brief Donne la valeur de la partie basse de la TIMEBASE */
	unsigned long tb_ticks_per_msec; /**< @brief Donne le nombre de ticks TIMEBASE par miliseconde */
};


#define SAF3000_WB_IOC_MAGIC		0x91

#define SAF3000_WB_STOP_REC_IDENT	0

/* pour stopper la reception et se mettre en recherche de synchro */
#define SAF3000_WB_STOP_REC			_IO(SAF3000_WB_IOC_MAGIC, \
						SAF3000_WB_STOP_REC_IDENT)

#endif
