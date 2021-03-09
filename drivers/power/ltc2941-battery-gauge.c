/*
 * I2C client/driver for the Linear Technology LTC2941 and LTC2943
 * Battery Gas Gauge IC
 *
 * Copyright (C) 2014 Topic Embedded Systems
 *
 * Author: Auryn Verwegen
 * Author: Mike Looijmans
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/swab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/reboot.h>

#include <linux/qpnp/power-on.h>

//#define DEV_DBG	dev_info
//#define DEV_DBG	dev_dbg
#define DEV_DBG(...)	((void)0)

#define I16_MSB(x)			((x >> 8) & 0xFF)
#define I16_LSB(x)			(x & 0xFF)



#define LTC294X_WORK_DELAY		10	/* Update delay in seconds */

#define LTC294X_MAX_VALUE		0xFFFF
#define LTC294X_MID_SUPPLY		0x7FFF
#define LTC294X_THRESH_HIGH		0xFF80

#define LTC2941_MAX_PRESCALER_EXP	7
#define LTC2943_MAX_PRESCALER_EXP	6
#define LTC2941_MIN_VOLTAGE_LOW_THRES_uV	11000000
#define LTC2941_VOLTAGE_CRIT_LOW_THRES_uV	12000000
#define LTC2941_VOLTAGE_CHARGE_LOW_THRES_uV	13500000
#define LTC2941_MIN_CHARGE_LOW_THRES 0

#define LTC294x_DRIVER_VERSION "1.01.001"

enum ltc294x_reg {
	LTC294X_REG_STATUS		= 0x00,
	LTC294X_REG_CONTROL		= 0x01,
	LTC294X_REG_ACC_CHARGE_MSB	= 0x02,
	LTC294X_REG_ACC_CHARGE_LSB	= 0x03,
	LTC294X_REG_THRESH_HIGH_MSB	= 0x04,
	LTC294X_REG_THRESH_HIGH_LSB	= 0x05,
	LTC294X_REG_THRESH_LOW_MSB	= 0x06,
	LTC294X_REG_THRESH_LOW_LSB	= 0x07,
	LTC294X_REG_VOLTAGE_MSB	= 0x08,
	LTC294X_REG_VOLTAGE_LSB	= 0x09,
	LTC294X_REG_CURRENT_MSB	= 0x0E,
	LTC294X_REG_CURRENT_LSB	= 0x0F,
	LTC294X_REG_TEMPERATURE_MSB	= 0x14,
	LTC294X_REG_TEMPERATURE_LSB	= 0x15,
};

#define LTC2943_REG_CONTROL_MODE_MASK (BIT(7) | BIT(6))
#define LTC2943_REG_CONTROL_MODE_SCAN BIT(7)
#define LTC294X_REG_CONTROL_PRESCALER_MASK	(BIT(5) | BIT(4) | BIT(3))
#define LTC294X_REG_CONTROL_SHUTDOWN_MASK	(BIT(0))
#define LTC294X_REG_CONTROL_PRESCALER_SET(x) \
	((x << 3) & LTC294X_REG_CONTROL_PRESCALER_MASK)
#define LTC294X_REG_CONTROL_ALCC_CONFIG_DISABLED	0
#define LTC294X_REG_CONTROL_ALCC_CONFIG_ChargeCompleteMode	(BIT(1))

#define LTC2941_NUM_REGS	0x08
#define LTC2943_NUM_REGS	0x18

#define LTC294x_BATT_CAPACITY_DEFAULT	1
#define LTC294x_BATT_CAPACITY_MIN		1

struct ltc294x_info {
	struct i2c_client *client;	/* I2C Client pointer */
	struct device     *dev;
	struct power_supply *supply;	/* Supply pointer */
#if 0
	struct power_supply_desc supply_desc;	/* Supply description */
#else
	struct power_supply batt_psy;
#endif
	struct power_supply dc_psy;
	struct power_supply* usb_psy;
	struct delayed_work work;	/* Work scheduler */
	int num_regs;	/* Number of registers (chip type) */
	int charge;	/* Last charge register content */
	int r_sense;	/* mOhm */
	int Qlsb;	/* nAh */
	u32 prescaler_exp;

	int gpio_charge_in_progress;
	int gpio_charge_completed;

	//status tracking
	uint8_t			batt_status;
	uint32_t		batt_capacity;
	uint8_t			dc_present;
	uint8_t			chg_enabled;
	uint16_t		charge_low_thres;
	uint32_t		voltage_charge_low_thres_uV;
	uint32_t		voltage_crit_low_thres_uV;
};

static inline int convert_bin_to_uAh(
	const struct ltc294x_info *info, int Q)
{
	return ((Q * (info->Qlsb / 10))) / 100;
}

static inline int convert_uAh_to_bin(
	const struct ltc294x_info *info, int uAh)
{
	int Q;

	Q = (uAh * 100) / (info->Qlsb/10);
	return (Q < LTC294X_MAX_VALUE) ? Q : LTC294X_MAX_VALUE;
}

static int ltc294x_read_regs(struct i2c_client *client,
	enum ltc294x_reg reg, u8 *buf, int num_regs)
{
	int ret;
	struct i2c_msg msgs[2] = { };
	u8 reg_start = reg;

