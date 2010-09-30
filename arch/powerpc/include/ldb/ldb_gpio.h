#ifndef __LDB_GPIO_H
#define __LDB_GPIO_H

#include <linux/of_gpio.h>

/*
 * Fonctions pour faciliter l'utilisation des gpio dans le LDB S3K
 */

typedef int ldb_gpio_t;

#define _GPIO_MASK	0x3fffffff
#define _GPIO_INV	0x40000000

static inline int ldb_gpio_init(struct of_device *ofdev, int idx, int dir)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np = ofdev->node;
	int _gpio, _result;
	enum of_gpio_flags flags;

	_gpio = of_get_gpio_flags(np, idx, &flags);

	if (!gpio_is_valid(_gpio)) {
		dev_err(dev, "invalid gpio #%d: %d\n", idx, _gpio);
		goto erreur;
	}
	if (_gpio>_GPIO_MASK) {
		dev_err(dev, "gpio trop grand #%d: %d\n", idx, _gpio);
		goto erreur;
	}

	_result = gpio_request(_gpio, dev_driver_string(dev));
	if (_result) {
		dev_err(dev, "can't request gpio #%d: %d\n", idx, _result);
		goto erreur;
	}

	if (dir)
		/* init a la valeur de repos */
		_result = gpio_direction_output(_gpio, flags & OF_GPIO_ACTIVE_LOW?1:0);
	else
		_result = gpio_direction_input(_gpio);
	if (_result) {
		dev_err(dev, "can't set direction for gpio #%d: %d\n", idx, _result);
		goto erreur_gpio;
	}
	
	if (flags & OF_GPIO_ACTIVE_LOW) {
		_gpio |= _GPIO_INV;
	}
	return _gpio;
	
erreur_gpio:
	gpio_free(_gpio);
erreur:
	return -1;
}

static inline int ldb_gpio_is_valid(int gpio)
{
	return gpio_is_valid(gpio&_GPIO_MASK);
}

static inline void ldb_gpio_free(int gpio)
{
	gpio_free(gpio&_GPIO_MASK);
}	

static inline void ldb_gpio_set_value(int gpio, int val)
{
	gpio_set_value(gpio&_GPIO_MASK, gpio&_GPIO_INV?!val:val);
}

static inline int ldb_gpio_get_value(int gpio)
{
	int val = gpio_get_value(gpio&_GPIO_MASK);
	return gpio&_GPIO_INV?!val:val;
}

#endif /* __LDB_GPIO_H */
