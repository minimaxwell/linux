#define DEBUG 1

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

#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/system.h>
#include <asm/time.h>
#include <asm/8xx_immap.h>
#include <asm/cpm1.h>
#include <asm/fs_pd.h>
#include <asm/udbg.h>

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
	{CPM_PORTC, 13, CPM_PIN_INPUT | CPM_PIN_GPIO | CPM_PIN_FALLEDGE},	/* ACQ_ALARM		*/
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

//	/* Set FEC1 and FEC2 to MII mode */
//	clrbits32(&mpc8xx_immr->im_cpm.cp_cptr, 0x00000180);
}

/*
 * Controlleur GPIO pour les ChipSelect SPI 
 */

static spinlock_t cpld_csspi_lock;
static struct of_mm_gpio_chip cpld_csspi_mm_gc;

static int cpld_csspi_get(struct gpio_chip *gc, unsigned int gpio)
{
	return gpio == ((in_be16(cpld_csspi_mm_gc.regs) >> 5) & 7) ? 1:0;
}

static void cpld_csspi_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	unsigned long flags;
	unsigned short reg;

	spin_lock_irqsave(&cpld_csspi_lock, flags);

	reg = in_be16(cpld_csspi_mm_gc.regs);
	reg &= ~(7<<5);
	if (val) reg |= (gpio&7) << 5;
	out_be16(cpld_csspi_mm_gc.regs, reg);

	spin_unlock_irqrestore(&cpld_csspi_lock, flags);
}

static int cpld_csspi_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return 0;
}

static int cpld_csspi_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	cpld_csspi_set(gc, gpio, val);
	return 0;
}

static void cpld_csspi_save_regs(struct of_mm_gpio_chip *mm_gc)
{
}

static int __init cpld_csspi_gpiochip_add(struct device_node *np)
{
	struct of_mm_gpio_chip *mm_gc;
	struct of_gpio_chip *of_gc;
	struct gpio_chip *gc;

	pr_info("Initialisation ChipSelects CPLD\n");
	
	spin_lock_init(&cpld_csspi_lock);

	mm_gc = &cpld_csspi_mm_gc;
	of_gc = &mm_gc->of_gc;
	gc = &of_gc->gc;

	mm_gc->save_regs = cpld_csspi_save_regs;
	of_gc->gpio_cells = 2;
	gc->ngpio = 8;
	gc->direction_input = cpld_csspi_dir_in;
	gc->direction_output = cpld_csspi_dir_out;
	gc->get = cpld_csspi_get;
	gc->set = cpld_csspi_set;

	return of_mm_gpiochip_add(np, mm_gc);
}

/*
 * Controlleur GPIO pour les ports OP1 � OP4 du MPC
 */

static spinlock_t opx_gpio_lock;
static struct of_mm_gpio_chip opx_gpio_mm_gc;

#define M8XX_PGCRX_CXOE    0x00000080
#define M8XX_PGCRX_CXRESET 0x00000040

static int opx_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	unsigned int *pgcrx=(unsigned int *)opx_gpio_mm_gc.regs;
	unsigned int reg;
	unsigned int mask;
	unsigned int d31 = gpio&1;
	unsigned int d30 = (gpio>>1)&1;

	reg = in_be32(pgcrx + d30);
	mask = d31^d30 ? M8XX_PGCRX_CXOE:M8XX_PGCRX_CXRESET;
	return reg&mask ? 1:0;
}

static void opx_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	unsigned long flags;
	unsigned int *pgcrx=(unsigned int *)opx_gpio_mm_gc.regs;
	unsigned int reg;
	unsigned int mask;
	unsigned int d31 = gpio&1;
	unsigned int d30 = (gpio>>1)&1;

	spin_lock_irqsave(&opx_gpio_lock, flags);

	reg = in_be32(pgcrx + d30);
	mask = d31^d30 ? M8XX_PGCRX_CXOE:M8XX_PGCRX_CXRESET;
	if (val) reg |= mask;
	else reg &= ~mask;
	out_be32(pgcrx + d30, reg);

	spin_unlock_irqrestore(&opx_gpio_lock, flags);
}

static int opx_gpio_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	return -EINVAL;
}

static int opx_gpio_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	opx_gpio_set(gc, gpio, val);
	return 0;
}

static void opx_gpio_save_regs(struct of_mm_gpio_chip *mm_gc)
{
}