	msgs[0].addr	= client->addr;
	msgs[0].len	= 1;
	msgs[0].buf	= &reg_start;

	msgs[1].addr	= client->addr;
	msgs[1].len	= num_regs;
	msgs[1].buf	= buf;
	msgs[1].flags	= I2C_M_RD;

	ret = i2c_transfer(client->adapter, &msgs[0], 2);
	if (ret < 0) {
		dev_err(&client->dev, "ltc2941 read_reg failed!\n");
		return ret;
	}

	//dev_err(&client->dev, "---%s (%#x, 1/%d) -> %#x\n",
	//	__func__, reg, num_regs, *buf);
//
//	if (num_regs > 1) {
//		dev_err(&client->dev, "---%s (%#x, 2/%d) -> %#x\n",
//			__func__, reg, num_regs, buf[1]);
//	}

	return 0;
}

static int ltc294x_write_regs(struct i2c_client *client,
	enum ltc294x_reg reg, const u8 *buf, int num_regs)
{
	int ret;
	u8 reg_start = reg;

	ret = i2c_smbus_write_i2c_block_data(client, reg_start, num_regs, buf);
	if (ret < 0) {
		dev_err(&client->dev, "ltc2941 write_reg failed!\n");
		return ret;
	}

	DEV_DBG(&client->dev, "  >>>>> %s (%#x, %d) -> %#x\n",
		__func__, reg, num_regs, *buf);

	return 0;
}

static int ltc294x_reset(const struct ltc294x_info *info, int prescaler_exp)
{
	int ret;
	u8 value;
	u8 control;

	/* Read status and control registers */
	//ret = ltc294x_read_regs(info->client, LTC294X_REG_STATUS, &value, 1);
	//if (ret < 0) {
	//	dev_err(&info->client->dev,
	//		"Could not read registers from device\n");
	//	goto error_exit;
	//}
	ret = ltc294x_read_regs(info->client, LTC294X_REG_CONTROL, &value, 1);
	if (ret < 0) {
		dev_err(&info->client->dev,
			"Could not read registers from device\n");
		goto error_exit;
	}

	control = LTC294X_REG_CONTROL_PRESCALER_SET(prescaler_exp) |
						LTC294X_REG_CONTROL_ALCC_CONFIG_ChargeCompleteMode;
	/* Put the 2943 into "monitor" mode, so it measures every 10 sec */
	if (info->num_regs == LTC2943_NUM_REGS)
		control |= LTC2943_REG_CONTROL_MODE_SCAN;

	if (value != control) {
		ret = ltc294x_write_regs(info->client,
			LTC294X_REG_CONTROL, &control, 1);
		if (ret < 0) {
			dev_err(&info->client->dev,
				"Could not write register\n");
			goto error_exit;
		}
		DEV_DBG(&info->client->dev, "ltc294x_reset: Control register was reset\n");

		{
			int8_t value[2] = {(uint8_t)(info->charge_low_thres>>8), (uint8_t)(info->charge_low_thres) };
			ret = ltc294x_write_regs(info->client,
								LTC294X_REG_ACC_CHARGE_MSB, value, 2);

			if (ret < 0) {
				dev_err(&info->client->dev,
							"ltc294x_reset: Could not write to register LTC294X_REG_ACC_CHARGE_MSB\n");
			}
			else {
				DEV_DBG(&info->client->dev, "ltc294x_reset: ---->  Just updated LTC294X_REG_ACC_CHARGE_MSB!\n");
			}
		}

	}
#if 0
	{
		uint8_t dataw[2] = {(uint8_t)(LTC294X_THRESH_HIGH>>8), (uint8_t)LTC294X_THRESH_HIGH};
		uint8_t datar[2];

		int ret = ltc294x_read_regs(info->client, LTC294X_REG_THRESH_HIGH_MSB, &datar[0], 2);
		if (ret < 0) {
			dev_err(&info->client->dev,
						"ltc294x_work: Could not read register LTC294X_REG_THRESH_HIGH_MSB\n");
		}
		else {
			DEV_DBG(&info->client->dev, "ltc294x_update: ---->  Just read LTC294X_REG_THRESH_HIGH : %.2x%.2x\n", datar[0], datar[1]);
			if (dataw[0] != datar[0] || dataw[1] != datar[1]) {
				ret = ltc294x_write_regs(info->client, LTC294X_REG_THRESH_HIGH_MSB, &dataw[0], 2);

				if (ret < 0) {
					dev_err(&info->client->dev,
							"ltc294x_work: Could not write to register LTC294X_REG_THRESH_HIGH_MSB\n");
				}
				else {
					DEV_DBG(&info->client->dev, "ltc294x_update: ---->  Just updated LTC294X_REG_THRESH_HIGH :  %.2x%.2x\n", dataw[0], dataw[1]);
				}
			}
		}
	}
#endif
	return 0;

error_exit:
	return ret;
}

static int ltc294x_read_charge_register(const struct ltc294x_info *info)
{
	int ret;
	u8 datar[2];

	ret = ltc294x_read_regs(info->client,
		LTC294X_REG_ACC_CHARGE_MSB, &datar[0], 2);
	if (ret < 0)
		return ret;
	return (datar[0] << 8) + datar[1];
}

