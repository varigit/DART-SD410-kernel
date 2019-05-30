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
#include <linux/i2c.h>

#include "sciaps-driver.h"

#define SCIAPS_CNTLR_CONTROL_DEFAULT					0x0000

#define SCIAPS_CNTLR_REG_Size		2

#define SCIAPS_CNTLR_REG_Protocol			0x00
#define SCIAPS_CNTLR_REG_FWCodeLW			0x01
#define SCIAPS_CNTLR_REG_FWCodeHW			0x02
#define SCIAPS_CNTLR_REG_VersionLW			0x03
#define SCIAPS_CNTLR_REG_VersionHW			0x04
#define SCIAPS_CNTLR_REG_Status				0x40
#define SCIAPS_CNTLR_REG_Control			0x80 //R/W
#define SCIAPS_CNTLR_REG_TempCalConst		0x85 //R/W/NV
#define SCIAPS_CNTLR_REG_SerialLW			0x87 //R/W/NV
#define SCIAPS_CNTLR_REG_SerialHW			0x88 //R/W/NV
#define SCIAPS_CNTLR_REG_PowerControl		0x80 //R/W

enum sciaps_cntlr_register_index_t {
		SCIAPS_CNTLR_REG_Protocol_Index = 0,
		SCIAPS_CNTLR_REG_FWCodeLW_Index,
		SCIAPS_CNTLR_REG_FWCodeHW_Index,
		SCIAPS_CNTLR_REG_VersionLW_Index,
		SCIAPS_CNTLR_REG_VersionHW_Index,
		SCIAPS_CNTLR_REG_Status_Index,
		SCIAPS_CNTLR_REG_Control_Index,
		SCIAPS_CNTLR_REG_TempCalConst_Index,
		SCIAPS_CNTLR_REG_SerialLW_Index,
		SCIAPS_CNTLR_REG_SerialHW_Index,
		SCIAPS_CNTLR_REG_PowerControl_Index,
		SCIAPS_CNTLR_REG_Register_Index,
		SCIAPS_CNTLR_REG_NumberOfRegisters,
};

static uint8_t sciaps_cntlr_registers[SCIAPS_CNTLR_REG_NumberOfRegisters] = {
		SCIAPS_CNTLR_REG_Protocol,
		SCIAPS_CNTLR_REG_FWCodeLW,
		SCIAPS_CNTLR_REG_FWCodeHW,
		SCIAPS_CNTLR_REG_VersionLW,
		SCIAPS_CNTLR_REG_VersionHW,
		SCIAPS_CNTLR_REG_Status,
		SCIAPS_CNTLR_REG_Control,
		SCIAPS_CNTLR_REG_TempCalConst,
		SCIAPS_CNTLR_REG_SerialLW,
		SCIAPS_CNTLR_REG_SerialHW,
		SCIAPS_CNTLR_REG_PowerControl,
		SCIAPS_CNTLR_REG_Register_Index,
				};


struct sciaps_data_t {
	uint8_t				_device_id;
	struct mutex		_lock;
	struct i2c_client*	_i2c;
	union device_data_t {
		struct sciaps_cntlr_t {
			uint16_t	_values[SCIAPS_CNTLR_REG_NumberOfRegisters];
			uint16_t	_reg;
		} _sciaps_cntlr;
	} _data;
};

static const struct i2c_device_id sciaps_cntlr_i2c_device_id_table[] = {
	{ .name = "sciaps_pic_cntlr",	.driver_data = (kernel_ulong_t)SCIAPS_DRIVER_ID_SCIAPS_CNTLR_I2C,	}, 
	{}
};
MODULE_DEVICE_TABLE(i2c, sciaps_cntlr_i2c_device_id_table);

static const struct of_device_id sciaps_cntlr_i2c_of_match_table[] = {
	{ .compatible = "sciaps,sciaps_pic_cntlr",	.data = &sciaps_cntlr_i2c_device_id_table[0]	},
	{}
};
MODULE_DEVICE_TABLE(of, sciaps_cntrl_i2c_of_match_table);

static struct sciaps_data_t* sciaps_cntlr_i2c_check_and_get_data(struct i2c_client *i2c)
{
	struct sciaps_data_t* data;
	uint8_t device_id = SCIAPS_DRIVER_ID_SCIAPS_CNTLR_I2C;
	if (!i2c) {
		return NULL;
	}
	data = i2c_get_clientdata(i2c);
	if (!data) {
		return NULL;
	}
	if (data->_device_id != device_id) {
		return NULL;
	}
	return data;
}
	
