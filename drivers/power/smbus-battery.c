/*
 * Gas Gauge driver for SBS Compliant Batteries
 *
 * Copyright (c) 2016, Sciaps
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#if !defined(__devinit)
#define __devinit
#endif

#if !defined(__devexit)
#define __devexit
#endif

#if !defined(__devexit_p)
#define __devexit_p(x) (&(x))
#endif

#define SMBUS_BATTERY_REG_TEMP			0x08 /* Temperature */
#define SMBUS_BATTERY_REG_STATUS		0x16 /* Battery Status */
#define SMBUS_BATTERY_REG_RSOC			0x0D /* Relative State-of-Charge */

/* bits in SMBUS_BATTERY_REG_STATUS */
#define SMBUS_BATTERY_STATUS_OVER_CHARGED	0x8000
#define SMBUS_BATTERY_STATUS_TERMINATE_CHARGE	0x4000
#define SMBUS_BATTERY_STATUS_OVERTEMP		0x1000
#define SMBUS_BATTERY_STATUS_TERMINATE_DISCHARGE	0x0800
#define SMBUS_BATTERY_STATUS_INIT		0x80
#define SMBUS_BATTERY_STATUS_DISCHARGING	0x40
#define SMBUS_BATTERY_STATUS_FULL_CHRG		0x20
#define SMBUS_BATTERY_STATUS_FULL_DISCHRG	0x10

static enum power_supply_property smbus_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	//POWER_SUPPLY_PROP_CYCLE_COUNT,
	//POWER_SUPPLY_PROP_VOLTAGE_NOW,
	//POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	//POWER_SUPPLY_PROP_TEMP,
	//POWER_SUPPLY_PROP_SERIAL_NUMBER,
	//POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	//POWER_SUPPLY_PROP_CHARGE_NOW,
	//POWER_SUPPLY_PROP_CHARGE_FULL,
	//POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

struct smbus_battery_reg_cache {
	bool present;
	s16 status;
	s16 temperature;
	s16 time_to_empty;
	s16 time_to_full;
	s16 charge_full;
	s16 charge_design_full;
	s16 cycle_count;
	s16 capacity;
};

struct smbus_info {
	struct i2c_client	*client;
	struct power_supply	power_supply;
	struct smbus_battery_reg_cache reg_cache;
	struct delayed_work	work;
	struct mutex lock;
};

static int i2c_read_errs = 0;

static int smbus_read_word_data(struct i2c_client *client, u8 address, s16* out)
{
	s32 ret = i2c_smbus_read_word_data(client, address);

	if (ret < 0) {
		dev_err(&client->dev,
			"%s: i2c read at address 0x%x failed: 0x%x\n",
			__func__, address, ret);
	} else {
		*out = (s16)(0xFFFF & ret);
	}

	return ret;
}

/*
static int smbus_write_word_data(struct i2c_client *client, u8 address,
	u16 value)
{
	s32 ret = 0;
	int retries = 2;

	while (retries > 0) {
		ret = i2c_smbus_write_word_data(client, address,
			le16_to_cpu(value));
		if (ret >= 0)
			break;
		retries--;
	}

	if (ret < 0) {
		dev_dbg(&client->dev,
			"%s: i2c write to address 0x%x failed\n",
			__func__, address);
		return ret;
	}

	return 0;
}
*/

static int smbus_battery_status(struct smbus_info *chip, union power_supply_propval *val)
{

	if (!chip->reg_cache.present) {
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		return 0;
	}

	if (chip->reg_cache.status & SMBUS_BATTERY_STATUS_FULL_CHRG)
		val->intval = POWER_SUPPLY_STATUS_FULL;
	else if (chip->reg_cache.status & SMBUS_BATTERY_STATUS_DISCHARGING)
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	else
		val->intval = POWER_SUPPLY_STATUS_CHARGING;

	return 0;
}

