/*arch/powerpc/platforms/8xx/cmpc885.c
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
#include <asm-generic/gpio.h>

#include <saf3000/saf3000.h>

#include "cmpc885.h"
#include "mpc8xx.h"
#include "mcr3000_2g.h"

struct cpm_pin {
	int port, pin, flags;
};


static struct cpm_pin cmpc885_pins_fev170[] = { /* FEV 170, a supprimer une fois FEV 169 traitee */
	{CPM_PORTC,  9, CPM_PIN_INPUT | CPM_PIN_GPIO }, 	/* CTS2			*/
	{CPM_PORTC,  5, CPM_PIN_INPUT | CPM_PIN_GPIO }, 	/* CTS3			*/
};
	
static void __init init_ioports_fev170(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmpc885_pins_fev170); i++) {
		struct cpm_pin *pin = &cmpc885_pins_fev170[i];
		cpm1_set_pin(pin->port, pin->pin, pin->flags);
	}
	pr_info("CTS2 and CTS3 temporarily deactivated (FEV170)\n");
}


/*
 * Init Carte 
 */
void __init cmpc885_pics_init(void)
{
	struct device_node *np;
	const char *model = "";
	int irq;

	mpc8xx_pics_init();
	
	np = of_find_node_by_path("/");
	if (np) {

		/* MCR3000_2G configuration */
		if (!strcmp(model, "MCR3000_2G")) {
			irq = fpgaf_pic_init();
			if (irq != NO_IRQ)
				set_irq_chained_handler(irq, fpgaf_cascade);
		}

		/* if CMPC885 configuration there nothing to do */

	} else {
		printk(KERN_ERR "MODEL: failed to identify model\n");
	}
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

static void __init cmpc885_setup_arch(void)
{
	cpm_reset();
	/* Set FEC1 and FEC2 to RMII mode on 100Mbps speed */
	mpc8xx_immr->im_cpm.cp_cptr = 0x00000180;
	mpc8xx_early_ping_watchdog();
}

static int __init cmpc885_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return of_flat_dt_is_compatible(root, "fsl,cmpc885");
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

		init_ioports_fev170();		
		/* MCR3000_2G configuration */
		if (!strcmp(model, "MCR3000_2G")) {
			irq = fpgaf_pic_init();
			if (irq != NO_IRQ)
				set_irq_chained_handler(irq, fpgaf_cascade);
			u16_gpiochip_init("s3k,mcr3000-fpga-f-gpio");
			fpgaf_init_platform_devices();
			
			fpga_clk_init();
			cpm1_clk_setup(CPM_CLK_SMC2, CPM_CLK5, CPM_CLK_RTX);

		} 
		/* CMPC885 configuration by default */
		else {
		}
	} else {
		pr_err("MODEL: failed to identify model\n");
	}

	return 0;
}
machine_device_initcall(cmpc885, declare_of_platform_devices);

define_machine(cmpc885) {
	.name			= "CMPC885",
	.probe			= cmpc885_probe,
	.setup_arch		= cmpc885_setup_arch,
	.init_IRQ		= cmpc885_pics_init,
	.get_irq		= mpc8xx_get_irq,
	.restart		= mpc8xx_restart,
	.calibrate_decr		= mpc8xx_calibrate_decr,
	.set_rtc_time		= mpc8xx_set_rtc_time,
	.get_rtc_time		= mpc8xx_get_rtc_time,
	.progress		= udbg_progress,
};
