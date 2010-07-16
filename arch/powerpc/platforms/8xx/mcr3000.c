/*arch/powerpc/platforms/8xx/mcr3000.c
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

#include "mcr3000.h"
#include "mpc8xx.h"

struct cpm_pin {
	int port, pin, flags;
};

static struct cpm_pin mcr3000_pins[] = {
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
	
	/* SPI  a verifier (faux) */
	{CPM_PORTB, 28, CPM_PIN_OUTPUT},					/* MISO			*/
	{CPM_PORTB, 29, CPM_PIN_OUTPUT},					/* MOSI 		*/
	{CPM_PORTB, 30, CPM_PIN_OUTPUT},					/* CLK			*/
	{CPM_PORTB, 31, CPM_PIN_OUTPUT},					/* SEL			*/
	
	/* NAND */
	{CPM_PORTD,  3, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* MISO			*/
	{CPM_PORTD,  4, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* MOSI 		*/
	{CPM_PORTD,  5, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CLK			*/
	
	/* FPGA a verifier (faux) */
	{CPM_PORTB, 14, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* PROGFPGA		*/
	{CPM_PORTB, 16, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* INITFPGA 		*/
	{CPM_PORTB, 17, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* DONEFPGA		*/

	/* TDMA a verifier (faux) */
	{CPM_PORTA,  8, CPM_PIN_OUTPUT},					/* PCM_OUT		*/
	{CPM_PORTA,  9, CPM_PIN_OUTPUT},					/* PCM_IN 		*/

	/* EEPROM */
	{CPM_PORTB, 20, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* EE_HOLD		*/
	{CPM_PORTB, 21, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* EE_WP 		*/
};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mcr3000_pins); i++) {
		struct cpm_pin *pin = &mcr3000_pins[i];
		cpm1_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm1_clk_setup(CPM_CLK_SMC1, CPM_BRG1, CPM_CLK_RTX);
//	cpm1_clk_setup(CPM_CLK_SMC2, CPM_BRG2, CPM_CLK_RTX);
//	cpm1_clk_setup(CPM_CLK_SCC1, CPM_CLK1, CPM_CLK_TX);
//	cpm1_clk_setup(CPM_CLK_SCC1, CPM_CLK2, CPM_CLK_RX);

//	/* Set FEC1 and FEC2 to MII mode */
//	clrbits32(&mpc8xx_immr->im_cpm.cp_cptr, 0x00000180);
}

static void __init mcr3000_setup_arch(void)
{
////	struct device_node *np;
////	u32 __iomem *bcsr_io;
//	__volatile__ unsigned char dummy;
//	uint msr;


	cpm_reset();
	init_ioports();

////	np = of_find_compatible_node(NULL, NULL, "fsl,mcr3000-bcsr");
////	if (!np) {
////		printk(KERN_CRIT "Could not find fsl,mcr3000-bcsr node\n");
////		return;
////	}
////
////	bcsr_io = of_iomap(np, 0);
////	of_node_put(np);
////
////	if (bcsr_io == NULL) {
////		printk(KERN_CRIT "Could not remap BCSR\n");
////		return;
////	}
////
////	clrbits32(bcsr_io, BCSR1_RS232EN_1 | BCSR1_RS232EN_2 | BCSR1_ETHEN);
////	iounmap(bcsr_io);

//        /* ALBOP 16-05-2006 set internal clock to external freq */
//	/* and setup checkstop handling to generate a HRESET    */
//	mpc8xx_immr->im_clkrst.car_plprcr = 0x00A64084;
//	
//	while(1){
//	   __asm__("mfmsr %0" : "=r" (msr) );
//	   msr &= ~0x1000;
//	   __asm__("mtmsr %0" : : "r" (msr) );
//	   dummy = mpc8xx_immr->im_clkrst.res[0];
//	}
}

static int __init mcr3000_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return of_flat_dt_is_compatible(root, "fsl,mcr3000");
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
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	return 0;
}
machine_device_initcall(mcr3000, declare_of_platform_devices);

define_machine(mcr3000) {
	.name			= "MCR3000",
	.probe			= mcr3000_probe,
	.setup_arch		= mcr3000_setup_arch,
	.init_IRQ		= mpc8xx_pics_init,
	.get_irq		= mpc8xx_get_irq,
	.restart		= mpc8xx_restart,
	.calibrate_decr		= mpc8xx_calibrate_decr,
	.set_rtc_time		= mpc8xx_set_rtc_time,
	.get_rtc_time		= mpc8xx_get_rtc_time,
	.progress		= udbg_progress,
};
