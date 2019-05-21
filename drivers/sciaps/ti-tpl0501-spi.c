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

#define SCIAPS_TI_TPL0501_WIPER_CODE_Mask					0xff
#define SCIAPS_TI_TPL0501_WIPER_CODE_Shift					0

#define SCIAPS_TI_TPL0501_WIPER_CODE_Default				0x80

#define SCIAPS_TI_TPL0501_WIPER_CODE_MAX					SCIAPS_TI_TPL0501_WIPER_CODE_Mask						

#define SCIAPS_TI_TPL0501_Rtot_Ohm							100000

struct sciaps_data_t {
	uint8_t				_device_id;
	struct mutex		_lock;
	struct spi_device*	_spi;
	union device_data_t {
		struct ti_tpl0501_t {
			uint8_t			_wiper_code;
		} _ti_tpl0501;
	} _data;
};

static const struct spi_device_id sciaps_ti_tpl0501_spi_device_id_table[] = {
	{ .name = "tpl0501",	.driver_data = (kernel_ulong_t)SCIAPS_DRIVER_ID_TI_TPL0501_SPI,	}, 
	{}
};
MODULE_DEVICE_TABLE(spi, sciaps_ti_tpl0501_spi_device_id_table);

static const struct of_device_id sciaps_ti_tpl0501_spi_of_match_table[] = {
	{ .compatible = "ti,tpl0501",	.data = &sciaps_ti_tpl0501_spi_device_id_table[0]	},
	{}
};
MODULE_DEVICE_TABLE(of, sciaps_ti_tpl0501_spi_of_match_table);


#define SCIAPS_TI_TPL0501_MAX_DATA_LENGTH	4
#define SCIAPS_TI_TPL0501_SPI_SPEED_HZ		SCIAPS_SPI_SPEED_HZ_Default
static char tpl0501_tx_buffer[SCIAPS_TI_TPL0501_MAX_DATA_LENGTH];

static struct sciaps_data_t* tpl0501_spi_check_and_get_data(struct spi_device *spi)
{
	struct sciaps_data_t* data;
	uint8_t device_id = SCIAPS_DRIVER_ID_TI_TPL0501_SPI;
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
	
static int tpl0501_spi_cmd_wiper_code_set(struct spi_device *spi, bool lock_it) 
{
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	int offset = 0;
	uint8_t value = 0;
	int ret;

	if (!data) {
		return -EINVAL;
	}

	if (lock_it)
		mutex_lock(&data->_lock);
	{	
		value = data->_data._ti_tpl0501._wiper_code;
		dev_info(&spi->dev, "tpl0501_spi_cmd_wiper_code_set: value = 0x%x", value);
		
		{
			struct spi_message		msg;
			struct spi_transfer		xfer;
			offset = 0;
			memset(&msg,0x00,sizeof (msg));
			memset(&xfer,0x00,sizeof (xfer));
			tpl0501_tx_buffer[offset++] = (uint8_t)(value);
			xfer.tx_buf = tpl0501_tx_buffer;
			xfer.rx_buf = NULL;
			xfer.speed_hz = SCIAPS_TI_TPL0501_SPI_SPEED_HZ;
			xfer.len	= offset;

			spi_message_init(&msg);
			spi_message_add_tail(&xfer, &msg);
			ret = spi_sync(spi, &msg);	
			
			if (ret < 0) {
				dev_info(&spi->dev, "tpl0501_spi_cmd_wiper_code_set: spi_sync failed with errno = %d", ret);
			}

		}
	}
	if (lock_it)
		mutex_unlock(&data->_lock);	
	
	return ret;
}

static ssize_t tpl0501_wiper_code_max_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint8_t value = SCIAPS_TI_TPL0501_WIPER_CODE_MAX;

	if (!data) {
		return -EINVAL;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t tpl0501_wiper_code_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint8_t value = 0;

	if (!data) {
		return -EINVAL;
	}
	
	mutex_lock(&data->_lock);
	{
		value = data->_data._ti_tpl0501._wiper_code;
	}
	mutex_unlock(&data->_lock);

	return scnprintf(buf, PAGE_SIZE, "%x\n", value);
}

static ssize_t tpl0501_wiper_code_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint8_t value = 0;
	int offset = 0;

	if (!data) {
		return -EINVAL;
	}
	
	if ((offset = kstrtou8(buf, 16, &value)) < 0) {
		return -EINVAL;
	}	
	

	mutex_lock(&data->_lock);
	{
		dev_info(child, "tpl0501_wiper_code_store: value = 0x%x", value);
		
		data->_data._ti_tpl0501._wiper_code = value; 

		tpl0501_spi_cmd_wiper_code_set(spi, false);
	}
	mutex_unlock(&data->_lock);