static int smbus_battery_health(struct smbus_info *chip, union power_supply_propval *val)
{
	if(!chip->reg_cache.present) {
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		return 0;
	}

	if (chip->reg_cache.status & SMBUS_BATTERY_STATUS_OVERTEMP) {
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if(chip->reg_cache.status & SMBUS_BATTERY_STATUS_OVER_CHARGED) {
		val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else {
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	}

	return 0;
}

static int smbus_battery_get_capacity(struct smbus_info *chip,
	union power_supply_propval *val)
{
	if (!chip->reg_cache.present) {
		val->intval = 0;
		return 0;
	}


	if (chip->reg_cache.status & SMBUS_BATTERY_STATUS_FULL_CHRG)
		val->intval = 100;
	else
		val->intval = chip->reg_cache.capacity;

	if(val->intval < 1) {
		val->intval = 1;
	}

	if(val->intval > 100) {
		val->intval = 100;
	}

	return 0;
}

static int smbus_battery_get_temp(struct smbus_info *chip,
	union power_supply_propval *val)
{
	if (!chip->reg_cache.present) {
		return -ENODATA;
	}


	val->intval = chip->reg_cache.temperature;
	val->intval -= 2731;	// convert from .1 degrees K

	return 0;
}

static int smbus_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	struct smbus_info *chip = container_of(psy,
				struct smbus_info, power_supply);

  struct i2c_client *client = chip->client;

	mutex_lock(&chip->lock);

	switch(psp) {

		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = chip->reg_cache.present ? 1 : 0;
			break;

		case POWER_SUPPLY_PROP_STATUS:
			ret = smbus_battery_status(chip, val);
			break;

		case POWER_SUPPLY_PROP_HEALTH:
			ret = smbus_battery_health(chip, val);
			break;

		case POWER_SUPPLY_PROP_CAPACITY:
			ret = smbus_battery_get_capacity(chip, val);
			break;

		case POWER_SUPPLY_PROP_TEMP:
			ret = smbus_battery_get_temp(chip, val);
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;

		default:
			dev_err(&client->dev, "unknown psp: %d\n", psp);
			ret = -EINVAL;
	}

	mutex_unlock(&chip->lock);

	if(ret >= 0) {
		dev_dbg(&client->dev,
			"%s: property = %d, value = %x\n", __func__, psp, val->intval);
	}

	return ret;
}


static void smbus_external_power_changed(struct power_supply *psy)
{
	struct smbus_info *chip = container_of(psy, struct smbus_info, power_supply);
  struct i2c_client *client = chip->client;

	dev_err(&client->dev, "external_power_changed");
	i2c_read_errs = 0;
}

static void smbus_delayed_work(struct work_struct *work)
{
	#define CHECK_READ_WORD(reg, location, min, max) \
	ret = smbus_read_word_data(chip->client, reg, &ival); \
	if(ret >= 0) { \
		dev_dbg(&chip->client->dev, "read " #reg " = 0x%x\n", ival); \
		if(ival >= min && ival < max) { \
			location = ival; \
		}	\
	}

	// Roughly 60 seconds at 2s poll freq
	#define READ_ERR_LIMIT 30

	struct smbus_info *chip;
	bool changed = false;
	struct smbus_battery_reg_cache cache;
	s32 ret;
	s16 ival;

	chip = container_of(work, struct smbus_info, work.work);

	mutex_lock(&chip->lock);

	cache = chip->reg_cache;

	mutex_unlock(&chip->lock);

	CHECK_READ_WORD(SMBUS_BATTERY_REG_STATUS, cache.status, 0, 65535)
	cache.present = ret >= 0;
	if (cache.present) {
		CHECK_READ_WORD(SMBUS_BATTERY_REG_RSOC, cache.capacity, 0, 100)
		CHECK_READ_WORD(SMBUS_BATTERY_REG_TEMP, cache.temperature, 0, 65535)
		i2c_read_errs = 0;
	}
	else {
		if (i2c_read_errs <= READ_ERR_LIMIT) {
			if (i2c_read_errs == READ_ERR_LIMIT)
				dev_err(&chip->client->dev,
					"%s: Too many i2c errors.  Entering slow poll mode\n", __func__);
			i2c_read_errs++;
		}
	}

	mutex_lock(&chip->lock);

	changed = memcmp(&chip->reg_cache, &cache, sizeof(struct smbus_battery_reg_cache));
	if (changed) {
		chip->reg_cache = cache;
	}

	mutex_unlock(&chip->lock);

	if (changed) {
		power_supply_changed(&chip->power_supply);
	}

	if (i2c_read_errs > READ_ERR_LIMIT)
		schedule_delayed_work(&chip->work, 10*60*HZ); else
		schedule_delayed_work(&chip->work, 2*HZ);
}

static int __devinit smbus_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct smbus_info *chip;
	int rc;
	char *name;

	name = kasprintf(GFP_KERNEL, "smbus-%s", dev_name(&client->dev));
	if (!name) {
		dev_err(&client->dev, "Failed to allocate device name\n");
		return -ENOMEM;
	}

	chip = kzalloc(sizeof(struct smbus_info), GFP_KERNEL);
	if (!chip) {
		rc = -ENOMEM;
		goto exit_free_name;
	}

	chip->client = client;
	chip->power_supply.name = name;
	chip->power_supply.type = POWER_SUPPLY_TYPE_BATTERY;
	chip->power_supply.properties = smbus_properties;
	chip->power_supply.num_properties = ARRAY_SIZE(smbus_properties);
	chip->power_supply.get_property = smbus_get_property;
	chip->power_supply.external_power_changed = smbus_external_power_changed;

	chip->reg_cache.present = false;
	chip->reg_cache.capacity = 100;

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, chip);

	INIT_DELAYED_WORK(&chip->work, smbus_delayed_work);

	rc = power_supply_register(&client->dev, &chip->power_supply);
	if (rc) {
		dev_err(&client->dev,
			"%s: Failed to register power supply\n", __func__);
		goto exit_psupply;
	}

	dev_info(&client->dev,
		"%s: battery gas gauge device registered\n", client->name);

	schedule_delayed_work(&chip->work, HZ);

	return 0;

exit_psupply:
	kfree(chip);

exit_free_name:
	kfree(name);

	return rc;
}

static int __devexit smbus_remove(struct i2c_client *client)
{
	struct smbus_info *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->work);
	power_supply_unregister(&chip->power_supply);
	mutex_destroy(&chip->lock);
	kfree(chip->power_supply.name);
	kfree(chip);
	chip = NULL;

	return 0;
}

static const struct i2c_device_id smbus_id[] = {
	{ "smbus-battery", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, smbus_id);

static struct i2c_driver smbus_battery_driver = {
	.probe		= smbus_probe,
	.remove		= __devexit_p(smbus_remove),
	.id_table	= smbus_id,
	.driver = {
		.name	= "smbus-battery",
	},
};
module_i2c_driver(smbus_battery_driver);

MODULE_AUTHOR("Paul Soucy <paul@dev-smart.com>");
MODULE_DESCRIPTION("SMBus battery monitor driver");
MODULE_LICENSE("GPL");
