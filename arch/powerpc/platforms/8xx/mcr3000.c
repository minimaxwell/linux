/* 
 * arch/powerpc/platforms/8xx/mcr3000.c
 *
 * Copyright 2010 CSSI
 *
 */

#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>

#include <ldb/ldb_gpio.h>

#include <sysdev/simple_gpio.h>

#include "mcr3000.h"
#include "mpc8xx.h"

/*
 * INIT PORTS 
 */

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
	
	/* SPI */
	{CPM_PORTB, 28, CPM_PIN_OUTPUT},					/* MISO			*/
	{CPM_PORTB, 29, CPM_PIN_OUTPUT},					/* MOSI 		*/
	{CPM_PORTB, 30, CPM_PIN_OUTPUT},					/* CLK			*/
	{CPM_PORTB, 31, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* SEL			*/
	
	/* NAND */
	{CPM_PORTD,  3, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* MISO			*/
	{CPM_PORTD,  4, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* MOSI 		*/
	{CPM_PORTD,  5, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* CLK			*/
	
	/* FPGA a verifier (faux) */
	{CPM_PORTB, 14, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* PROGFPGA		*/
	{CPM_PORTB, 16, CPM_PIN_INPUT | CPM_PIN_GPIO},				/* INITFPGA 		*/
	{CPM_PORTB, 17, CPM_PIN_INPUT | CPM_PIN_GPIO},				/* DONEFPGA		*/

	/* FPGA C4E1 a verifier (faux) */
	{CPM_PORTA, 1, CPM_PIN_INPUT | CPM_PIN_GPIO},				/* DONEFPGA		*/

	/* TDMA a verifier (faux) */
	{CPM_PORTA,  8, CPM_PIN_OUTPUT},					/* PCM_OUT		*/
	{CPM_PORTA,  9, CPM_PIN_OUTPUT},					/* PCM_IN 		*/

	/* EEPROM */
	{CPM_PORTB, 20, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* EE_HOLD		*/
	{CPM_PORTB, 21, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* EE_WP 		*/
	
	/* ALARMES */
	{CPM_PORTC, 13, CPM_PIN_INPUT | CPM_PIN_GPIO},				/* ACQ_ALARM		*/
	{CPM_PORTD, 15, CPM_PIN_OUTPUT | CPM_PIN_GPIO},				/* Alarme Mineure	*/
	
};

static void __init init_ioports(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mcr3000_pins); i++) {
		struct cpm_pin *pin = &mcr3000_pins[i];
		cpm1_set_pin(pin->port, pin->pin, pin->flags);
	}

	cpm1_clk_setup(CPM_CLK_SMC1, CPM_BRG1, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC1, CPM_BRG2, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC2, CPM_BRG3, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC3, CPM_BRG4, CPM_CLK_RTX);
	cpm1_clk_setup(CPM_CLK_SCC4, CPM_BRG4, CPM_CLK_RTX);
}

/*
 * Controlleur d'IRQ du CPLD 
 */

static u16 __iomem *cpld_pic_reg;
static struct irq_host *cpld_pic_host;

static void cpld_mask_irq(struct irq_data *d)
{
}

static void cpld_unmask_irq(struct irq_data *d)
{
}

static void cpld_end_irq(struct irq_data *d)
{
	unsigned int vec = (unsigned int)irqd_to_hwirq(d);

	clrbits16(cpld_pic_reg, 1<<(15-vec));
}

static struct irq_chip cpld_pic = {
	.name = "CPLD PIC",
	.irq_mask = cpld_mask_irq,
	.irq_unmask = cpld_unmask_irq,
	.irq_eoi = cpld_end_irq,
};

int cpld_get_irq(void)
{
	int vec;
	int ret;

	vec = 16 - ffs(in_be16(cpld_pic_reg)&0x1fe0);
	
	clrbits16(cpld_pic_reg, 1<<(15-vec));
	
	ret=irq_linear_revmap(cpld_pic_host, vec);
	return ret;
}

static int cpld_pic_host_map(struct irq_host *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	pr_debug("cpld_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &cpld_pic, handle_fasteoi_irq);
	return 0;
}

static struct irq_host_ops cpld_pic_host_ops = {
	.map = cpld_pic_host_map,
};

static int cpld_pic_init(void)
{
	struct device_node *np = NULL;
	struct resource res;
	unsigned int irq = NO_IRQ, hwirq;
	int ret;

	pr_debug("cpld_pic_init\n");

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-pic");
	if (np == NULL) {
		printk(KERN_ERR "CPLD PIC init: can not find mcr3000-pic node\n");
		return irq;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto end;

	cpld_pic_reg = ioremap(res.start, res.end - res.start + 1);
	if (cpld_pic_reg == NULL)
		goto end;

	irq = irq_of_parse_and_map(np, 0);
	if (irq == NO_IRQ)
		goto end;

	/* Initialize the CPLD interrupt controller. */
	hwirq = (unsigned int)virq_to_hw(irq);

	cpld_pic_host = irq_alloc_host(np, IRQ_HOST_MAP_LINEAR, 16, &cpld_pic_host_ops, 16);
	if (cpld_pic_host == NULL) {
		printk(KERN_ERR "CPLD PIC: failed to allocate irq host!\n");
		irq = NO_IRQ;
		goto end;
	}

end:
	of_node_put(np);
	return irq;
}

static void cpld_cascade(unsigned int irq, struct irq_desc *desc)
{
	int cascade_irq;
	struct irq_chip *chip;

	if ((cascade_irq = cpld_get_irq()) >= 0) {
		struct irq_desc *cdesc = irq_to_desc(cascade_irq);

		generic_handle_irq(cascade_irq);
		chip = irq_desc_get_chip(cdesc);
		if (chip->irq_eoi) chip->irq_eoi(&cdesc->irq_data);
	}
	chip = irq_desc_get_chip(desc);
	if (chip->irq_eoi) chip->irq_eoi(&desc->irq_data);
}

/*
 * Init Carte 
 */

void __init mcr3000_pics_init(void)
{
	int irq;

	mpc8xx_pics_init();
	
	irq = cpld_pic_init();
	if (irq != NO_IRQ)
		irq_set_chained_handler(irq, cpld_cascade);
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

static void __init mcr3000_setup_arch(void)
{
	cpm_reset();
	init_ioports();

	mpc8xx_early_ping_watchdog();
}

static int __init mcr3000_probe(void)
{
	unsigned long root = of_get_flat_dt_root();
	return of_flat_dt_is_compatible(root, "fsl,mcr3000");
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .name = "soc", },
	{ .name = "cpm", },
	{ .name = "localbus", },
	{ .name = "cpld", },
	{},
};

static int __init declare_of_platform_devices(void)
{
	struct device_node *np;

	pr_info("MCR3000 declare_of_platform_devices()\n");
	
	mpc8xx_early_ping_watchdog();
	
	proc_mkdir("s3k",0);
		
	simple_gpiochip_init("s3k,mcr3000-cpld-gpio");
	
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-cpld");
	if (np) {
		int ngpios = of_gpio_count(np);
		int i;
		
		if (ngpios<3) {
			pr_err("Gpio manquants dans mcr3000-cpld, impossible de les positionner\n");
		}
		for (i=0; i < ngpios; i++) {
			int gpio;

			gpio = ldb_gpio_init(np, NULL, i, 1);
			if (gpio==-1) continue;
			
			switch (i) { /* traitement specifique pour chaque GPIO */
			case 0: /* SPISEL */
			case 1: /* masque IRQ CPLD */
			case 2: /* valider ethernet 2 */
				/* activation SPISEL permanent */
				/* activation IRQ CPLD permanent */
				ldb_gpio_set_value(gpio, 1);
				break;
			}
		}
	}
	else {
		pr_crit("Could not find s3k,mcr3000-cpld node\n");
	}
	
	return 0;
}
machine_device_initcall(mcr3000, declare_of_platform_devices);

define_machine(mcr3000) {
	.name			= "MCR3000",
	.probe			= mcr3000_probe,
	.setup_arch		= mcr3000_setup_arch,
	.init_IRQ		= mcr3000_pics_init,
	.get_irq		= mpc8xx_get_irq,
	.restart		= mpc8xx_restart,
	.calibrate_decr		= mpc8xx_calibrate_decr,
	.set_rtc_time		= mpc8xx_set_rtc_time,
	.get_rtc_time		= mpc8xx_get_rtc_time,
	.progress		= udbg_progress,
};