static int ltc294x_get_charge_now(const struct ltc294x_info *info, int *val)
{
	int ret_val;
	int value = ltc294x_read_charge_register(info);

	if (value < 0)
		return value;
	/* When r_sense < 0, this counts up when the battery discharges */
	if (info->Qlsb < 0)
		value -= 0xFFFF;
	ret_val = convert_bin_to_uAh(info, value);
	if (val)
		*val = ret_val;

	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; raw value: %d(0x%x); charge_now(uAh): %d\n",
		__func__, 0, value, value, ret_val);

	return 0;
}

static int ltc294x_set_charge_now(const struct ltc294x_info *info, int val)
{
	int ret;
	u8 dataw[2];
	u8 ctrl_reg;
	s32 value;

	value = convert_uAh_to_bin(info, val);
	/* Direction depends on how sense+/- were connected */
	if (info->Qlsb < 0)
		value += 0xFFFF;
	if ((value < 0) || (value > 0xFFFF)) /* input validation */
		return -EINVAL;

	/* Read control register */
	ret = ltc294x_read_regs(info->client,
		LTC294X_REG_CONTROL, &ctrl_reg, 1);
	if (ret < 0)
		return ret;
	/* Disable analog section */
	ctrl_reg |= LTC294X_REG_CONTROL_SHUTDOWN_MASK;
	ret = ltc294x_write_regs(info->client,
		LTC294X_REG_CONTROL, &ctrl_reg, 1);
	if (ret < 0)
		return ret;
	/* Set new charge value */
	dataw[0] = I16_MSB(value);
	dataw[1] = I16_LSB(value);
	ret = ltc294x_write_regs(info->client,
		LTC294X_REG_ACC_CHARGE_MSB, &dataw[0], 2);
	if (ret < 0)
		goto error_exit;
	/* Enable analog section */
error_exit:
	ctrl_reg &= ~LTC294X_REG_CONTROL_SHUTDOWN_MASK;
	ret = ltc294x_write_regs(info->client,
		LTC294X_REG_CONTROL, &ctrl_reg, 1);

	return ret < 0 ? ret : 0;
}

static int ltc294x_get_charge_counter(
	const struct ltc294x_info *info, int *val)
{
	int ret_val;
	int value = ltc294x_read_charge_register(info);

	if (value < 0)
		return value;
	value -= LTC294X_MID_SUPPLY;
	ret_val = convert_bin_to_uAh(info, value);
	if (val)
		*val = ret_val;
	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; raw value: %d(0x%x); charge_now(uAh): %d\n",
		__func__, 0, value, value, ret_val);
	return 0;
}

static int ltc294x_get_voltage(const struct ltc294x_info *info, int *val)
{
	int ret;
	int ret_val;
	u8 datar[2];
	u32 value;

	ret = ltc294x_read_regs(info->client,
		LTC294X_REG_VOLTAGE_MSB, &datar[0], 2);
	value = (datar[0] << 8) | datar[1];
	ret_val = ((value * 23600) / 0xFFFF) * 1000; /* in uV */
	if (val)
		*val = ret_val;
	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; raw value: %d(0x%x); voltage(uV): %d\n",
		__func__, ret, value, value, ret_val);
	return ret;
}

static int ltc294x_get_current(const struct ltc294x_info *info, int *val)
{
	int ret;
	int ret_val, orig_value;
	u8 datar[2];
	s32 value;

	ret = ltc294x_read_regs(info->client,
		LTC294X_REG_CURRENT_MSB, &datar[0], 2);
	orig_value = (datar[0] << 8) | datar[1];
	value = orig_value - 0x7FFF;
	/* Value is in range -32k..+32k, r_sense is usually 10..50 mOhm,
	 * the formula below keeps everything in s32 range while preserving
	 * enough digits */
	ret_val = 1000 * ((60000 * value) / (info->r_sense * 0x7FFF)); /* in uA */
	if (val)
		*val = ret_val;
	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; orig: %d(0x%x); adjusted: %d(0x%x); current(uA): %d\n",
		__func__, ret, orig_value, orig_value, value, value, ret_val);
	return ret;
}

