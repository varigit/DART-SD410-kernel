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

#define SCIAPS_LT_LTC1658_DATA_Mask						0x3fff //16383
#define SCIAPS_LT_LTC1658_DATA_Shift					2

#define SCIAPS_LT_LTC1658_DATA_Default					0

#define SCIAPS_LT_LTC1658_DATA_MAX						SCIAPS_LT_LTC1658_DATA_Mask						


struct sciaps_data_t {
	uint8_t				_device_id;
	struct mutex		_lock;
	struct spi_device*	_spi;
	union device_data_t {
		struct lt_ltc1658_t {
			uint16_t			_data;
		} _lt_ltc1658;
	} _data;
};

static const struct spi_device_id sciaps_lt_ltc1658_spi_device_id_table[] = {
	{ .name = "ltc1658",	.driver_data = (kernel_ulong_t)SCIAPS_DRIVER_ID_LT_LTC1658_SPI,	}, 
	{}
};
MODULE_DEVICE_TABLE(spi, sciaps_lt_ltc1658_spi_device_id_table);

static const struct of_device_id sciaps_lt_ltc1658_spi_of_match_table[] = {
	{ .compatible = "lt,ltc1658",	.data = &sciaps_lt_ltc1658_spi_device_id_table[0]	},
	{}
};
MODULE_DEVICE_TABLE(of, sciaps_lt_ltc1658_spi_of_match_table);


#define SCIAPS_LT_LTC1658_MAX_DATA_LENGTH	4
#define SCIAPS_LT_LTC1658_SPI_SPEED_HZ		SCIAPS_SPI_SPEED_HZ_Default
static char ltc1658_tx_buffer[SCIAPS_LT_LTC1658_MAX_DATA_LENGTH];

static struct sciaps_data_t* ltc1658_spi_check_and_get_data(struct spi_device *spi)
{
	struct sciaps_data_t* data;
	uint8_t device_id = SCIAPS_DRIVER_ID_LT_LTC1658_SPI;
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
	
static int ltc1658_spi_cmd_dac_data_set(struct spi_device *spi, bool lock_it) {
	struct sciaps_data_t* data = ltc1658_spi_check_and_get_data(spi);
	int offset = 0;
	uint16_t value = 0;
	int ret;

	if (!data) {
		return -EINVAL;
	}

	if (lock_it)
		mutex_lock(&data->_lock);
	{	

		value = (data->_data._lt_ltc1658._data & SCIAPS_LT_LTC1658_DATA_Mask);
		value <<=  SCIAPS_LT_LTC1658_DATA_Shift;
		dev_info(&spi->dev, "ltc1658_spi_cmd_dac_data: value = 0x%x", value);
		
		{
			struct spi_message		msg;
			struct spi_transfer		xfer;
			offset = 0;
			memset(&msg,0x00,sizeof (msg));
			memset(&xfer,0x00,sizeof (xfer));
			ltc1658_tx_buffer[offset++] = (uint8_t)(value>>8);
			ltc1658_tx_buffer[offset++] = (uint8_t)(value);
			
			xfer.tx_buf		= ltc1658_tx_buffer;
			xfer.rx_buf		= NULL;
			xfer.speed_hz	= SCIAPS_LT_LTC1658_SPI_SPEED_HZ;
			xfer.len		= offset;
			xfer.cs_change	= 1;

			spi_message_init(&msg);
			spi_message_add_tail(&xfer, &msg);
			ret = spi_sync(spi, &msg);	
			
			if (ret < 0) {
				dev_info(&spi->dev, "ltc1658_spi_cmd_dac_data: spi_sync failed with errno = %d", ret);
			}

		}
	}
	if (lock_it)
		mutex_unlock(&data->_lock);	
	
	return ret;
}

static ssize_t ltc1658_dac_data_max_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ltc1658_spi_check_and_get_data(spi);
	uint16_t value = SCIAPS_LT_LTC1658_DATA_MAX;

