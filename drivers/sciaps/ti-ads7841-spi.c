/*
 * Copyright 2019 SciAps
 *
 *
 *
 *
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mc13xxx.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/spi/spi.h>

#include "sciaps-driver.h"

#define SCIAPS_TI_ADS7841_CH0_Bit	0x01
#define SCIAPS_TI_ADS7841_CH1_Bit	0x02
#define SCIAPS_TI_ADS7841_CH2_Bit	0x04
#define SCIAPS_TI_ADS7841_CH3_Bit	0x08

#define SCIAPS_TI_ADS7841_Start_Start		(0x80)

#define SCIAPS_TI_ADS7841_Address_Shift		4
#define SCIAPS_TI_ADS7841_Address_CH0		(0x1 << SCIAPS_TI_ADS7841_Address_Shift)
#define SCIAPS_TI_ADS7841_Address_CH1		(0x5 << SCIAPS_TI_ADS7841_Address_Shift)
#define SCIAPS_TI_ADS7841_Address_CH2		(0x2 << SCIAPS_TI_ADS7841_Address_Shift)
#define SCIAPS_TI_ADS7841_Address_CH3		(0x6 << SCIAPS_TI_ADS7841_Address_Shift)

// If MODE pin is high
#define SCIAPS_TI_ADS7841_Mode_Shift		3
#define SCIAPS_TI_ADS7841_Mode_8bit			(0x1 << SCIAPS_TI_ADS7841_Mode_Shift)
#define SCIAPS_TI_ADS7841_Mode_12bit		(0x0)

#define SCIAPS_TI_ADS7841_InputMode_Shift		2
#define SCIAPS_TI_ADS7841_InputMode_Single		(0x1 << SCIAPS_TI_ADS7841_InputMode_Shift)
#define SCIAPS_TI_ADS7841_InputMode_Diff		(0x0)

#define SCIAPS_TI_ADS7841_PDMode_Shift			0
#define SCIAPS_TI_ADS7841_PDMode_Sleep			(0x0)
#define SCIAPS_TI_ADS7841_InputMode_Active		(0x3)


struct sciaps_data_t {
	uint8_t				_device_id;
	struct mutex		_lock;
	struct spi_device*	_spi;
	union device_data_t {
		struct ti_ads7841_t {
			uint16_t			_ch0;
			uint16_t			_ch1;
			uint16_t			_ch2;
			uint16_t			_ch3;
		} _ti_ads7841;
	} _data;
};

static const struct spi_device_id sciaps_ti_ads7841_spi_device_id_table[] = {
	{ .name = "ads7841",	.driver_data = (kernel_ulong_t)SCIAPS_DRIVER_ID_TI_ADS7841_SPI,	}, 
	{}
};
MODULE_DEVICE_TABLE(spi, sciaps_ti_ads7841_spi_device_id_table);

static const struct of_device_id sciaps_ti_ads7841_spi_of_match_table[] = {
	{ .compatible = "ti,ads7841",	.data = &sciaps_ti_ads7841_spi_device_id_table[0],	}, 
	{}
};
MODULE_DEVICE_TABLE(of, sciaps_ti_ads7841_spi_of_match_table);


#define SCIAPS_TI_ADS7841_MAX_TX_DATA_LENGTH	4
#define SCIAPS_TI_ADS7841_MAX_RX_DATA_LENGTH	8
#define SCIAPS_TI_ADS7841_SPI_SPEED_HZ			SCIAPS_SPI_SPEED_HZ_Default

static char ads7841_tx_buffer[SCIAPS_TI_ADS7841_MAX_TX_DATA_LENGTH];
static char ads7841_rx_buffer[SCIAPS_TI_ADS7841_MAX_RX_DATA_LENGTH];

static struct sciaps_data_t* ads7841_spi_check_and_get_data(struct spi_device *spi)
{
	struct sciaps_data_t* data;
	uint8_t device_id = SCIAPS_DRIVER_ID_TI_ADS7841_SPI;
	if (!spi) {
		return NULL;
	}
	data = spi_get_drvdata(spi);
	if (!data) {
		return NULL;
	}
	if (data->_device_id != device_id) {
		return NULL;
	}
	return data;
}

static int ads7841_spi_adc_convert(struct spi_device *spi, uint8_t mask, bool lock_it) 
{
	struct sciaps_data_t* data = ads7841_spi_check_and_get_data(spi);
	int tx_offset = 0;
	uint8_t control = 0;
	uint16_t* ch = NULL;
	int ret;
	
	if (!data) {
		return -EINVAL;
	}
	
	ret = 0;

	if (lock_it)
		mutex_lock(&data->_lock);
	{	

		control = 0;


		if (SCIAPS_TI_ADS7841_CH0_Bit == (mask & SCIAPS_TI_ADS7841_CH0_Bit)) {
			control = SCIAPS_TI_ADS7841_Address_CH0;
			ch = &(data->_data._ti_ads7841._ch0);
		}
		else if (SCIAPS_TI_ADS7841_CH1_Bit == (mask & SCIAPS_TI_ADS7841_CH1_Bit)) {
			control = SCIAPS_TI_ADS7841_Address_CH1;
			ch = &(data->_data._ti_ads7841._ch1);
		}
		else if (SCIAPS_TI_ADS7841_CH2_Bit == (mask & SCIAPS_TI_ADS7841_CH2_Bit)) {
			control = SCIAPS_TI_ADS7841_Address_CH2;
			ch = &(data->_data._ti_ads7841._ch2);
		}
		else if (SCIAPS_TI_ADS7841_CH3_Bit == (mask & SCIAPS_TI_ADS7841_CH3_Bit)) {
			control = SCIAPS_TI_ADS7841_Address_CH3;
			ch = &(data->_data._ti_ads7841._ch3);
		}
		
		if (!control) {
			ret = -ENODEV;
		}
		else {
			control |= SCIAPS_TI_ADS7841_Start_Start;
			control |= SCIAPS_TI_ADS7841_Mode_12bit;
			control |= SCIAPS_TI_ADS7841_InputMode_Single;
			control |= SCIAPS_TI_ADS7841_PDMode_Sleep;


			dev_info(&spi->dev, "ads7841_spi_adc_convert: control = 0x%x", control);
			
			{
				struct spi_message		msg;
				struct spi_transfer		xfer[2];
				tx_offset = 0;

				memset(&msg,0x00,sizeof (msg));
				memset(&xfer,0x00,sizeof (xfer));
				memset(&ads7841_tx_buffer,0x00,sizeof (ads7841_tx_buffer));
				memset(&ads7841_rx_buffer,0x00,sizeof (ads7841_rx_buffer));

				ads7841_tx_buffer[tx_offset++] = control;

				xfer[0].tx_buf			= ads7841_tx_buffer;
				xfer[0].rx_buf			= NULL;

				xfer[0].speed_hz		= SCIAPS_TI_ADS7841_SPI_SPEED_HZ;
				//xfer[0].delay_usecs	= 2;
				xfer[0].cs_change		= 0;
				xfer[0].len				= tx_offset;

				xfer[1].tx_buf		= NULL;
				xfer[1].rx_buf		= ads7841_rx_buffer;
				xfer[1].speed_hz	= SCIAPS_TI_ADS7841_SPI_SPEED_HZ;
				xfer[1].cs_change	= 1;
				xfer[1].len			= 2;


				spi_message_init(&msg);
				spi_message_add_tail(&xfer[0], &msg);
				spi_message_add_tail(&xfer[1], &msg);
				ret = spi_sync(spi, &msg);	
				if (ret < 0) {
					dev_info(&spi->dev, "dac7513_spi_cmd_dac_data: spi_sync failed with errno = %d", ret);
				}

				if (ch) {
					uint16_t value;
					value = ads7841_rx_buffer[0];
					value <<= 5;
					value |= (ads7841_rx_buffer[1] >> 3);
					*ch = value;
				}

			}
		}
	}
	if (lock_it)
		mutex_unlock(&data->_lock);	
		
	return ret;
}

static ssize_t ads7841_adc_ch0_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ads7841_spi_check_and_get_data(spi);
	ssize_t ret = 0;
	uint16_t value;
	
	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		ret = ads7841_spi_adc_convert(spi, SCIAPS_TI_ADS7841_CH0_Bit, false);
		value = data->_data._ti_ads7841._ch0;
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ads7841_adc_ch1_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ads7841_spi_check_and_get_data(spi);
	ssize_t ret = 0;
	uint16_t value;
	
	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		ret = ads7841_spi_adc_convert(spi, SCIAPS_TI_ADS7841_CH1_Bit, false);
		value = data->_data._ti_ads7841._ch1;
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ads7841_adc_ch2_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ads7841_spi_check_and_get_data(spi);
	ssize_t ret = 0;
	uint16_t value;
	
	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		ret = ads7841_spi_adc_convert(spi, SCIAPS_TI_ADS7841_CH2_Bit, false);
		value = data->_data._ti_ads7841._ch2;
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ads7841_adc_ch3_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ads7841_spi_check_and_get_data(spi);
	ssize_t ret = 0;
	uint16_t value;
	
	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		ret = ads7841_spi_adc_convert(spi, SCIAPS_TI_ADS7841_CH3_Bit, false);
		value = data->_data._ti_ads7841._ch3;
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static DEVICE_ATTR(adc_ch0,				0444,	ads7841_adc_ch0_show,			NULL);  
static DEVICE_ATTR(adc_ch1,				0444,	ads7841_adc_ch1_show,			NULL);  
static DEVICE_ATTR(adc_ch2,				0444,	ads7841_adc_ch2_show,			NULL);  
static DEVICE_ATTR(adc_ch3,				0444,	ads7841_adc_ch3_show,			NULL);  

static int ads7841_create_files(struct spi_device *spi)
{
	int ret = -EINVAL;
	if ((ret = device_create_file(&spi->dev, &dev_attr_adc_ch0)) == 0 ) {
		if ((ret = device_create_file(&spi->dev, &dev_attr_adc_ch1)) == 0 ) {
			if ((ret = device_create_file(&spi->dev, &dev_attr_adc_ch2)) == 0 ) {
				if ((ret = device_create_file(&spi->dev, &dev_attr_adc_ch3)) == 0 ) {
					ret = 0;
				}
				else {
					device_remove_file(&spi->dev, &dev_attr_adc_ch0);
					device_remove_file(&spi->dev, &dev_attr_adc_ch1);
					device_remove_file(&spi->dev, &dev_attr_adc_ch2);
					device_remove_file(&spi->dev, &dev_attr_adc_ch2);
				}				
			}
			else {
				device_remove_file(&spi->dev, &dev_attr_adc_ch0);
				device_remove_file(&spi->dev, &dev_attr_adc_ch1);
			}
		}
		else {
			device_remove_file(&spi->dev, &dev_attr_adc_ch0);
		}
	}
	return ret;
}

static void ads7841_remove_files(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_adc_ch0);
	device_remove_file(&spi->dev, &dev_attr_adc_ch1);
	device_remove_file(&spi->dev, &dev_attr_adc_ch2);
	device_remove_file(&spi->dev, &dev_attr_adc_ch3);
}

static int sciaps_ti_ads7841_spi_probe(struct spi_device *spi)
{

	int ret = 0;
	struct sciaps_data_t* data;

	if (!spi) {
		dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: Invalid params;");
		return -EINVAL;
	}

	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->max_speed_hz = %d;", spi->max_speed_hz);
	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->chip_select = %d;", spi->chip_select);
	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->mode = %d;", spi->mode);
	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->bits_per_word = %d;", spi->bits_per_word);
	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->modalias = %s;", spi->modalias);
	dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->cs_gpio = %d;", spi->cs_gpio);

	
	{
		uint8_t device_id = 0;
		const struct of_device_id *of_id =  of_match_device(of_match_ptr(sciaps_ti_ads7841_spi_of_match_table), &spi->dev);
		
		if (!of_id) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: there is NO match for %s;", spi->modalias);
			return ret;
		}
		if (!of_id->data) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: there is NO .data for %s;", spi->modalias);
			return ret;
		}
		device_id = (uint8_t)((const struct spi_device_id*)(of_id->data))->driver_data;
		dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: there is a match for spi->%s of device %d;", spi->modalias, device_id);
		if (device_id != SCIAPS_DRIVER_ID_TI_ADS7841_SPI) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_dac7513_spi_probe: Invalid device ID for %s;", spi->modalias);
			return ret;
		}

		{
			uint8_t spi_cpol = 0;	/*SPI_CPOL: clock polarity: 0 - a clock idles at 0; 1 - a clock idles at 1; */
			uint8_t spi_cpha = 0;	/*SPI_CPHA: clock phase: 1 - 'out' changes on the leading edge; 0 - 'out' changes on the trailing edge*/
			
			dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: processing for SCIAPS_DRIVER_ID_TI_ADS7841_SPI...");
			spi->bits_per_word = 8;
			dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->bits_per_word = %d now;", spi->bits_per_word);


			spi->mode = (spi_cpha|spi_cpol);
			dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi->mode = %d now;", spi->mode);
			data = devm_kzalloc(&spi->dev, sizeof(struct sciaps_data_t), GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: unable to allocate buffer for struct sciaps_ti_dac7513 for %s", spi->modalias);
				return ret;
			}
			
			data->_device_id					= device_id;
			data->_spi							= spi;
			data->_data._ti_ads7841._ch0		= 0;
			data->_data._ti_ads7841._ch1		= 0;
			data->_data._ti_ads7841._ch2		= 0;
			data->_data._ti_ads7841._ch3		= 0;


			if (ads7841_create_files(spi) != 0 ) {
				ret = -ENODEV;
				dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: unable to create device files for for sciaps_ti_ads7841");
				return ret;
			}
			mutex_init(&data->_lock);
			spi_set_drvdata(spi, data);
			dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: drvdata set for %s;", spi->modalias);
		       	       
		}
		ret = spi_setup(spi);
		if (ret < 0) {
			dev_err(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi_setup failed  for %s with errno: %d;", spi->modalias, ret);
			return ret;
		} 
		else {
			dev_info(&spi->dev, "sciaps_ti_ads7841_spi_probe: spi_setup success for %s;", spi->modalias);
		}
		
	}
	return 0;
}