static int sciaps_cntlr_i2c_read_reg(struct i2c_client *i2c, uint8_t reg, uint16_t *value, bool lock_it) 
{
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	int ret;

	if (!data) {
		return -EINVAL;
	}

	ret = 0;
	if (lock_it)
		mutex_lock(&data->_lock);
	{
		uint8_t buf[SCIAPS_CNTLR_REG_Size], addr[1] = {reg};
		
		struct i2c_msg msgs[] = {
			{
				.addr   = i2c->addr,
				.flags  = 0,
				.len    = 1,
				.buf    = addr,
			},
			{
				.addr   = i2c->addr,
				.flags  = I2C_M_RD,
				.len    = SCIAPS_CNTLR_REG_Size,
				.buf    = buf,
			}
		};

		if ((ret = i2c_transfer(i2c->adapter, msgs, 2)) < 0) {
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_read_reg: read error: %d\n", ret);
		}
		else {
			if (value) {
				uint16_t tmp = buf[1];
				tmp <<= 8;
				tmp |=  buf[0];
				*value = tmp;
				
			}		
		}
	}
	if (lock_it)
		mutex_unlock(&data->_lock);	
	
	return ret;
}

static int sciaps_cntlr_i2c_write_reg(struct i2c_client *i2c, uint8_t reg, uint16_t value, bool lock_it) 
{
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	int ret;

	if (!data) {
		return -EINVAL;
	}

	ret = 0;
	if (lock_it)
		mutex_lock(&data->_lock);
	{
		uint8_t buf[SCIAPS_CNTLR_REG_Size + 1] = {reg, (uint8_t)(value), (uint8_t)(value>>8)};
		
		struct i2c_msg msgs[] = {
			{
				.addr   = i2c->addr,
				.flags  = 0,
				.len    = 1 + SCIAPS_CNTLR_REG_Size,
				.buf    = buf,
			},
		};

		if ((ret = i2c_transfer(i2c->adapter, msgs, 1)) < 0) {
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_read_reg: read error: %d\n", ret);
		}

	}
	if (lock_it)
		mutex_unlock(&data->_lock);	
	
	return ret;
}

static ssize_t cntlr_any_store_impl(struct device* child, struct device_attribute* attr, const char* buf, size_t count, uint8_t reg)
{
	struct i2c_client* i2c = to_i2c_client(child);
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	uint32_t value;
	int ret = 0, offset = 0;


	if (!data || reg >= SCIAPS_CNTLR_REG_NumberOfRegisters || count < 2) {
		return -EINVAL;
	}
	
	if ((offset = kstrtou32(buf, 16, &value)) < 0) {
		return -EINVAL;
	}	
	
	mutex_lock(&data->_lock);
	{
		if (value > 0xffff) {
			uint16_t mask = (uint16_t)(value>>16);
			uint16_t curr_value = 0;
			ret = sciaps_cntlr_i2c_read_reg(i2c, sciaps_cntlr_registers[reg], &curr_value, false); 
			if (ret >= 0) {
				curr_value &= ~mask;
				curr_value |= (((uint16_t)value) & mask);
				value = curr_value;
			}
		}
		if (ret >= 0) {
			ret = sciaps_cntlr_i2c_write_reg(i2c, sciaps_cntlr_registers[reg], (uint16_t)value, false); 
			if (ret >= 0)
				data->_data._sciaps_cntlr._values[reg] = (uint16_t)value;
	
		}
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return count;
}

static ssize_t cntlr_any_show_impl(struct device* child, struct device_attribute* attr, char* buf, uint8_t reg)
{
	struct i2c_client* i2c = to_i2c_client(child);
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	uint16_t value;
	int ret = 0;


	if (!data || reg >= SCIAPS_CNTLR_REG_NumberOfRegisters) {
		return -EINVAL;
	}
	
	mutex_lock(&data->_lock);
	{
		ret = sciaps_cntlr_i2c_read_reg(i2c, sciaps_cntlr_registers[reg], &value, false); 
		if (ret >= 0)
			data->_data._sciaps_cntlr._values[reg] = value;
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%.4x\n", value);
}

static ssize_t cntlr_protocol_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_Protocol_Index);
}

static ssize_t cntlr_fw_code_lw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_FWCodeLW_Index);
}

static ssize_t cntlr_fw_code_hw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_FWCodeHW_Index);
}

static ssize_t cntlr_ver_lw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_VersionLW_Index);
}

static ssize_t cntlr_ver_hw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_VersionHW_Index);
}

static ssize_t cntlr_status_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_Status_Index);
}

static ssize_t cntlr_control_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_Control_Index);
}

static ssize_t cntlr_control_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	return cntlr_any_store_impl(child, attr, buf, count, SCIAPS_CNTLR_REG_Control_Index);
}

static ssize_t cntlr_temp_cal_const_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_TempCalConst_Index);
}

static ssize_t cntlr_temp_cal_const_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	return cntlr_any_store_impl(child, attr, buf, count, SCIAPS_CNTLR_REG_TempCalConst_Index);
}

static ssize_t cntlr_serial_lw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_SerialLW_Index);
}

