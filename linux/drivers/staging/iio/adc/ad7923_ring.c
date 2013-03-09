/*
 * AD7923 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc (from AD7298 Driver)
 * Copyright 2012 CS Systemes d'Information
 *
 * Licensed under the GPL-2.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "ad7923.h"

/**
 * ad7923_update_scan_mode() setup the spi transfer buffer for the new scan mask
 **/
int ad7923_update_scan_mode(struct iio_dev *indio_dev,
	const unsigned long *active_scan_mask)
{
	struct ad7923_state *st = iio_priv(indio_dev);
	int i, cmd, len;

	len = 0;
	for_each_set_bit(i, active_scan_mask, AD7923_MAX_CHAN) {
		cmd = AD7923_WRITE_CR | AD7923_CODING | AD7923_RANGE |
			AD7923_PM_MODE_WRITE(AD7923_PM_MODE_OPS) |
			AD7923_SEQUENCE_WRITE(AD7923_SEQUENCE_OFF) |
			AD7923_CHANNEL_WRITE(i);
		cmd <<= AD7923_SHIFT_REGISTER;
		st->tx_buf[len++] = cpu_to_be16(cmd);
	}

	/* build spi ring message */
	for (i = 0; i < len; i++) {
		st->ring_xfer[i].tx_buf = &st->tx_buf[i];
		st->ring_xfer[i].len = 2;
		st->ring_xfer[i].cs_change = 1;
	}

	spi_message_init(&st->ring_msg);
	spi_message_add_tail(&st->ring_xfer[0], &st->ring_msg);

	/* first transfer is invalid data */
	for (i = 0; i < len; i++) {
		st->ring_xfer[i + 1].rx_buf = &st->rx_buf[i];
		st->ring_xfer[i + 1].len = 2;
		st->ring_xfer[i + 1].cs_change = 1;
		spi_message_add_tail(&st->ring_xfer[i + 1], &st->ring_msg);
	}
	/* make sure last transfer cs_change is not set */
	st->ring_xfer[i + 1].cs_change = 0;

	return 0;
}

/**
 * ad7923_trigger_handler() bh of trigger launched polling to ring buffer
 *
 * Currently there is no option in this driver to disable the saving of
 * timestamps within the ring.
 **/
static irqreturn_t ad7923_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct ad7923_state *st = iio_priv(indio_dev);
	s64 time_ns = 0;
	__u16 buf[16];
	int b_sent, i;

	b_sent = spi_sync(st->spi, &st->ring_msg);
	if (b_sent)
		goto done;

	if (indio_dev->scan_timestamp) {
		time_ns = iio_get_time_ns();
		memcpy((u8 *)buf + indio_dev->scan_bytes - sizeof(s64),
			&time_ns, sizeof(time_ns));
	}

	for (i = 0; i < bitmap_weight(indio_dev->active_scan_mask,
						 AD7923_MAX_CHAN); i++)
		buf[i] = be16_to_cpu(st->rx_buf[i]);

	iio_push_to_buffers(indio_dev, (u8 *)buf);

done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

int ad7923_register_ring_funcs_and_init(struct iio_dev *indio_dev)
{
	return iio_triggered_buffer_setup(indio_dev, NULL,
			&ad7923_trigger_handler, NULL);
}

void ad7923_ring_cleanup(struct iio_dev *indio_dev)
{
	iio_triggered_buffer_cleanup(indio_dev);
}