	return count;

}

static ssize_t tpl0501_rtot_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint32_t value = SCIAPS_TI_TPL0501_Rtot_Ohm;

	if (!data) {
		return -EINVAL;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}
#define SCIAPS_TI_TPL0501_PRES  1000
static ssize_t tpl0501_rhw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct		spi_device* spi = to_spi_device(child);
	struct		sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint64_t	value = 0;
	uint8_t		wc = 0;

	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		wc = data->_data._ti_tpl0501._wiper_code;
	}
	mutex_unlock(&data->_lock);

	
	value = wc;
	value *= SCIAPS_TI_TPL0501_PRES;
	value /= (((uint16_t)SCIAPS_TI_TPL0501_WIPER_CODE_MAX) + 1);
	value = (1 * SCIAPS_TI_TPL0501_PRES - value);
	value *= SCIAPS_TI_TPL0501_Rtot_Ohm;
	
	value += (SCIAPS_TI_TPL0501_PRES / 2); 
	value /= SCIAPS_TI_TPL0501_PRES;

	return scnprintf(buf, PAGE_SIZE, "%d\n", (uint32_t)value);
}

static ssize_t tpl0501_rwl_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct spi_device* spi = to_spi_device(child);
	struct sciaps_data_t* data = tpl0501_spi_check_and_get_data(spi);
	uint64_t value = 0;
	uint8_t wc = 0;

	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->_lock);
	{
		wc = data->_data._ti_tpl0501._wiper_code;
	}
	mutex_unlock(&data->_lock);

	
	value = wc;
	value *= SCIAPS_TI_TPL0501_PRES;
	value /= (((uint16_t)SCIAPS_TI_TPL0501_WIPER_CODE_MAX) + 1);
	value *= SCIAPS_TI_TPL0501_Rtot_Ohm;
	
	value += (SCIAPS_TI_TPL0501_PRES / 2); 
	value /= SCIAPS_TI_TPL0501_PRES;
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", (uint32_t)value);
}

static DEVICE_ATTR(dp_wc,		0666,	tpl0501_wiper_code_show,		tpl0501_wiper_code_store);  
static DEVICE_ATTR(dp_wc_max,	0444,	tpl0501_wiper_code_max_show,	NULL);  
static DEVICE_ATTR(dp_rtot,		0444,	tpl0501_rtot_show,				NULL);  
static DEVICE_ATTR(dp_rhw,		0444,	tpl0501_rhw_show,				NULL);  
static DEVICE_ATTR(dp_rwl,		0444,	tpl0501_rwl_show,				NULL);  

static int tpl0501_create_files(struct spi_device *spi)
{
	int ret = -EINVAL;
	if ((ret = device_create_file(&spi->dev, &dev_attr_dp_wc)) == 0 ) {
		if ((ret = device_create_file(&spi->dev, &dev_attr_dp_wc_max)) == 0 ) {
			if ((ret = device_create_file(&spi->dev, &dev_attr_dp_rtot)) == 0 ) {
				if ((ret = device_create_file(&spi->dev, &dev_attr_dp_rhw)) == 0 ) {
					if ((ret = device_create_file(&spi->dev, &dev_attr_dp_rwl)) == 0 ) {
						ret = 0;
					}
					else {
						device_remove_file(&spi->dev, &dev_attr_dp_wc);
						device_remove_file(&spi->dev, &dev_attr_dp_wc_max);
						device_remove_file(&spi->dev, &dev_attr_dp_rtot);
						device_remove_file(&spi->dev, &dev_attr_dp_rhw);
					}
				}
				else {
					device_remove_file(&spi->dev, &dev_attr_dp_wc);
					device_remove_file(&spi->dev, &dev_attr_dp_wc_max);
					device_remove_file(&spi->dev, &dev_attr_dp_rtot);
				}
			}
			else {
				device_remove_file(&spi->dev, &dev_attr_dp_wc);
				device_remove_file(&spi->dev, &dev_attr_dp_wc_max);
			}
		}
		else {
			device_remove_file(&spi->dev, &dev_attr_dp_wc);
		}
	}
	return ret;
}

static void tpl0501_remove_files(struct spi_device *spi)
{
		device_remove_file(&spi->dev, &dev_attr_dp_wc);
		device_remove_file(&spi->dev, &dev_attr_dp_wc_max);
		device_remove_file(&spi->dev, &dev_attr_dp_rtot);
		device_remove_file(&spi->dev, &dev_attr_dp_rhw);
		device_remove_file(&spi->dev, &dev_attr_dp_rwl);
}

