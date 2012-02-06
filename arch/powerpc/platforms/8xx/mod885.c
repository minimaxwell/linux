/*arch/powerpc/platforms/8xx/mod885.c
 *
 * Copyright 2010 CSSI Inc.
 *
 */

#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>
#include <asm-generic/gpio.h>

#include <sysdev/simple_gpio.h>

#include "mod885.h"
#include "mpc8xx.h"
#include "mcr3000_2g.h"

struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin mod885_pins[] = {
	/* SMC1 */
	{CPM_PORTB, 24, CPM_PIN_INPUT}, 					/* RX  port DEBUG	*/
	{CPM_PORTB, 25, CPM_PIN_INPUT}, 					/* TX  port DEBUG	*/

	/* SMC2 */
	{CPM_PORTE, 20, CPM_PIN_INPUT | CPM_PIN_SECONDARY},			/* SMTXD2		*/
	{CPM_PORTB, 20, CPM_PIN_INPUT}, 					/* SMRXD2		*/
	
	/* SCC2 */
	{CPM_PORTA, 12, CPM_PIN_INPUT}, 					/* TXD2			*/
	{CPM_PORTA, 13, CPM_PIN_INPUT}, 					/* RXD2			*/
	{CPM_PORTB, 18, CPM_PIN_OUTPUT}, 					/* RTS2			*/
	{CPM_PORTC,  9, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY}, 	/* CTS2			*/
	{CPM_PORTC,  8, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY}, 	/* CD2			*/
	
	/* SCC3 */
	{CPM_PORTD, 10, CPM_PIN_INPUT}, 					/* TXD3			*/
	{CPM_PORTD, 11, CPM_PIN_INPUT}, 					/* RXD3			*/
	{CPM_PORTD,  7, CPM_PIN_INPUT}, 					/* RTS3			*/
	{CPM_PORTC,  5, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY}, 	/* CTS3			*/
	{CPM_PORTC,  4, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY}, 	/* CD3			*/
	
	/* SCC4 */
	{CPM_PORTD,  9, CPM_PIN_OUTPUT}, 					/* TXD4			*/
	{CPM_PORTE, 25, CPM_PIN_INPUT}, 					/* RXD4			*/
	
	/* SPI */
	{CPM_PORTB, 28, CPM_PIN_OUTPUT},					/* MISO			*/
	{CPM_PORTB, 29, CPM_PIN_OUTPUT},					/* MOSI 		*/
	{CPM_PORTB, 30, CPM_PIN_OUTPUT},					/* CLK			*/
	{CPM_PORTB, 14, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CS_TEMP		*/
	{CPM_PORTB, 19, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* WR_TEMP		*/
	{CPM_PORTB, 21, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CS_EEPROM		*/
	
	/* NAND */
	{CPM_PORTD, 12, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CLE_DISK		*/
	{CPM_PORTD, 13, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* ALE_DISK 		*/
	{CPM_PORTD, 15, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CS_DISK		*/

	/* USB */
	{CPM_PORTC,  7, CPM_PIN_OUTPUT},					/* USB_TXN			*/ 
	{CPM_PORTC,  6, CPM_PIN_OUTPUT},					/* USB_TXN			*/ 
	{CPM_PORTA, 14, CPM_PIN_INPUT},						/* USB_TXN			*/ 
	{CPM_PORTC, 11, CPM_PIN_INPUT | CPM_PIN_SECONDARY},			/* USB_TXN			*/ 
	{CPM_PORTC, 10, CPM_PIN_INPUT | CPM_PIN_SECONDARY},			/* USB_TXN			*/ 
	{CPM_PORTA, 15, CPM_PIN_INPUT},						/* USB_TXN			*/ 
		
	/* MII1 */
	{CPM_PORTA, 0, CPM_PIN_INPUT},						/* RMII-1_RXD1		*/
	{CPM_PORTA, 1, CPM_PIN_INPUT},						/* RMII-1_RXD0		*/
	{CPM_PORTA, 2, CPM_PIN_INPUT},						/* RMII-1_CRS_DV	*/
	{CPM_PORTA, 3, CPM_PIN_INPUT},						/* RMII-1_RXER		*/
	{CPM_PORTA, 4, CPM_PIN_OUTPUT},						/* RMII-1_TXD1		*/
	{CPM_PORTA, 11, CPM_PIN_OUTPUT},					/* RMII-1_TXD0		*/
	{CPM_PORTB, 31, CPM_PIN_INPUT},						/* RMII-1_REF_CLK	*/
	
	{CPM_PORTD, 8, CPM_PIN_INPUT},						/* R_MDC		*/

	/* MII2 */
	{CPM_PORTE, 14, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},			/* RMII-2_TXD0		*/
	{CPM_PORTE, 15, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},			/* RMII-2_TXD1		*/
	{CPM_PORTE, 16, CPM_PIN_OUTPUT},					/* RMII-2_REF_CLK	*/
	{CPM_PORTE, 19, CPM_PIN_OUTPUT | CPM_PIN_SECONDARY},			/* RMII-2_TXEN		*/
	{CPM_PORTE, 21, CPM_PIN_OUTPUT},					/* RMII-2_RXD0		*/
	{CPM_PORTE, 22, CPM_PIN_OUTPUT},					/* RMII-2_RXD1		*/
	{CPM_PORTE, 26, CPM_PIN_OUTPUT},					/* RMII-2_CRS_DV	*/
	{CPM_PORTE, 27, CPM_PIN_OUTPUT},					/* RMII-2_RXER		*/
};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mod885_pins); i++) {
		struct cpm_pin *pin = &mod885_pins[i];
		cpm1_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm1_clk_setup(CPM_CLK_SCC2, CPM_BRG1, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC3, CPM_BRG2, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC4, CPM_BRG3, CPM_CLK_RTX);
	/* cpm1_clk_setup(CPM_CLK_SMC2, CPM_CLK5, CPM_CLK_RTX); */
	cpm1_clk_setup(CPM_CLK_SMC2, CPM_BRG3, CPM_CLK_RTX); /* Normalement SMC2 est clocké par un BRG dans le FPGA via CLK5 */
	
	cpm1_clk_setup(CPM_CLK_SMC1, CPM_BRG4, CPM_CLK_RTX);
	
	/* Set FEC1 and FEC2 to RMII mode */
	mpc8xx_immr->im_cpm.cp_cptr = 0x00000180;
}


/*
 * Init Carte 
 */
void __init mod885_pics_init(void)
{
//	struct device_node *np;
//	const char *model = "";
//	int irq;

	mpc8xx_pics_init();
	
//	np = of_find_node_by_path("/");
//	if (np) {
//
//		/* MCR3000_2G configuration */
//		if (!strcmp(model, "MCR3000_2G")) {
//			irq = fpgaf_pic_init();
//			if (irq != NO_IRQ)
//				set_irq_chained_handler(irq, fpgaf_cascade);
//		}
//
//		/* if MOD885 configuration there nothing to do */
//
//	} else {
//		printk(KERN_ERR "MODEL: failed to identify model\n");
//	}
}

static int __init mpc8xx_early_ping_watchdog(void)
{
	volatile immap_t *immap = ioremap(get_immrbase(),sizeof(*immap));
	
	immap->im_siu_conf.sc_swsr=0x556C;
	immap->im_siu_conf.sc_swsr=0xAA39;
	
	iounmap(immap);
	
	return 0;
}
arch_initcall(mpc8xx_early_ping_watchdog);

static void __init mod885_setup_arch(void)
{
	cpm_reset();
	init_ioports();
	mpc8xx_early_ping_watchdog();
}

static int __init mod885_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return (of_flat_dt_is_compatible(root, "fsl,mod885") ||
		of_flat_dt_is_compatible(root, "fsl,mcr3000"));
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .name = "soc", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{ .name = "fpga-f", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	struct device_node *np;
	const char *model = "";
	int irq;

	np = of_find_node_by_path("/");
	if (np) {

		model = of_get_property(np, "model", NULL);

		pr_info("%s declare_of_platform_devices()\n", model);
		
		mpc8xx_early_ping_watchdog();
		proc_mkdir("s3k",0);
		of_platform_bus_probe(NULL, of_bus_ids, NULL);
		
		/* MCR3000_2G configuration */
		if (!strcmp(model, "MCR3000_2G")) {
			irq = fpgaf_pic_init();
			if (irq != NO_IRQ)
				irq_set_chained_handler(irq, fpgaf_cascade);
			simple_gpiochip_init("s3k,mcr3000-fpga-f-gpio");
			fpgaf_init_platform_devices();
			
/*			fpga_clk_init();
			cpm1_clk_setup(CPM_CLK_SMC2, CPM_CLK5, CPM_CLK_RTX);
*/
		} 
		/* MOD885 configuration by default */
		else {
		}
	} else {
		pr_err("MODEL: failed to identify model\n");
	}

	return 0;
}
machine_device_initcall(mod885, declare_of_platform_devices);

define_machine(mod885) {
	.name			= "MOD885",
	.probe			= mod885_probe,
	.setup_arch		= mod885_setup_arch,
	.init_IRQ		= mod885_pics_init,
	.get_irq		= mpc8xx_get_irq,
	.restart		= mpc8xx_restart,
	.calibrate_decr		= mpc8xx_calibrate_decr,
	.set_rtc_time		= mpc8xx_set_rtc_time,
	.get_rtc_time		= mpc8xx_get_rtc_time,
	.progress		= udbg_progress,
};