static int __init opx_gpio_gpiochip_add(struct device_node *np)
{
	struct of_mm_gpio_chip *mm_gc;
	struct of_gpio_chip *of_gc;
	struct gpio_chip *gc;

	pr_info("Initialisation GPIO OPx\n");
	
	spin_lock_init(&opx_gpio_lock);

	mm_gc = &opx_gpio_mm_gc;
	of_gc = &mm_gc->of_gc;
	gc = &of_gc->gc;

	mm_gc->save_regs = opx_gpio_save_regs;
	of_gc->gpio_cells = 2;
	gc->ngpio = 4;
	gc->direction_input = opx_gpio_dir_in;
	gc->direction_output = opx_gpio_dir_out;
	gc->get = opx_gpio_get;
	gc->set = opx_gpio_set;

	return of_mm_gpiochip_add(np, mm_gc);
}

/*
 * Controlleur d'IRQ du CPLD 
 */

static u16 __iomem *cpld_pic_reg;
static struct irq_host *cpld_pic_host;

static void cpld_mask_irq(unsigned int irq)
{
}

static void cpld_unmask_irq(unsigned int irq)
{
}

static void cpld_end_irq(unsigned int irq)
{
	unsigned int vec = (unsigned int)irq_map[irq].hwirq;

	clrbits16(cpld_pic_reg, 1<<(15-vec));
}

static struct irq_chip cpld_pic = {
	.name = "CPLD PIC",
	.mask = cpld_mask_irq,
	.unmask = cpld_unmask_irq,
	.eoi = cpld_end_irq,
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

	irq_to_desc(virq)->status |= IRQ_LEVEL;
	set_irq_chip_and_handler(virq, &cpld_pic, handle_fasteoi_irq);
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
	hwirq = (unsigned int)irq_map[irq].hwirq;

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

	if ((cascade_irq = cpld_get_irq()) >= 0) {
		struct irq_desc *cdesc = irq_to_desc(cascade_irq);

		generic_handle_irq(cascade_irq);
		if (cdesc->chip->eoi) cdesc->chip->eoi(cascade_irq);
	}
	if (desc->chip->eoi) desc->chip->eoi(irq);
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
		set_irq_chained_handler(irq, cpld_cascade);
}

static void __init mcr3000_setup_arch(void)
{
//	__volatile__ unsigned char dummy;
//	uint msr;
	immap_t *immap;


	cpm_reset();
	init_ioports();

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

	/* En attendant que ce soit fait par UBOOT */
	
	immap = ioremap(get_immrbase(),sizeof(*immap));
	
	immap->im_memctl.memc_br5 = 0x14000801;
	immap->im_memctl.memc_or5 = 0xFFFF8916;
	
	iounmap(immap);
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
	{},
};

static int __init declare_of_platform_devices(void)
{
	struct device_node *np;
	
	simple_gpiochip_init("s3k,mcr3000-cpld-gpio");
	
	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-cpld-csspi");
	if (np) 
		cpld_csspi_gpiochip_add(np);
	else
		pr_crit("Could not find s3k,mcr3000-cpld-csspi node\n");
	
	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-opx-gpio");
	if (np) 
		opx_gpio_gpiochip_add(np);
	else
		pr_crit("Could not find s3k,mcr3000-opx-gpio node\n");
	
	of_platform_bus_probe(NULL, of_bus_ids, NULL);

	np = of_find_compatible_node(NULL, NULL, "s3k,mcr3000-cpld");
	if (np) {
		int ngpios = of_gpio_count(np);
		int i=0;
		for (; i < ngpios; i++) {
			int gpio;
			int ret;
			enum of_gpio_flags flags;

			gpio = of_get_gpio_flags(np, i, &flags);
			if (!gpio_is_valid(gpio)) {
				pr_err("invalid gpio #%d: %d\n", i, gpio);
				continue;
			}

			ret = gpio_request(gpio, __func__);
			if (ret) {
				pr_err("can't request gpio #%d: %d\n", i, ret);
				continue;
			}

			ret = gpio_direction_output(gpio, flags & OF_GPIO_ACTIVE_LOW);
			if (ret) {
				pr_err("can't set output direction for gpio #%d: %d\n", i, ret);
				continue;
			}
			switch (i) { /* traitement specifique pour chaque GPIO */
			case 0: /* SPISEL */
			case 1: /* masque IRQ CPLD */
			case 2: /* valider ethernet 2 */
				/* activation SPISEL permanent */
				/* activation IRQ CPLD permanent */
				gpio_set_value(gpio, !(flags & OF_GPIO_ACTIVE_LOW));
				break;
			}
		}
		if (!ngpios) {
			pr_err("Pas de gpio defini dans mcr3000-cpld, impossible de positionner SPISEL\n");
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