	if (!data) {
		return -EINVAL;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ltc1658_dac_data_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ltc1658_spi_check_and_get_data(spi);
	uint16_t value = 0;

	if (!data) {
		return -EINVAL;
	}
	
	mutex_lock(&data->_lock);
	{
		value = data->_data._lt_ltc1658._data;
	}
	mutex_unlock(&data->_lock);

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t ltc1658_dac_data_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = ltc1658_spi_check_and_get_data(spi);
	uint16_t value = 0;
	int offset = 0;

	if (!data) {
		return -EINVAL;
	}
	
	if ((offset = kstrtou16(buf, 16, &value)) < 0) {
		return -EINVAL;
	}	
	

	mutex_lock(&data->_lock);
	{
		dev_info(child, "ltc1658_dac_data_store: value = 0x%x", value);
		
		data->_data._lt_ltc1658._data = value; 

		ltc1658_spi_cmd_dac_data_set(spi, false);
	}
	mutex_unlock(&data->_lock);

	return count;

}

static DEVICE_ATTR(dac_ch0,			0666,	ltc1658_dac_data_show,			ltc1658_dac_data_store);  
static DEVICE_ATTR(dac_ch0_max,		0444,	ltc1658_dac_data_max_show,		NULL);  

static int ltc1658_create_files(struct spi_device *spi)
{
	int ret = -EINVAL;
	if ((ret = device_create_file(&spi->dev, &dev_attr_dac_ch0)) == 0 ) {
		if ((ret = device_create_file(&spi->dev, &dev_attr_dac_ch0_max)) == 0 ) {
			ret = 0;
		}
		else {
			device_remove_file(&spi->dev, &dev_attr_dac_ch0);
		}
	}
	return ret;
}

static void ltc1658_remove_files(struct spi_device *spi)
{
	device_remove_file(&spi->dev, &dev_attr_dac_ch0);
	device_remove_file(&spi->dev, &dev_attr_dac_ch0_max);
}



static int sciaps_lt_ltc1658_spi_probe(struct spi_device *spi)
{

	int ret = 0;
	struct sciaps_data_t* data;

	if (!spi) {
		dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: Invalid params;");
		return -EINVAL;
	}

	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->max_speed_hz = %d;",		spi->max_speed_hz);
	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->chip_select = %d;",		spi->chip_select);
	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->mode = %d;",				spi->mode);
	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->bits_per_word = %d;",	spi->bits_per_word);
	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->modalias = %s;",			spi->modalias);
	dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->cs_gpio = %d;",			spi->cs_gpio);

	
	{
		uint8_t device_id = 0;
		const struct of_device_id *of_id =  of_match_device(sciaps_lt_ltc1658_spi_of_match_table, &spi->dev);
		
		if (!of_id) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: there is NO match for %s;", spi->modalias);
			return ret;
		}
		if (!of_id->data) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: there is NO .data for %s;", spi->modalias);
			return ret;
		}
		device_id = (uint8_t)((const struct spi_device_id*)(of_id->data))->driver_data;
		dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: there is a match for spi->%s of device %d;", spi->modalias, device_id);
		if (device_id != SCIAPS_DRIVER_ID_LT_LTC1658_SPI) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: Invalid device ID for %s;", spi->modalias);
			return ret;
		}
 
		{
			uint8_t spi_cpol = 0;	/*SPI_CPOL: clock polarity: 0 - a clock idles at 0; 1 - a clock idles at 1; */
			uint8_t spi_cpha = 0;	/*SPI_CPHA: clock phase: 1 - 'out' changes on the leading edge; 0 - 'out' changes on the trailing edge*/

			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: processing for SCIAPS_DRIVER_ID_LT_LTC1658_SPI...");
			spi->bits_per_word = 16;
			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->bits_per_word = %d now;", spi->bits_per_word);
			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: Clock idles at %d; Leading edge change: %d", spi_cpha, spi_cpol);

			spi->mode = (spi_cpha|spi_cpol);
			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi->mode = %d now;", spi->mode);
			
			data = devm_kzalloc(&spi->dev, sizeof(struct sciaps_data_t), GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: unable to allocate buffer for struct sciaps_lt_ltc1658 for %s", spi->modalias);
				return ret;
			}
				
			data->_device_id					= device_id;
			data->_spi							= spi;
			data->_data._lt_ltc1658._data		= SCIAPS_LT_LTC1658_DATA_Default;


			if (ltc1658_create_files(spi) != 0 ) {
				ret = -ENODEV;
				dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: unable to create device files for for sciaps_lt_ltc1658");
				return ret;
			}
			mutex_init(&data->_lock);
			spi_set_drvdata(spi, data);
			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: drvdata set for %s;", spi->modalias);
		       	       
		}
		ret = spi_setup(spi);
		if (ret < 0) {
			dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi_setup failed  for %s with errno: %d;", spi->modalias, ret);
			return ret;
		} 
		else {
			dev_info(&spi->dev, "sciaps_lt_ltc1658_spi_probe: spi_setup success for %s;", spi->modalias);
		}
	}
	return 0;
}

static int sciaps_lt_ltc1658_spi_remove(struct spi_device *spi)
{
	if (spi) {
		struct sciaps_data_t* data = spi_get_drvdata(spi);

		if (data) {
			mutex_destroy(&data->_lock);
			ltc1658_remove_files(spi);
		}
	}
	dev_err(&spi->dev, "sciaps_lt_ltc1658_spi_remove: spi->modalias = %s;", spi->modalias);
	return 0;
}

static struct spi_driver sciaps_lt_ltc1658_spi_driver = {
	.id_table = sciaps_lt_ltc1658_spi_device_id_table,
	.driver = {
		.name = "sciaps-lt-ltc1658-spi",
		.owner = THIS_MODULE,
		.of_match_table = sciaps_lt_ltc1658_spi_of_match_table,
	},
	.probe = sciaps_lt_ltc1658_spi_probe,
	.remove = sciaps_lt_ltc1658_spi_remove,
};

static int __init sciaps_lt_ltc1658_spi_init(void)
{
	return spi_register_driver(&sciaps_lt_ltc1658_spi_driver);
}
subsys_initcall(sciaps_lt_ltc1658_spi_init);

static void __exit sciaps_lt_ltc1658_spi_exit(void)
{
	spi_unregister_driver(&sciaps_lt_ltc1658_spi_driver);
}
module_exit(sciaps_lt_ltc1658_spi_exit);

MODULE_DESCRIPTION("SciAps LT LTC1658 SPI Driver");
MODULE_AUTHOR("Andre Doudkin");
MODULE_LICENSE("GPL v2");