static int sciaps_ti_tpl0501_spi_probe(struct spi_device *spi)
{

	int ret = 0;
	struct sciaps_data_t* data;

	if (!spi) {
		dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: Invalid params;");
		return -EINVAL;
	}

	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->max_speed_hz = %d;",		spi->max_speed_hz);
	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->chip_select = %d;",		spi->chip_select);
	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->mode = %d;",				spi->mode);
	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->bits_per_word = %d;",	spi->bits_per_word);
	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->modalias = %s;",			spi->modalias);
	dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->cs_gpio = %d;",			spi->cs_gpio);

	
	{
		uint8_t device_id = 0;
		const struct of_device_id *of_id =  of_match_device(sciaps_ti_tpl0501_spi_of_match_table, &spi->dev);
		
		if (!of_id) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: there is NO match for %s;", spi->modalias);
			return ret;
		}
		if (!of_id->data) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: there is NO .data for %s;", spi->modalias);
			return ret;
		}
		device_id = (uint8_t)((const struct spi_device_id*)(of_id->data))->driver_data;
		dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: there is a match for spi->%s of device %d;", spi->modalias, device_id);
		if (device_id != SCIAPS_DRIVER_ID_TI_TPL0501_SPI) {
			ret = -ENODEV;
			dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: Invalid device ID for %s;", spi->modalias);
			return ret;
		}
 
		{
			uint8_t spi_cpol = SPI_CPOL;	/*SPI_CPOL: clock polarity: 0 - a clock idles at 0; 1 - a clock idles at 1; */
			uint8_t spi_cpha = SPI_CPHA;	/*SPI_CPHA: clock phase: 1 - 'out' changes on the leading edge; 0 - 'out' changes on the trailing edge*/


			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: processing for SCIAPS_DRIVER_ID_TI_TPL0501_SPI...");
			spi->bits_per_word = 8;
			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->bits_per_word = %d now;", spi->bits_per_word);
			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: Clock idles at %d; Leading edge change: %d", spi_cpha, spi_cpol);

			spi->mode = (spi_cpha|spi_cpol);
			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi->mode = %d now;", spi->mode);
			
			data = devm_kzalloc(&spi->dev, sizeof(struct sciaps_data_t), GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: unable to allocate buffer for struct sciaps_ti_tpl0501 for %s", spi->modalias);
				return ret;
			}
				
			data->_device_id						= device_id;
			data->_spi								= spi;
			data->_data._ti_tpl0501._wiper_code		= SCIAPS_TI_TPL0501_WIPER_CODE_Default;


			if (tpl0501_create_files(spi) != 0 ) {
				ret = -ENODEV;
				dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: unable to create device files for for sciaps_ti_tpl0501");
				return ret;
			}
			mutex_init(&data->_lock);
			spi_set_drvdata(spi, data);
			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: drvdata set for %s;", spi->modalias);
		       	       
		}
		ret = spi_setup(spi);
		if (ret < 0) {
			dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi_setup failed  for %s with errno: %d;", spi->modalias, ret);
			return ret;
		} 
		else {
			dev_info(&spi->dev, "sciaps_ti_tpl0501_spi_probe: spi_setup success for %s;", spi->modalias);
		}
	}
	return 0;
}

static int sciaps_ti_tpl0501_spi_remove(struct spi_device *spi)
{
	if (spi) {
		struct sciaps_data_t* data = spi_get_drvdata(spi);

		if (data) {
			mutex_destroy(&data->_lock);
			tpl0501_remove_files(spi);
		}
	}
	dev_err(&spi->dev, "sciaps_ti_tpl0501_spi_remove: spi->modalias = %s;", spi->modalias);
	return 0;
}

static struct spi_driver sciaps_ti_tpl0501_spi_driver = {
	.id_table = sciaps_ti_tpl0501_spi_device_id_table,
	.driver = {
		.name = "sciaps-ti-tpl0501-spi",
		.owner = THIS_MODULE,
		.of_match_table = sciaps_ti_tpl0501_spi_of_match_table,
	},
	.probe = sciaps_ti_tpl0501_spi_probe,
	.remove = sciaps_ti_tpl0501_spi_remove,
};

static int __init sciaps_ti_tpl0501_spi_init(void)
{
	return spi_register_driver(&sciaps_ti_tpl0501_spi_driver);
}
subsys_initcall(sciaps_ti_tpl0501_spi_init);

static void __exit sciaps_ti_tpl0501_spi_exit(void)
{
	spi_unregister_driver(&sciaps_ti_tpl0501_spi_driver);
}
module_exit(sciaps_ti_tpl0501_spi_exit);

MODULE_DESCRIPTION("SciAps TI TPL0501  SPI Driver");
MODULE_AUTHOR("Andre Doudkin");
MODULE_LICENSE("GPL v2");