static int ltc294x_get_temperature(const struct ltc294x_info *info, int *val)
{
	int ret;
	int ret_val;
	u8 datar[2];
	u32 value;

	ret = ltc294x_read_regs(info->client,
		LTC294X_REG_TEMPERATURE_MSB, &datar[0], 2);
	value = (datar[0] << 8) | datar[1];
	/* Full-scale is 510 Kelvin, convert to tenths of degree Celsius  */
	ret_val = (((51000 * value) / 0xFFFF) - 27315);
	ret_val += 5;
	ret_val /= 10;
	if (val)
		*val = ret_val;
	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; raw value: %d(0x%x); temp(10th of C): %d\n",
		__func__, ret, value, value, ret_val);
	return ret;
}
static int ltc294x_get_prop_batt_health(struct ltc294x_info *info)
{
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int ltc294x_get_prop_batt_present(struct ltc294x_info *info)
{
	return 1;
}

static int ltc294x_get_prop_batt_status(struct ltc294x_info *info)
{
	if (!info)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	return info->batt_status;
}

static int ltc294x_get_prop_charging_status(struct ltc294x_info *info)
{
	if (!info)
		return 0;
	return (info->dc_present);
}

static int ltc294x_get_prop_charge_type(struct ltc294x_info *info)
{
	return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
}

static int ltc294x_get_prop_batt_capacity(struct ltc294x_info *info)
{
	if (!info)
		return LTC294x_BATT_CAPACITY_DEFAULT;

	return info->batt_capacity;
}

static int ltc294x_get_prop_chg_full_design(struct ltc294x_info *info)
{
	int ret_val;
	int value = 0xFFFF;

	if (info->Qlsb < 0)
		value -= 0xFFFF;

	ret_val = convert_bin_to_uAh(info, value);

	DEV_DBG(&info->client->dev, "   >>> %s --> ret: %d; raw value: %d(0x%x); chg_full_design(uAh): %d\n",
		__func__, 0, value, value, ret_val);

	return ret_val;
}


static int ltc294x_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct ltc294x_info *info  = container_of(psy,
				struct ltc294x_info, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->dc_present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->chg_enabled ? info->dc_present : 0;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int ltc294x_get_property(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
#if 0
	struct ltc294x_info *info = power_supply_get_drvdata(psy);
#else
	struct ltc294x_info *info  = container_of(psy,
				struct ltc294x_info, batt_psy);
#endif
	switch (prop) {
	//case POWER_SUPPLY_PROP_PRESENT:
	//	val->intval = info->dc_present;
	//	break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->chg_enabled ? info->dc_present : 0;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = ltc294x_get_prop_batt_health(info);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = ltc294x_get_prop_batt_present(info);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = ltc294x_get_prop_batt_status(info);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = ltc294x_get_prop_charging_status(info);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = ltc294x_get_prop_charge_type(info);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = ltc294x_get_prop_batt_capacity(info);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = ltc294x_get_prop_chg_full_design(info);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		return ltc294x_get_charge_now(info, &val->intval);
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		return ltc294x_get_charge_counter(info, &val->intval);
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return ltc294x_get_voltage(info, &val->intval);
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		return ltc294x_get_current(info, &val->intval);
	case POWER_SUPPLY_PROP_TEMP:
		return ltc294x_get_temperature(info, &val->intval);
	default:
		return -EINVAL;
	}
	return 0;
}

static int ltc294x_set_property(struct power_supply *psy,
	enum power_supply_property psp,
	const union power_supply_propval *val)
{
#if 0
	struct ltc294x_info *info = power_supply_get_drvdata(psy);
#else
	struct ltc294x_info *info = container_of(psy,
				struct ltc294x_info, batt_psy);
#endif
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		return ltc294x_set_charge_now(info, val->intval);
	case POWER_SUPPLY_PROP_ONLINE:
		//It is already set...

		DEV_DBG(&info->client->dev, "ltc294x_set_property : POWER_SUPPLY_PROP_ONLINE\n");
		return 0;
	default:
		return -EPERM;
	}
}

static int ltc294x_property_is_writeable(
	struct power_supply *psy, enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		return 1;
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}
static void ltc294x_external_power_changed(struct power_supply *psy)
{
	struct ltc294x_info *info = container_of(psy,
				struct ltc294x_info, batt_psy);
	union power_supply_propval prop = {0,};
	int rc;

	DEV_DBG(&info->client->dev, "----> ltc294x_external_power_changed\n");

#if 0
	rc = info->usb_psy->get_property(info->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &prop);

	if (rc < 0)
		pr_err("ltc294x_external_power_changed: could not read USB ONLINE property, rc=%d\n", rc);
	else {
		if (prop.intval != info->dc_present) {
			rc = power_supply_set_online(info->usb_psy,	info->dc_present);
			if (rc < 0)
				pr_err("ltc294x_external_power_changed: could not set usb online, rc=%d\n", rc);
		}
	}
#endif
	rc = info->usb_psy->get_property(info->supply,
				POWER_SUPPLY_PROP_ONLINE, &prop);
	if (rc < 0)
		pr_err("ltc294x_external_power_changed: could not read BATT ONLINE property, rc=%d\n", rc);
	else {
		if (prop.intval != info->dc_present) {
			rc = power_supply_set_online(info->supply,	info->dc_present);
			if (rc < 0)
				pr_err("ltc294x_external_power_changed: could not set batt online, rc=%d\n", rc);
		}
	}
}
#define LTC294x_BATTERY_DISCHARGING_THRES_CURR				(-50000)
#define LTC294x_BATTERY_MAX_CHARGE							(0xffff)
#define LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MIN		(0x300)
#define LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX		(0xfd6f)

static void ltc294x_update(struct ltc294x_info *info, bool update_it)
{
	int gauge_voltage, gauge_current, gauge_temperature, charge_now, charge_in_progress, charge_complete, batt_status, dc_present;
	bool updated = false;
	uint32_t capacity;
	int charge = ltc294x_read_charge_register(info);

	if (charge < 0
			|| ltc294x_get_voltage(info, &gauge_voltage) < 0
			|| ltc294x_get_current(info, &gauge_current) < 0
			|| ltc294x_get_temperature(info, &gauge_temperature) < 0
			|| ltc294x_get_charge_now(info, &charge_now) < 0) {
		batt_status = POWER_SUPPLY_STATUS_UNKNOWN;

	}
	else {
		if (update_it && (gauge_voltage <= info->voltage_crit_low_thres_uV 
							|| (charge_now && charge_now <= info->charge_low_thres && gauge_voltage < info->voltage_charge_low_thres_uV)
						)) {
			if (gauge_current <= LTC294x_BATTERY_DISCHARGING_THRES_CURR) {
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> Powering off...\n");
				kernel_power_off();
			}
		}
		if (info->gpio_charge_in_progress >= 0 && info->gpio_charge_completed >= 0) {
			charge_in_progress	= gpio_get_value(info->gpio_charge_in_progress);
			charge_complete		= gpio_get_value(info->gpio_charge_completed);
		}
		else {
			charge_in_progress	= 1;
			charge_complete		= 1;
		}
		if (!charge_in_progress && charge_complete) {
			// charging
			// battery present
			batt_status = POWER_SUPPLY_STATUS_CHARGING;
			DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_CHARGING\n");
		}
		else if (charge_in_progress && !charge_complete) {
			// Charge complete
			// battery present
			batt_status = POWER_SUPPLY_STATUS_FULL;
			DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_FULL\n");
		}
		else if (charge_in_progress && charge_complete) {
			// Unknown
			if (gauge_current <= LTC294x_BATTERY_DISCHARGING_THRES_CURR ) {
				//discharging
				//battery present
				dc_present = 0;
				batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_DISCHARGING\n");
			}
			else if (charge == LTC294x_BATTERY_MAX_CHARGE) {
				batt_status = POWER_SUPPLY_STATUS_FULL;
				dc_present = 1;
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_FULL\n");
			}
			else if ((info->charge >= LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX && charge <= LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MIN)
						|| charge > info->charge) {
				// charging
				// battery present
				dc_present = 1;
				batt_status = POWER_SUPPLY_STATUS_CHARGING;
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_CHARGING slowly\n");
			}
			else if (charge < info->charge) {
				// discharging
				// battery present
				dc_present = 0;
				batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> POWER_SUPPLY_STATUS_DISCHARGING slowly\n");
			}
			else {
				// Just keep the previous state...
				batt_status = info->batt_status;
				DEV_DBG(&info->client->dev, "ltc294x_update: ----> Batt_status:   Keep it the same...\n");
			}

		}
		else {
			//error
			batt_status = POWER_SUPPLY_STATUS_UNKNOWN;
			DEV_DBG(&info->client->dev, "ltc294x_update: --!!!!!!!!-->  charge_in_progress == 0 && charge_in_progress == 0\n");
		}

	}

	updated = false;
	if (batt_status != info->batt_status) {
		info->batt_status = batt_status;
		updated = true;
		if (info->batt_status == POWER_SUPPLY_STATUS_FULL) {
#if 0
			uint8_t value[2] = {0xff, 0xff};
			int ret = ltc294x_write_regs(info->client,
								LTC294X_REG_ACC_CHARGE_MSB, value, 2);

			if (ret < 0) {
				dev_err(&info->client->dev,
							"ltc294x_work: Could not write to register LTC294X_REG_ACC_CHARGE_MSB\n");
			}
			else {
				DEV_DBG(&info->client->dev, "ltc294x_update: ---->  Just updated LTC294X_REG_ACC_CHARGE_MSB!\n");
			}
#endif
			charge_now = LTC294x_BATTERY_MAX_CHARGE;
		}

	}
	if (info->charge >= LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX && charge <= LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MIN) {
		uint8_t value[2] = {(uint8_t)(LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX>>8), (uint8_t)LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX};
		int ret;
		charge = LTC294x_BATTERY_CHARGE_OVERFLOW_PROTECTION_MAX;

		ret = ltc294x_write_regs(info->client,
							LTC294X_REG_ACC_CHARGE_MSB, value, 2);

		if (ret < 0) {
			dev_err(&info->client->dev,
						"ltc294x_work: Could not write to register LTC294X_REG_ACC_CHARGE_MSB\n");
		}
		else {
			DEV_DBG(&info->client->dev, "ltc294x_update: ---->  Just updated LTC294X_REG_ACC_CHARGE_MSB!\n");
		}
	}
	if (charge != info->charge) {
		uint16_t batt_max_capacity = LTC294x_BATTERY_MAX_CHARGE;
		info->charge = charge;

		if (batt_max_capacity > info->charge_low_thres) {
			batt_max_capacity -= info->charge_low_thres;
		}
		if (charge <= info->charge_low_thres) {
			charge = 0;
		}
		else {
			charge -= info->charge_low_thres;
		}
		capacity = 0;
		if (charge) {
			capacity = charge;
			capacity *= 10000;
			capacity /= batt_max_capacity;
			capacity += 50;
			capacity /=100;
		}

		if (!capacity) {
			//If capacity is 0% always show 1% as the device is to be powered off soon... 
			capacity = LTC294x_BATT_CAPACITY_MIN;
		}

		if (capacity != info->batt_capacity) {
			info->batt_capacity = capacity;
			updated = true;
		}
	}

	if (updated && update_it) {
		power_supply_changed(info->supply);
	}

	if (info->dc_present != dc_present) {
		info->dc_present = dc_present;
		if (update_it) {
			power_supply_changed(&info->dc_psy);
			power_supply_changed(info->supply);
		}
	}
}

static void ltc294x_work(struct work_struct *work)
{
	struct ltc294x_info *info;

	info = container_of(work, struct ltc294x_info, work.work);

	if (ltc294x_reset(info, info->prescaler_exp) < 0) {
	}
	else {
		ltc294x_update(info, true);
	}

	schedule_delayed_work(&info->work, LTC294X_WORK_DELAY * HZ);
}

static int determine_initial_status(struct ltc294x_info *info)
{
	int ret = ltc294x_reset(info, info->prescaler_exp);
	if (info) {
		info->batt_status	= POWER_SUPPLY_STATUS_UNKNOWN;
		info->batt_capacity = LTC294x_BATT_CAPACITY_DEFAULT;
		if (ret < 0) {
			// Do nothing
		}
		else {
			info->batt_status = POWER_SUPPLY_STATUS_UNKNOWN;
			info->batt_capacity = LTC294x_BATT_CAPACITY_DEFAULT;
			ltc294x_update(info, false);
		}
	}
	else {
		return -1;
	}

	return 0;
}

static enum power_supply_property ltc294x_dc_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
};


