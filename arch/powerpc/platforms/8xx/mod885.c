/*arch/powerpc/platforms/8xx/mod885.c
 *
 * Copyright 2010 CSSI Inc.
 *
 */

#include <linux/init.h>
#include <linux/of_platform.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>

#include "mod885.h"
#include "mpc8xx.h"

struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin mod885_pins[] = {
	/* SMC1 */
	{CPM_PORTA,  5, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CTS port DEBUG	*/
	{CPM_PORTB, 24, CPM_PIN_INPUT}, 					/* RX  port DEBUG	*/
	{CPM_PORTB, 25, CPM_PIN_INPUT}, 					/* TX  port DEBUG	*/

	/* SCC1 */
	{CPM_PORTA, 14, CPM_PIN_INPUT},						/* TX 			*/
	{CPM_PORTA, 15, CPM_PIN_INPUT},						/* RX 			*/
	{CPM_PORTB, 19, CPM_PIN_OUTPUT},					/* RTS			*/
	{CPM_PORTC, 11, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY},	/* CTS			*/

	/* SCC2 */
	{CPM_PORTA, 12, CPM_PIN_INPUT},						/* TX 			*/
	{CPM_PORTA, 13, CPM_PIN_INPUT},						/* RX 			*/
	{CPM_PORTB, 18, CPM_PIN_OUTPUT},					/* RTS			*/
	{CPM_PORTC,  9, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY},	/* CTS			*/
	
	/* SCC3 */
	{CPM_PORTD, 10, CPM_PIN_INPUT},						/* TX 			*/
	{CPM_PORTD, 11, CPM_PIN_INPUT},						/* RX 			*/
	{CPM_PORTD,  7, CPM_PIN_OUTPUT},					/* RTS			*/
	{CPM_PORTC,  7, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY},	/* CTS			*/
	
	/* SCC4 */
	{CPM_PORTD,  8, CPM_PIN_INPUT},						/* TX 			*/
	{CPM_PORTD,  9, CPM_PIN_INPUT},						/* RX 			*/
	{CPM_PORTD,  6, CPM_PIN_OUTPUT},					/* RTS			*/
	{CPM_PORTC,  4, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_SECONDARY},	/* DCD			*/
	{CPM_PORTC,  5, CPM_PIN_INPUT | CPM_PIN_SECONDARY},			/* CTS			*/
	
	/* SPI */
	{CPM_PORTB, 28, CPM_PIN_OUTPUT},					/* MISO			*/
	{CPM_PORTB, 29, CPM_PIN_OUTPUT},					/* MOSI 		*/
	{CPM_PORTB, 30, CPM_PIN_OUTPUT},					/* CLK			*/
	
	/* NAND */
	{CPM_PORTD,  12, CPM_PIN_OUTPUT | CPM_PIN_GPIO},			/* CLE_DISK		*/
	{CPM_PORTD,  13, CPM_PIN_OUTPUT | CPM_PIN_GPIO},			/* ALE_DISK 		*/
	{CPM_PORTD,  15, CPM_PIN_OUTPUT | CPM_PIN_GPIO},			/* CS_DISK		*/
	

	/* EEPROM */
	{CPM_PORTB, 21, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CS_EEPROM 		*/
	
};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mod885_pins); i++) {
		struct cpm_pin *pin = &mod885_pins[i];
		cpm1_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm1_clk_setup(CPM_CLK_SMC1, CPM_BRG1, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC1, CPM_BRG2, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC2, CPM_BRG3, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC3, CPM_BRG4, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_RTX);
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
	return of_flat_dt_is_compatible(root, "fsl,mod885");
	return 0;
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .name = "soc", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	pr_info("MOD885 declare_of_platform_devices()\n");
	
	mpc8xx_early_ping_watchdog();
	
	proc_mkdir("s3k",0);
		
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	return 0;
}
machine_device_initcall(mod885, declare_of_platform_devices);

define_machine(mod885) {
	.name			= "MOD885",
	.probe			= mod885_probe,
	.setup_arch		= mod885_setup_arch,
	.init_IRQ		= mpc8xx_pics_init,
	.get_irq		= mpc8xx_get_irq,
	.restart		= mpc8xx_restart,
	.calibrate_decr		= mpc8xx_calibrate_decr,
	.set_rtc_time		= mpc8xx_set_rtc_time,
	.get_rtc_time		= mpc8xx_get_rtc_time,
	.progress		= udbg_progress,
};
