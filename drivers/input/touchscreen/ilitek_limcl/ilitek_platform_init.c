/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Jijie Wang <jijie_wang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#include "ilitek_ts.h"
struct ilitek_ts_data *ilitek_data;
/* Debug level */
uint32_t print_debug_level = PRINT_NONE;

/* probe function is used for matching and initializing input device */
static int /*__devinit*/ ilitek_touch_driver_probe(struct i2c_client *client,
        const struct i2c_device_id
        *id)
{
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
    int ret = 0;
    const char *vdd_name = "vdd";
    const char *vcc_i2c_name = "vcc_i2c";
#endif	 
    tp_log_info("*** %s ***\n", __func__);
    
    if (client == NULL) {
        tp_log_err("i2c client is NULL\n");
        return -1;
    }
    
    ilitek_data = kzalloc(sizeof(struct ilitek_ts_data), GFP_KERNEL);
    
    if (ilitek_data == NULL) {
        tp_log_err("Alloc GFP_KERNEL memory failed.");
        return -ENOMEM;
    }
    
    ilitek_data->client = client;
#ifdef ILITEK_ENABLE_REGULATOR_POWER_ON
    ilitek_data->vdd = regulator_get(&client->dev, vdd_name);
    
    if (IS_ERR(ilitek_data->vdd)) {
        tp_log_err("regulator_get vdd fail\n");
        ilitek_data->vdd = NULL;
    } else {
        ret =
            regulator_set_voltage(ilitek_data->vdd, VDD_VOLTAGE_MIN,
                                  VDD_VOLTAGE_MAX);
                                  
        if (ret)
            tp_log_err("Could not set to 2800mv.\n");
    }
    
    ilitek_data->vdd_i2c = regulator_get(&client->dev, vcc_i2c_name);
    
    if (IS_ERR(ilitek_data->vdd_i2c)) {
        tp_log_err("regulator_get vdd_i2c fail\n");
        ilitek_data->vdd_i2c = NULL;
    } else {
        ret =
            regulator_set_voltage(ilitek_data->vdd_i2c, I2C_VOLTAGE_MIN,
                                  I2C_VOLTAGE_MAX);
                                  
        if (ret)
            tp_log_err("Could not set to 1800mv.\n");
    }
    
#endif	 
    return ilitek_main_probe(ilitek_data);
}


/* remove function is triggered when the input device is removed from input sub-system */
static int ilitek_touch_driver_remove(struct i2c_client *client)
{
    tp_log_info("*** %s ***\n", __func__);
    return ilitek_main_remove(ilitek_data);
}


#ifdef CONFIG_OF
static struct of_device_id ilitek_touch_match_table[] = {
    {.compatible = "tchip,ilitek",}, {},
};


#endif	 
static const struct i2c_device_id ilitek_touch_device_id[] = {
    {ILITEK_TS_NAME, 0}, {}, /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, ilitek_touch_device_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id ilitekts_acpi_id[] = { {"ILTK0001", 0}, {}
};

MODULE_DEVICE_TABLE(acpi, ilitekts_acpi_id);

#endif	 
static struct i2c_driver ilitek_touch_device_driver = { .driver = {.name =
                ILITEK_TS_NAME,
            .owner = THIS_MODULE,
                
#ifdef CONFIG_OF
            .of_match_table= ilitek_touch_match_table,
                
#endif	 
#ifdef CONFIG_ACPI
            . acpi_match_table= ACPI_PTR(ilitekts_acpi_id),
                
#endif	 
                                                                      },
            .probe = ilitek_touch_driver_probe, 
            .remove = ilitek_touch_driver_remove, 
            .id_table = ilitek_touch_device_id,
};

static int __init ilitek_touch_driver_init(void)
{
    int ret;
    /* register driver */
    ret = i2c_add_driver(&ilitek_touch_device_driver);
    
    if (ret != 0) {
        tp_log_err
        ("add touch device driver i2c driver failed.so remove\n");
        i2c_del_driver(&ilitek_touch_device_driver);
        return -ENODEV;
    }
    
    tp_log_info("add touch device driver i2c driver.\n");
    return ret;
}

static void __exit ilitek_touch_driver_exit(void)
{
    tp_log_info("remove touch device driver i2c driver.\n");
    i2c_del_driver(&ilitek_touch_device_driver);
}
module_init(ilitek_touch_driver_init);

module_exit(ilitek_touch_driver_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");