static enum power_supply_property ltc294x_properties[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static int ltc294x_i2c_remove(struct i2c_client *client)
{
	struct ltc294x_info *info = i2c_get_clientdata(client);

	cancel_delayed_work(&info->work);
	power_supply_unregister(info->supply);

	if (info->gpio_charge_in_progress >= 0) {
		gpio_free(info->gpio_charge_in_progress);
		info->gpio_charge_in_progress = -1;
	}
	if (info->gpio_charge_completed >= 0) {
		gpio_free(info->gpio_charge_completed);
		info->gpio_charge_completed = -1;
	}
	return 0;
}

static char *ltc294x_power_dc_supplied_to[] = {
	"battery-gauge",
};


static int ltc294x_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
#if 0
	struct power_supply_config psy_cfg = {};
#endif
	struct ltc294x_info *info;
	int ret;
	u32 prescaler_exp;
	s32 r_sense;
	u32 charge_low_thres;
	u32 voltage_crit_low_thres_uV;
	u32 voltage_charge_low_thres_uV;
	struct device_node *np;
	struct power_supply *usb_psy;

	dev_info(&client->dev, "ltc294x_i2c_probe. Version: %s\n", LTC294x_DRIVER_VERSION);

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_err(&client->dev, "ltc294x_i2c_probe: USB supply not found; defer probe\n");
		return -EPROBE_DEFER;
	}

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	device_init_wakeup(&client->dev, 1);
	i2c_set_clientdata(client, info);

	np = of_node_get(client->dev.of_node);

	info->num_regs = id->driver_data;
	info->batt_psy.name = "battery-gauge";//np->name;

	info->dc_psy.name = "dc";
	info->charge_low_thres				= LTC2941_MIN_CHARGE_LOW_THRES;
	info->voltage_crit_low_thres_uV		= LTC2941_VOLTAGE_CRIT_LOW_THRES_uV;
	info->voltage_charge_low_thres_uV	= LTC2941_VOLTAGE_CHARGE_LOW_THRES_uV;

	/* r_sense can be negative, when sense+ is connected to the battery
	 * instead of the sense-. This results in reversed measurements. */
	ret = of_property_read_u32(np, "lltc,resistor-sense", &r_sense);
	if (ret < 0) {
		dev_err(&client->dev,
			"Could not find lltc,resistor-sense in devicetree\n");
		return ret;
	}
	info->r_sense = r_sense;

	ret = of_property_read_u32(np, "lltc,prescaler-exponent",
		&prescaler_exp);
	if (ret < 0) {
		dev_warn(&client->dev,
			"ltc294x_i2c_probe: lltc,prescaler-exponent not in devicetree\n");
		prescaler_exp = LTC2941_MAX_PRESCALER_EXP;
	}

	ret = of_property_read_u32(np, "sciaps,charge-low-thres",
		&charge_low_thres);

	if (ret < 0) {
		dev_warn(&client->dev,
			"ltc294x_i2c_probe: sciaps,charge-low-thres not in devicetree. Using the default value: %d \n", LTC2941_MIN_CHARGE_LOW_THRES);
	}
	else {
		if (charge_low_thres >= LTC294X_MID_SUPPLY) {
			dev_warn(&client->dev,
				"ltc294x_i2c_probe: sciaps,charge-low-thres invalid value. Using the default value: %d \n", LTC2941_MIN_CHARGE_LOW_THRES);
		}
		else {
			info->charge_low_thres = charge_low_thres;
		}
	}

	ret = of_property_read_u32(np, "sciaps,voltage-crit-low-thres-uV",
		&voltage_crit_low_thres_uV);

	if (ret < 0) {
		dev_warn(&client->dev,
			"ltc294x_i2c_probe: sciaps,voltage-crit-low-thres-uV not in devicetree. Using the default value: %d \n", LTC2941_VOLTAGE_CRIT_LOW_THRES_uV);
	}
	else {
		info->voltage_crit_low_thres_uV = voltage_crit_low_thres_uV;
	}

	ret = of_property_read_u32(np, "sciaps,voltage-charge-low-thres-uV",
		&voltage_charge_low_thres_uV);

	if (ret < 0) {
		dev_warn(&client->dev,
			"ltc294x_i2c_probe: sciaps,voltage-low-thres-uV not in devicetree. Using the default value: %d \n", LTC2941_VOLTAGE_CHARGE_LOW_THRES_uV);
	}
	else {
		info->voltage_charge_low_thres_uV = voltage_charge_low_thres_uV;
	}

	if (1 == of_gpio_named_count(np, "sciaps,gpio-charge-in-progress")) {
		info->gpio_charge_in_progress = of_get_named_gpio(np, "sciaps,gpio-charge-in-progress", 0);

		dev_info(&client->dev,
			"ltc294x_i2c_probe : sciaps,gpio-charge-in-progress is %d\n", info->gpio_charge_in_progress);
		if (gpio_is_valid(info->gpio_charge_in_progress)) {
			dev_info(&client->dev,
				"ltc294x_i2c_probe : sciaps,gpio-charge-in-progress gpio is valid\n");
			if ((ret = gpio_request(info->gpio_charge_in_progress, "sciaps-battery-charge-in-progress")) < 0) {
				dev_warn(&client->dev,
					"ltc294x_i2c_probe : sciaps,gpio-charge-in-progress gpio request failed: %d\n", ret);
				info->gpio_charge_in_progress = -1;
			}
		}
		else {
			dev_warn(&client->dev,
				"ltc294x_i2c_probe : sciaps,gpio-charge-in-progress gpio is NOT valid\n");
			info->gpio_charge_in_progress = -1;
		}
	}
	else {
		info->gpio_charge_in_progress = -1;
		dev_warn(&client->dev,
			"Could not find sciaps,gpio-charge-in-progress in the devicetree\n");
	}

	if (1 == of_gpio_named_count(np, "sciaps,gpio-charge-completed")) {
		info->gpio_charge_completed = of_get_named_gpio(np, "sciaps,gpio-charge-completed", 0);

		dev_info(&client->dev,
			"ltc294x_i2c_probe : sciaps,gpio-charge-completed is %d\n", info->gpio_charge_completed);

		if (gpio_is_valid(info->gpio_charge_completed)) {
			dev_info(&client->dev,
				"ltc294x_i2c_probe : sciaps,gpio-charge-completed gpio is valid\n");
			if ((ret = gpio_request(info->gpio_charge_completed, "sciaps-battery-charge-completed")) < 0) {
				dev_warn(&client->dev,
					"ltc294x_i2c_probe : sciaps,gpio-charge-completed gpio request failed: %d\n", ret);
				if(info->gpio_charge_in_progress >= 0) {
					gpio_free(info->gpio_charge_in_progress);
					info->gpio_charge_in_progress = -1;
				}
				info->gpio_charge_completed = -1;
			}

		}
		else {
			info->gpio_charge_completed = -1;
			dev_warn(&client->dev,
				"ltc294x_i2c_probe : sciaps,gpio-charge-completed gpio is NOT valid\n");
			if(info->gpio_charge_in_progress >= 0) {
				gpio_free(info->gpio_charge_in_progress);
				info->gpio_charge_in_progress = -1;
			}
		}
	}
	else {
		info->gpio_charge_completed = -1;
		dev_warn(&client->dev,
			"Could not find sciaps,gpio-charge-completed in devicetree\n");
		if(info->gpio_charge_in_progress >= 0) {
			gpio_free(info->gpio_charge_in_progress);
			info->gpio_charge_in_progress = -1;
		}
	}

	if (info->gpio_charge_in_progress >= 0 && info->gpio_charge_completed>= 0) {
		if ((ret = gpio_direction_input(info->gpio_charge_in_progress)) < 0
				|| (ret = gpio_direction_input(info->gpio_charge_completed)) < 0) {
			dev_err(&client->dev,
				"ltc294x_i2c_probe : sciaps,gpio-charge-completed or sciaps,gpio-charge-in-progress gpio dir set failed: %d\n", ret);
			gpio_free(info->gpio_charge_in_progress);
			info->gpio_charge_in_progress = -1;
			gpio_free(info->gpio_charge_completed);
			info->gpio_charge_completed = -1;
			return ret;
		}
	}

	if (info->num_regs == LTC2943_NUM_REGS) {
		if (prescaler_exp > LTC2943_MAX_PRESCALER_EXP)
			prescaler_exp = LTC2943_MAX_PRESCALER_EXP;
		info->Qlsb = ((340 * 50000) / r_sense) /
				(4096 / (1 << (2*prescaler_exp)));
	} else {
		if (prescaler_exp > LTC2941_MAX_PRESCALER_EXP)
			prescaler_exp = LTC2941_MAX_PRESCALER_EXP;
		info->Qlsb = ((85 * 50000) / r_sense) /
				(128 / (1 << prescaler_exp));
	}
	info->prescaler_exp = prescaler_exp;


	info->client = client;
	info->dev = &client->dev;
	info->usb_psy = usb_psy;
	info->batt_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	info->batt_psy.properties = ltc294x_properties;
	if (info->num_regs >= LTC294X_REG_TEMPERATURE_LSB)
		info->batt_psy.num_properties =
			ARRAY_SIZE(ltc294x_properties);
	else if (info->num_regs >= LTC294X_REG_CURRENT_LSB)
		info->batt_psy.num_properties =
			ARRAY_SIZE(ltc294x_properties) - 1;
	else if (info->num_regs >= LTC294X_REG_VOLTAGE_LSB)
		info->batt_psy.num_properties =
			ARRAY_SIZE(ltc294x_properties) - 2;
	else
		info->batt_psy.num_properties =
			ARRAY_SIZE(ltc294x_properties) - 3;
	info->batt_psy.get_property				= ltc294x_get_property;
	info->batt_psy.set_property				= ltc294x_set_property;
	info->batt_psy.property_is_writeable	= ltc294x_property_is_writeable;
	info->batt_psy.external_power_changed	= ltc294x_external_power_changed;


	//info->dc_psy.type			= POWER_SUPPLY_TYPE_WIRELESS;
	//info->dc_psy.type			= POWER_SUPPLY_TYPE_UPS;
	info->dc_psy.type			= POWER_SUPPLY_TYPE_MAINS;
	info->dc_psy.get_property	= ltc294x_dc_get_property;
	info->dc_psy.properties		= ltc294x_dc_properties;
	info->dc_psy.num_properties = ARRAY_SIZE(ltc294x_dc_properties);
	info->dc_psy.supplied_to	= ltc294x_power_dc_supplied_to;
	info->dc_psy.num_supplicants = ARRAY_SIZE(ltc294x_power_dc_supplied_to);
	info->dc_present	= 0;
	info->chg_enabled	= 1;

