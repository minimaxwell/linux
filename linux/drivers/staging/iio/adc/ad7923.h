/*
 * AD7923 SPI ADC driver
 *
 * Copyright 2012
 *
 * Licensed under the GPL-2 or later.
 */
#ifndef IIO_ADC_AD7923_H_
#define IIO_ADC_AD7923_H_

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
#define AD7923_SEQUENCE_LAST	(2)		/* terminating sequence cycle */
#define AD7923_SEQUENCE_ON	(3)		/* continuous sequence */

#define AD7923_PM_MODE_WRITE(mode)	(mode << 4)	/* write mode */
#define AD7923_CHANNEL_WRITE(channel)	(channel << 6)	/* write channel */
#define AD7923_SEQUENCE_WRITE(sequence)	(((sequence & 1) << 3) \
					+ ((sequence & 2) << 9))
						/* write sequence fonction */

/* val = valeur, dec = decalage à gauche, bits = nombre de bits */
#define EXTRACT(val,dec,bits)		((val >> dec) & ((1 << bits) - 1))
#define EXTRACT_PERCENT(val,bits)	(((val + 1) * 100) >> bits)

struct ad7923_platform_data {
	/* External Vref voltage applied */
	u16				vref_mv;
};

struct ad7923_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	struct spi_transfer		ring_xfer[10];
	struct spi_transfer		scan_single_xfer[2];
	struct spi_message		ring_msg;
	struct spi_message		scan_single_msg;
	/*
	 * DMA (thus cache coherency maintenance) requires the
	 * transfer buffers to live in their own cache lines.
	 */
	unsigned short			rx_buf[8] ____cacheline_aligned;
	unsigned short			tx_buf[2];
};

//#ifdef CONFIG_IIO_BUFFER
//int ad7923_register_ring_funcs_and_init(struct iio_dev *indio_dev);
//void ad7923_ring_cleanup(struct iio_dev *indio_dev);
//#else /* CONFIG_IIO_BUFFER */

static inline int
ad7923_register_ring_funcs_and_init(struct iio_dev *indio_dev)
{
        return 0;
}

static inline void ad7923_ring_cleanup(struct iio_dev *indio_dev)
{
}
//#endif /* CONFIG_IIO_BUFFER */
#endif /* IIO_ADC_AD7923_H_ */
