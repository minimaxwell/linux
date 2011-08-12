/*
 * saf3000_class.c - MCR3000 2G FPGAF information
 *
 * Authors: Christophe LEROY
 * Copyright (c) 2010  CSSI
 *
 */

#include <linux/device.h>

static struct class *saf3000_class = NULL;

struct class *saf3000_class_get(void)
{
	if (saf3000_class == NULL) {
		saf3000_class = class_create(THIS_MODULE, "saf3000");
	}
	return saf3000_class;
}

EXPORT_SYMBOL(saf3000_class_get);