#if 0
	psy_cfg.drv_data = info;
#endif

	INIT_DELAYED_WORK(&info->work, ltc294x_work);

	ret = determine_initial_status(info);
	if (ret < 0) {
		dev_err(&client->dev,
			"ltc294x_i2c_probe: Unable to determine init status rc = %d\n", ret);
		if (info->gpio_charge_in_progress >= 0) {
			gpio_free(info->gpio_charge_in_progress);
			info->gpio_charge_in_progress = -1;
		}
		if (info->gpio_charge_completed >= 0) {
			gpio_free(info->gpio_charge_completed);
			info->gpio_charge_completed = -1;
		}
		return ret;
	}
	//ret = ltc294x_reset(info, prescaler_exp);
	//if (ret < 0) {
	//	dev_err(&client->dev, "ltc294x_i2c_probe: Communication with chip failed\n");
	//	return ret;
	//}
#if 0
	info->supply = power_supply_register(&client->dev, &info->supply_desc,
					     &psy_cfg);
	if (IS_ERR(info->supply)) {
		dev_err(&client->dev, "failed to register ltc2941\n");
		return PTR_ERR(info->supply);
	} else {
		schedule_delayed_work(&info->work, LTC294X_WORK_DELAY * HZ);
	}
#else
	ret = power_supply_register(&client->dev, &info->batt_psy);
	if (ret < 0) {
		dev_err(&client->dev, "ltc294x_i2c_probe: failed to register ltc2941\n");
		if (info->gpio_charge_in_progress >= 0) {
			gpio_free(info->gpio_charge_in_progress);
			info->gpio_charge_in_progress = -1;
		}
		if (info->gpio_charge_completed >= 0) {
			gpio_free(info->gpio_charge_completed);
			info->gpio_charge_completed = -1;
		}
		return ret;
	}
	else {
		info->supply = &info->batt_psy;
		ret = power_supply_register(&client->dev, &info->dc_psy);
		if ( ret < 0) {
			dev_err(&client->dev,
				"ltc294x_i2c_probe: Unable to register dc_psy ret = %d\n", ret);
		}


		schedule_delayed_work(&info->work, LTC294X_WORK_DELAY * HZ);
	}

