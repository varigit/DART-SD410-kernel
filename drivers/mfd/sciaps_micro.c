#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/types.h>

#include <linux/mfd/sciaps_micro.h>


struct sciaps_micro_data {
	struct	i2c_client	*client;
	u8 read_reg;
	u16 read_reg_value;
	bool			nvram_unlocked;
};

typedef struct {
	const char	*command;
	u8		buf[3];
} sciaps_micro_cmds;

static sciaps_micro_cmds sciaps_micro_known_cmds[] = {
	{ "reset_micro",	{0xA0, 0x7A, 0xA7} },
	{ "enter_bl",		{0xA1, 0x3C, 0xC3} },
	{ "unlock_nvram",	{0xA2, 0x55, 0xAA} },
};

static const int libs_cmd_num = sizeof(sciaps_micro_known_cmds)/sizeof(sciaps_micro_cmds);

#if 0
static int sciaps_micro_i2c_read(struct i2c_client *client, int count,
			u8 *buf)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= count,
			.buf	= buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		dev_err(&client->dev, "%s read error %d\n", __func__, ret);
	return ret;
}
#endif

static int sciaps_micro_i2c_write(struct i2c_client *client,
			u8 *buf, int count)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= count,
			.buf	= buf,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s write error %d\n", __func__, ret);
	return ret;
}

static int sciaps_micro_i2c_write_reg(struct i2c_client *client, u8 reg,
	u16 value)
{
	int ret;
	u8 value_buf[2] = { 0xFF & value, 0xFF & (value >> 8) };
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= value_buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		dev_err(&client->dev, "%s read error %d\n", __func__, ret);
	return ret;
}

static int sciaps_micro_i2c_read_reg(struct i2c_client *client, u8 reg,
	u16 *value)
{
	int ret;
	u8 value_buf[2];
	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &reg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 2,
			.buf	= value_buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s read error %d\n", __func__, ret);
	} else {
		*value = (value_buf[1] << 8) | value_buf[0];
	}
	return ret;
}

static ssize_t sciaps_micro_print_reg(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sciaps_micro_data* drvdata = i2c_get_clientdata(client);

	return scnprintf(buf, PAGE_SIZE, "0x%02x 0x%04x\n",
		drvdata->read_reg, drvdata->read_reg_value);
}


static ssize_t sciaps_micro_set_read_reg(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct i2c_client* client = to_i2c_client(dev);
	struct sciaps_micro_data* drvdata = i2c_get_clientdata(client);
	int err;

	err = kstrtou8(buf, 16, &drvdata->read_reg);
	if(err < 0){
		dev_err(dev, "Error parsing register num\n");
		return -EINVAL;
	}

	err = sciaps_micro_i2c_read_reg(drvdata->client,
		drvdata->read_reg, &drvdata->read_reg_value);
	if(err < 0) {
		dev_err(dev, "error reading from i2c bus: 0x%x", err);
		return err;
	}

	return err;
}

static ssize_t sciaps_micro_store_reg(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct i2c_client* client = to_i2c_client(dev);
	int err;
	u8 reg;
	u16 value;

	err = sscanf(buf, "%hhu %hx", &reg, &value);
	if(err != 2) {
		dev_err(dev, "error parsing command\n");
		return -EINVAL;
	}

	err = sciaps_micro_i2c_write_reg(client, reg, value);
	if(err < 0) {
		dev_err(dev, "error writing i2c command: 0x%x\n", err);
	}

	return err;
}

static ssize_t sciaps_micro_write_data(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct i2c_client* client = to_i2c_client(dev);
	int err;

	err = sciaps_micro_i2c_write(client, (u8*)buf, count);
	if(err < 0) {
		dev_err(dev, "error writing i2c data: 0x%x\n", err);
	}

	return err;
}

static DEVICE_ATTR(read_reg, 0666, sciaps_micro_print_reg, sciaps_micro_set_read_reg);
static DEVICE_ATTR(write_reg, 0222, NULL, sciaps_micro_store_reg);
static DEVICE_ATTR(write, 0222, NULL, sciaps_micro_write_data);

static int sciaps_micro_setup_sysfs(struct i2c_client *client)
{
	device_create_file(&client->dev, &dev_attr_read_reg);
	device_create_file(&client->dev, &dev_attr_write_reg);
	device_create_file(&client->dev, &dev_attr_write);

	return 0;
}

static int sciaps_micro_delete_sysfs(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_read_reg);
	device_remove_file(&client->dev, &dev_attr_write_reg);
	device_remove_file(&client->dev, &dev_attr_write);

	return 0;
}

static int sciaps_micro_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sciaps_micro_data *sciaps_micro;

	sciaps_micro = kzalloc(sizeof(struct sciaps_micro_data), GFP_KERNEL);
	if (!sciaps_micro) {
		dev_err(&client->dev, "cannot allocate memory\n");
		return -ENOMEM;
	}

	sciaps_micro->client = client;
	sciaps_micro->read_reg = 0;
	sciaps_micro->read_reg_value = 0;
	sciaps_micro->nvram_unlocked = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c not supported\n");
		return -EPFNOSUPPORT;
	}

	i2c_set_clientdata(client, sciaps_micro);
	sciaps_micro_setup_sysfs(client);
	device_create_file(&client->dev, &dev_attr_read_reg);

	dev_info(&client->dev, "device probed\n");

	return 0;
}
static int sciaps_micro_remove(struct i2c_client *client)
{
	struct sciaps_micro_data *sciaps_micro;

	sciaps_micro = i2c_get_clientdata(client);

	sciaps_micro_delete_sysfs(client);
	i2c_unregister_device(client);

	kfree(sciaps_micro);

	return 0;
}

static void sciaps_micro_shutdown(struct i2c_client *client)
{
	int err;

	err = sciaps_micro_i2c_write(client, sciaps_micro_known_cmds[0].buf, 3);
  if (err < 0) {
		dev_err(&client->dev, "%s: could not shutdown PIC\n", __func__);
	}
}

static struct i2c_device_id sciaps_micro_idtable[] = {
	{ "sciaps_micro", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sciaps_micro_idtable);

static struct i2c_driver sciaps_micro_driver = {
	.driver = {
		.name   = "sciaps_micro",
	},

	.id_table   = sciaps_micro_idtable,
	.probe      = sciaps_micro_probe,
	.remove     = sciaps_micro_remove,
	.shutdown   = sciaps_micro_shutdown,
};

module_i2c_driver(sciaps_micro_driver);

MODULE_AUTHOR("Paul Soucy <paul@dev-smart.com>");
MODULE_DESCRIPTION("Sciaps Power Board Micro I2C client driver");
MODULE_LICENSE("GPL");
