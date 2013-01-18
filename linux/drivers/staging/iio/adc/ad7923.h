/*
 * AD7923 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc (from AD7298 Driver)
 * Copyright 2012 CS Systemes d'Information
 *
 * Licensed under the GPL-2 or later.
 */
#ifndef IIO_ADC_AD7923_H_
#define IIO_ADC_AD7923_H_

#define AD7923_USE_CS

#ifdef AD7923_USE_CS
struct convert_ident {
	unsigned short	min;
	unsigned short	max;
};
int ad7923_convert(int channel, int val);
#endif

#define AD7923_WRITE_CR		(1 << 11)	/* write control register */
#define AD7923_RANGE		(1 << 1)	/* range to REFin */
#define AD7923_CODING		(1 << 0)	/* coding is straight binary */
#define AD7923_PM_MODE_AS	(1)		/* auto shutdown */
#define AD7923_PM_MODE_FS	(2)		/* full shutdown */
#define AD7923_PM_MODE_OPS	(3)		/* normal operation */
#define AD7923_CHANNEL_0	(0)		/* analog input 0 */
#define AD7923_CHANNEL_1	(1)		/* analog input 1 */
#define AD7923_CHANNEL_2	(2)		/* analog input 2 */
#define AD7923_CHANNEL_3	(3)		/* analog input 3 */
#define AD7923_SEQUENCE_OFF	(0)		/* no sequence fonction */
#define AD7923_SEQUENCE_PROTECT	(2)		/* no interrupt write cycle */
#define AD7923_SEQUENCE_ON	(3)		/* continuous sequence */

#define AD7923_MAX_CHAN		4

#define AD7923_PM_MODE_WRITE(mode)	(mode << 4)	/* write mode */
#define AD7923_CHANNEL_WRITE(channel)	(channel << 6)	/* write channel */
#define AD7923_SEQUENCE_WRITE(sequence)	(((sequence & 1) << 3) \
					+ ((sequence & 2) << 9))
						/* write sequence fonction */
/* left shift for CR : bit 11 transmit in first */
#define AD7923_SHIFT_REGISTER	4

/* val = value, dec = left shift, bits = number of bits of the mask */
#define EXTRACT(val, dec, bits)		((val >> dec) & ((1 << bits) - 1))
/* val = value, bits = number of bits of the original value */
#define EXTRACT_PERCENT(val, bits)	(((val + 1) * 100) >> bits)

struct ad7923_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	struct spi_transfer		ring_xfer[6];
	struct spi_transfer		scan_single_xfer[2];
	struct spi_message		ring_msg;
	struct spi_message		scan_single_msg;
	/*
	 * DMA (thus cache coherency maintenance) requires the
	 * transfer buffers to live in their own cache lines.
	 */
	unsigned short			rx_buf[4] ____cacheline_aligned;
	unsigned short			tx_buf[2];
};

#ifdef CONFIG_IIO_BUFFER
int ad7923_register_ring_funcs_and_init(struct iio_dev *indio_dev);
void ad7923_ring_cleanup(struct iio_dev *indio_dev);
int ad7923_update_scan_mode(struct iio_dev *indio_dev,
	const unsigned long *active_scan_mask);
#else /* CONFIG_IIO_BUFFER */
static inline int
ad7923_register_ring_funcs_and_init(struct iio_dev *indio_dev)
{
	return 0;
}
static inline void ad7923_ring_cleanup(struct iio_dev *indio_dev)
{
}
#define ad7923_update_scan_mode NULL
#endif /* CONFIG_IIO_BUFFER */
#endif /* IIO_ADC_AD7923_H_ */