#endif
	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int ltc294x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltc294x_info *info = i2c_get_clientdata(client);

	cancel_delayed_work(&info->work);
	return 0;
}

static int ltc294x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ltc294x_info *info = i2c_get_clientdata(client);

	schedule_delayed_work(&info->work, LTC294X_WORK_DELAY * HZ);
	return 0;
}

static SIMPLE_DEV_PM_OPS(ltc294x_pm_ops, ltc294x_suspend, ltc294x_resume);
#define LTC294X_PM_OPS (&ltc294x_pm_ops)

#else
#define LTC294X_PM_OPS NULL
#endif /* CONFIG_PM_SLEEP */


static const struct i2c_device_id ltc294x_i2c_id[] = {
	{"ltc2941", LTC2941_NUM_REGS},
	{"ltc2943", LTC2943_NUM_REGS},
	{ },
};
MODULE_DEVICE_TABLE(i2c, ltc294x_i2c_id);

static struct i2c_driver ltc294x_driver = {
	.driver = {
		.name	= "LTC2941",
		.pm	= LTC294X_PM_OPS,
	},
	.probe		= ltc294x_i2c_probe,
	.remove		= ltc294x_i2c_remove,
	.id_table	= ltc294x_i2c_id,
};
module_i2c_driver(ltc294x_driver);

MODULE_AUTHOR("Auryn Verwegen, Topic Embedded Systems");
MODULE_AUTHOR("Mike Looijmans, Topic Embedded Products");
MODULE_DESCRIPTION("LTC2941/LTC2943 Battery Gas Gauge IC driver");
MODULE_LICENSE("GPL");