static int sciaps_ti_ads7841_spi_remove(struct spi_device *spi)
{
	if (spi) {
		struct sciaps_data_t* data = spi_get_drvdata(spi);

		if (data) {
			ads7841_remove_files(spi);
			mutex_destroy(&data->_lock);
		}
	}
	dev_err(&spi->dev, "sciaps_ti_ads7841_spi_remove: spi->modalias = %s;", spi->modalias);
	return 0;
}

static struct spi_driver sciaps_ti_ads7841_spi_driver = {
	.id_table = sciaps_ti_ads7841_spi_device_id_table,
	.driver = {
		.name = "sciaps-ti-ads7841-spi",
		.owner = THIS_MODULE,
		.of_match_table = sciaps_ti_ads7841_spi_of_match_table,
	},
	.probe = sciaps_ti_ads7841_spi_probe,
	.remove = sciaps_ti_ads7841_spi_remove,
};

static int __init sciaps_ti_ads7841_spi_init(void)
{
	return spi_register_driver(&sciaps_ti_ads7841_spi_driver);
}
subsys_initcall(sciaps_ti_ads7841_spi_init);

static void __exit sciaps_ti_ads7841_spi_exit(void)
{
	spi_unregister_driver(&sciaps_ti_ads7841_spi_driver);
}
module_exit(sciaps_ti_ads7841_spi_exit);

MODULE_DESCRIPTION("SciAps TI ADS7841 SPI Driver");
MODULE_AUTHOR("Andre Doudkin");
MODULE_LICENSE("GPL v2");