static ssize_t cntlr_serial_lw_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	return cntlr_any_store_impl(child, attr, buf, count, SCIAPS_CNTLR_REG_SerialLW_Index);
}

static ssize_t cntlr_serial_hw_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_SerialHW_Index);
}

static ssize_t cntlr_serial_hw_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	return cntlr_any_store_impl(child, attr, buf, count, SCIAPS_CNTLR_REG_SerialHW_Index);
}

static ssize_t cntlr_pcon_show(struct device* child, struct device_attribute* attr, char* buf)
{
	return cntlr_any_show_impl(child, attr, buf, SCIAPS_CNTLR_REG_PowerControl_Index);
}


static ssize_t cntlr_reg_show(struct device* child, struct device_attribute* attr, char* buf)
{
	struct i2c_client* i2c = to_i2c_client(child);
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	uint16_t value;

	if (!data) {
		return -EINVAL;
	}
	
	mutex_lock(&data->_lock);
	{
		value = data->_data._sciaps_cntlr._reg;
	}
	mutex_unlock(&data->_lock);

	return scnprintf(buf, PAGE_SIZE, "%.4x\n", value);
}

static ssize_t cntlr_reg_store(struct device* child, struct device_attribute* attr, const char* buf, size_t count)
{
	struct i2c_client* i2c = to_i2c_client(child);
	struct sciaps_data_t* data = sciaps_cntlr_i2c_check_and_get_data(i2c);
	uint32_t value;
	uint16_t reg;
	uint16_t reg_value;
	int ret = 0, offset = 0;


	if (!data || count < 2) {
		return -EINVAL;
	}
	
	if ((offset = kstrtou32(buf, 16, &value)) < 0) {
		return -EINVAL;
	}	
	
	mutex_lock(&data->_lock);
	{
		if (value > 0xffff) {
			reg			= (uint8_t)(value>>16); // Ignore the most significant byte
			reg_value	= (uint16_t)(value);
			ret = sciaps_cntlr_i2c_write_reg(i2c, reg, reg_value, false);
			if (ret >= 0) {
				data->_data._sciaps_cntlr._reg = reg_value;
				ret = sciaps_cntlr_i2c_read_reg(i2c, reg, &reg_value, false);
				if (ret >= 0) {
					data->_data._sciaps_cntlr._reg = reg_value;
				}
			}
		}
		else {
			reg = (uint8_t)(value);
			ret = sciaps_cntlr_i2c_read_reg(i2c, reg, &reg_value, false);
			if (ret >= 0) {
				data->_data._sciaps_cntlr._reg = reg_value;
			}
		}
	}
	mutex_unlock(&data->_lock);

	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(sc_proto,			0444,	cntlr_protocol_show,			NULL);  
static DEVICE_ATTR(sc_code_lw,			0444,	cntlr_fw_code_lw_show,			NULL);  
static DEVICE_ATTR(sc_code_hw,			0444,	cntlr_fw_code_hw_show,			NULL);  
static DEVICE_ATTR(sc_version_lw,		0444,	cntlr_ver_lw_show,				NULL);  
static DEVICE_ATTR(sc_version_hw,		0444,	cntlr_ver_hw_show,				NULL);  
static DEVICE_ATTR(sc_status,			0444,	cntlr_status_show,				NULL);  
static DEVICE_ATTR(sc_control,			0666,	cntlr_control_show,				cntlr_control_store);  
static DEVICE_ATTR(sc_temp_cal_const,	0666,	cntlr_temp_cal_const_show,		cntlr_temp_cal_const_store);  
static DEVICE_ATTR(sc_serial_lw,		0666,	cntlr_serial_lw_show,			cntlr_serial_lw_store);  
static DEVICE_ATTR(sc_serial_hw,		0666,	cntlr_serial_hw_show,			cntlr_serial_hw_store);  
static DEVICE_ATTR(sc_pcon,				0444,	cntlr_pcon_show,				NULL);  
static DEVICE_ATTR(sc_reg,				0666,	cntlr_reg_show,					cntlr_reg_store);  

static struct device_attribute* sciaps_cntlr_dev_attrs[SCIAPS_CNTLR_REG_NumberOfRegisters] = {
									&dev_attr_sc_proto,         
									&dev_attr_sc_code_lw,       
									&dev_attr_sc_code_hw,       
									&dev_attr_sc_version_lw,    
									&dev_attr_sc_version_hw,    
									&dev_attr_sc_status,        
									&dev_attr_sc_control,       
									&dev_attr_sc_temp_cal_const,
									&dev_attr_sc_serial_lw,     
									&dev_attr_sc_serial_hw,     
									&dev_attr_sc_pcon,
									&dev_attr_sc_reg,
								};

static void cntlr_remove_files(struct i2c_client *i2c)
{
	int  i = 0;

	for (i = 0; i < SCIAPS_CNTLR_REG_NumberOfRegisters; i++) {
		device_remove_file(&i2c->dev, sciaps_cntlr_dev_attrs[i]);
	}
}

static int cntlr_create_files(struct i2c_client *i2c)
{
	int ret = -EINVAL, i;
	
	for (i = 0; i < SCIAPS_CNTLR_REG_NumberOfRegisters; i++) {
		if ((ret = device_create_file(&i2c->dev, sciaps_cntlr_dev_attrs[i])) != 0 ) {
			break;
		}
	}
	if (i < SCIAPS_CNTLR_REG_NumberOfRegisters) {
		for (; i >= 0; i--) {
			device_remove_file(&i2c->dev, sciaps_cntlr_dev_attrs[i]);
		}
	}
	else {
		ret = 0;
	}


	return ret;
}




static int sciaps_cntlr_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{

	int ret = 0;
	struct sciaps_data_t* data;

	if (!i2c || !id) {
		dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: Invalid params;");
		return -EINVAL;
	}

	dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: i2c->flags = %x;",		i2c->flags);
	dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: i2c->addr = %x;",		i2c->addr);
	dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: i2c->name = %s;",		i2c->name);
	dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: id->name = %s;",		id->name);

	
	{
		uint8_t device_id = 0;
		const struct of_device_id *of_id =  of_match_device(sciaps_cntlr_i2c_of_match_table, &i2c->dev);
		
		if (!of_id) {
			ret = -ENODEV;
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: there is NO match for %s;", i2c->name);
			return ret;
		}
		if (!of_id->data) {
			ret = -ENODEV;
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: there is NO .data for %s;", i2c->name);
			return ret;
		}
		device_id = (uint8_t)((const struct i2c_device_id*)(of_id->data))->driver_data;
		dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: there is a match for i2c->%s of device %d;", i2c->name, device_id);
		if (device_id != SCIAPS_DRIVER_ID_SCIAPS_CNTLR_I2C) {
			ret = -ENODEV;
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: Invalid device ID for %s;", i2c->name);
			return ret;
		}
 
		{

			dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: processing for SCIAPS_DRIVER_ID_SCIAPS_CNTLR_I2C...");

			data = devm_kzalloc(&i2c->dev, sizeof(struct sciaps_data_t), GFP_KERNEL);
			if (!data) {
				ret = -ENOMEM;
				dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: unable to allocate buffer for struct sciaps_cntlr for %s", i2c->name);
				return ret;
			}
				
			data->_device_id					= device_id;
			data->_i2c							= i2c;


			if (cntlr_create_files(i2c) != 0 ) {
				ret = -ENODEV;
				dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: unable to create device files for for sciaps_cntlr");
				return ret;
			}
			mutex_init(&data->_lock);
			data->_data._sciaps_cntlr._reg = 0;
			i2c_set_clientdata(i2c, data);
			dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: clientdata set for %s;", i2c->name);
		       	       
		}
		if (ret < 0) {
			dev_err(&i2c->dev, "sciaps_cntlr_i2c_probe: i2c_setup failed  for %s with errno: %d;", i2c->name, ret);
			return ret;
		} 
		else {
			dev_info(&i2c->dev, "sciaps_cntlr_i2c_probe: i2c_setup success for %s;", i2c->name);
		}
	}
	return ret;
}

static int sciaps_cntlr_i2c_remove(struct i2c_client *i2c)
{
	if (i2c) {
		struct sciaps_data_t* data = i2c_get_clientdata(i2c);

		if (data) {
			mutex_destroy(&data->_lock);
			cntlr_remove_files(i2c);
		}
	}
	dev_err(&i2c->dev, "sciaps_cntlr_i2c_remove: i2c->name = %s;", i2c->name);
	return 0;
}

static struct i2c_driver sciaps_cntlr_i2c_driver = {
	.id_table = sciaps_cntlr_i2c_device_id_table,
	.driver = {
		.name = "sciaps-cntlr-i2c",
		.owner = THIS_MODULE,
		.of_match_table = sciaps_cntlr_i2c_of_match_table,
	},
	.probe = sciaps_cntlr_i2c_probe,
	.remove = sciaps_cntlr_i2c_remove,
};

static int __init sciaps_cntlr_i2c_init(void)
{
	return i2c_add_driver(&sciaps_cntlr_i2c_driver);
}
subsys_initcall(sciaps_cntlr_i2c_init);

static void __exit sciaps_cntlr_i2c_exit(void)
{
	i2c_del_driver(&sciaps_cntlr_i2c_driver);
}
module_exit(sciaps_cntlr_i2c_exit);

MODULE_DESCRIPTION("SciAps Controller I2C Driver");
MODULE_AUTHOR("Andre Doudkin");
MODULE_LICENSE("GPL v2");

