/*
 * AD7923 SPI ADC driver
 *
 * Copyright 2012
 *
 * Licensed under the GPL-2.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/module.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>

#include <saf3000/saf3000.h>

#include "ad7923.h"

#define AD7923_V_CHAN(index)						\
	{								\
		.type = IIO_VOLTAGE,					\
		.indexed = 1,						\
		.channel = index,					\
		.info_mask = IIO_CHAN_INFO_RAW_SEPARATE_BIT |		\
		IIO_CHAN_INFO_SCALE_SHARED_BIT,				\
		.address = index,					\
		.scan_index = index,					\
		.scan_type = {						\
			.sign = 'u',					\
			.realbits = 12,					\
			.storagebits = 16,				\
		},							\
	}

static struct iio_chan_spec ad7923_channels[] = {
	AD7923_V_CHAN(0),
	AD7923_V_CHAN(1),
	AD7923_V_CHAN(2),
	AD7923_V_CHAN(3),
	IIO_CHAN_SOFT_TIMESTAMP(4),
};

static int ad7923_scan_direct(struct ad7923_state *st, unsigned ch)
{
	int ret, cmd;

	cmd = AD7923_WRITE_CR | AD7923_PM_MODE_WRITE(AD7923_PM_MODE_OPS) |
		AD7923_SEQUENCE_WRITE(AD7923_SEQUENCE_OFF) | AD7923_CODING |
		AD7923_CHANNEL_WRITE(ch) | AD7923_RANGE;
	cmd <<= AD7923_SHIFT_REGISTER;
	st->tx_buf[0] = cpu_to_be16(cmd);

	ret = spi_sync(st->spi, &st->scan_single_msg);
	if (ret)
		return ret;

	return be16_to_cpu(st->rx_buf[0]);
}

static int ad7923_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val,
			   int *val2,
			   long m)
{
	int ret;
	struct ad7923_state *st = iio_priv(indio_dev);

	switch (m) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&indio_dev->mlock);
		if (iio_buffer_enabled(indio_dev))
			ret = -EBUSY;
		else
			ret = ad7923_scan_direct(st, chan->address);
		mutex_unlock(&indio_dev->mlock);

		if (ret < 0)
			return ret;
		if (chan->address == EXTRACT(ret, 12, 4)) {
			*val = EXTRACT(ret, 0, 12);
			*val2 = EXTRACT_PERCENT(*val, 12);
		}
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static const struct iio_info ad7923_info = {
	.read_raw = &ad7923_read_raw,
	.update_scan_mode = ad7923_update_scan_mode,
	.driver_module = THIS_MODULE,
};

static int __devinit ad7923_probe(struct spi_device *spi)
{
	struct ad7923_state *st;
	struct device *dev = &spi->dev;
	int ret;
	struct iio_dev *indio_dev = iio_device_alloc(sizeof(*st));

#ifdef AD7923_USE_CS
	ret = type_fav();
	if ((ret != FAV_NVCS_LEMO) && (ret != FAV_NVCS_FISCHER)) {
		dev_err(dev, "Driver AD7923 is not added (FAV = %d).\n", ret);
		return -ENODEV;
	}
#endif

	if (indio_dev == NULL)
		return -ENOMEM;

	st = iio_priv(indio_dev);

	st->reg = regulator_get(&spi->dev, "vcc");
	if (!IS_ERR(st->reg)) {
		ret = regulator_enable(st->reg);
		if (ret)
			goto error_put_reg;
	}

	spi_set_drvdata(spi, indio_dev);

	st->spi = spi;

	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->dev.parent = &spi->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = ad7923_channels;
	indio_dev->num_channels = ARRAY_SIZE(ad7923_channels);
	indio_dev->info = &ad7923_info;

	/* Setup default message */
	st->scan_single_xfer[0].tx_buf = &st->tx_buf[0];
	st->scan_single_xfer[0].len = 2;
	st->scan_single_xfer[0].cs_change = 1;
	st->scan_single_xfer[1].rx_buf = &st->rx_buf[0];
	st->scan_single_xfer[1].len = 2;

	spi_message_init(&st->scan_single_msg);
	spi_message_add_tail(&st->scan_single_xfer[0], &st->scan_single_msg);
	spi_message_add_tail(&st->scan_single_xfer[1], &st->scan_single_msg);

	ret = ad7923_register_ring_funcs_and_init(indio_dev);
	if (ret)
		goto error_disable_reg;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_cleanup_ring;

	dev_info(dev, "Driver AD7923 is added.\n");

	return 0;

error_cleanup_ring:
	ad7923_ring_cleanup(indio_dev);
error_disable_reg:
	if (!IS_ERR(st->reg))
		regulator_disable(st->reg);
error_put_reg:
	if (!IS_ERR(st->reg))
		regulator_put(st->reg);
	iio_device_free(indio_dev);

	return ret;
}

static int __devexit ad7923_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct ad7923_state *st = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	ad7923_ring_cleanup(indio_dev);
	if (!IS_ERR(st->reg)) {
		regulator_disable(st->reg);
		regulator_put(st->reg);
	}
	iio_device_free(indio_dev);

	return 0;
}

static const struct spi_device_id ad7923_id[] = {
	{"ad7923", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, ad7923_id);

static struct spi_driver ad7923_driver = {
	.driver = {
		.name	= "ad7923",
		.owner	= THIS_MODULE,
	},
	.probe		= ad7923_probe,
	.remove		= __devexit_p(ad7923_remove),
	.id_table	= ad7923_id,
};
module_spi_driver(ad7923_driver);

MODULE_AUTHOR("Patrick Vasseur <patrick.vasseur@c-s.fr>");
MODULE_DESCRIPTION("Analog Devices AD7923 ADC");
MODULE_LICENSE("GPL v2");
