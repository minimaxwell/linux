#ifndef _SAF3000_H
#define _SAF3000_H

#include <linux/device.h>

extern struct class *saf3000_class_get(void);

extern void __init u16_gpiochip_init(const char *);

extern void fpga_clk_set_brg(void);
		
#endif
