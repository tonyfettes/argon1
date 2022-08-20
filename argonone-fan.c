// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/pm.h>

#define ARGONONE_FAN_DEVICE_NAME "argonone-fan"

static struct i2c_device_id const argonone_fan_id_table[] = {
	{ ARGONONE_FAN_DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, argonone_fan_id_table);

static unsigned short const argonone_fan_address = 0x1a;
static unsigned short const argonone_fan_address_list[] = {
	argonone_fan_address, I2C_CLIENT_END
};

static int argonone_fan_probe(struct i2c_client *client);
static int argonone_fan_remove(struct i2c_client *client);
static void argonone_fan_shutdown(struct i2c_client *client);
static int argonone_fan_suspend(struct device *device);
static int argonone_fan_resume(struct device *device);

static ssize_t argonone_fan_attr_min_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argonone_fan_attr_min = {
	.attr = { .name = "fan1_min", .mode = 0444, },
	.show = argonone_fan_attr_min_show,
};

static ssize_t argonone_fan_attr_max_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argonone_fan_attr_max = {
	.attr = { .name = "fan1_max", .mode = 0444, },
	.show = argonone_fan_attr_max_show,
};

static ssize_t argonone_fan_attr_input_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argonone_fan_attr_input = {
	.attr = { .name = "fan1_input", .mode = 0444, },
	.show = argonone_fan_attr_input_show,
};

static ssize_t argonone_fan_attr_target_store(struct device *device, struct device_attribute *attribute, const char *buffer, size_t count);
static struct device_attribute argonone_fan_attr_target = {
	.attr = { .name = "fan1_target", .mode = 0644, },
	.show = argonone_fan_attr_input_show,
	.store = argonone_fan_attr_target_store,
};

static struct attribute *argonone_fan_hwmon_attr[] = {
	&argonone_fan_attr_input.attr,
	&argonone_fan_attr_target.attr,
	&argonone_fan_attr_min.attr,
	&argonone_fan_attr_max.attr,
	NULL,
};

static const struct attribute_group argonone_fan_hwmon_group = {
	.attrs = argonone_fan_hwmon_attr,
};

static const struct attribute_group *argonone_fan_hwmon_groups[] = {
	&argonone_fan_hwmon_group,
	NULL
};

static int argonone_fan_get_max_state(struct thermal_cooling_device *device, unsigned long *target);
static int argonone_fan_get_cur_state(struct thermal_cooling_device *device, unsigned long *target);
static int argonone_fan_set_cur_state(struct thermal_cooling_device *device, unsigned long target);

static struct thermal_cooling_device_ops const argonone_fan_cl_dev_ops = {
	.get_max_state = argonone_fan_get_max_state,
	.get_cur_state = argonone_fan_get_cur_state,
	.set_cur_state = argonone_fan_set_cur_state,
};

static struct dev_pm_ops argonone_fan_pm_ops = {
	.suspend = argonone_fan_suspend,
	.resume = argonone_fan_resume,
};

static struct i2c_driver argonone_fan_driver = {
	.driver = {
		.name = ARGONONE_FAN_DEVICE_NAME,
		.pm = &argonone_fan_pm_ops,
	},
	.id_table = argonone_fan_id_table,
	.probe_new = argonone_fan_probe,
	.remove = argonone_fan_remove,
	.class = I2C_CLASS_HWMON,
	.address_list = argonone_fan_address_list,
	.shutdown = argonone_fan_shutdown,
};
module_i2c_driver(argonone_fan_driver);

struct argonone_fan_data {
	struct thermal_cooling_device *cl_dev;
	struct device *hwmon_dev;
	unsigned short throttle;
};

static int argonone_fan_probe(struct i2c_client *client)
{
	int err;
	struct argonone_fan_data *data;
	struct device *dev;
	struct device_node *np;

	dev = &client->dev;
	np = dev->of_node;

	data = devm_kmalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = i2c_smbus_write_byte(client, 0);
	if (err)
		return err;

	data->throttle = 0;

	/* The data is set before registering any of these device since once a device is registered, then coresponding callbacks will be called and they would find out data is NULL. */
	i2c_set_clientdata(client, data);

	data->hwmon_dev = devm_hwmon_device_register_with_groups(dev, ARGONONE_FAN_DEVICE_NAME, client, argonone_fan_hwmon_groups);
	if (IS_ERR(data->hwmon_dev))
		return PTR_ERR(data->hwmon_dev);

	data->cl_dev = devm_thermal_of_cooling_device_register(dev, np, "argonone-fan", client, &argonone_fan_cl_dev_ops);
	if (IS_ERR(data->cl_dev))
		return PTR_ERR(data->cl_dev);

	return 0;
}

static int argonone_fan_remove(struct i2c_client *client)
{
	return i2c_smbus_write_byte(client, 0);
}

static void argonone_fan_shutdown(struct i2c_client *client)
{
	i2c_smbus_write_byte(client, 0);
}

static int argonone_fan_suspend(struct device *device)
{
	int err;
	struct i2c_client *client;

	client = to_i2c_client(device);
	err = i2c_smbus_write_byte(client, 0);
	if (err)
		return err;

	return 0;
}

static int argonone_fan_resume(struct device *device)
{
	int err;
	struct i2c_client *client;
	struct argonone_fan_data *data;

	client = to_i2c_client(device);
	data = dev_get_drvdata(device);
	err = i2c_smbus_write_byte(client, data->throttle);
	if (err)
		return err;

	return 0;
}

#if 0
static int argonone_fan_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	int ret;
	int address = info->addr;
	struct i2c_adapter *adapter = client->adapter;

	printk(KERN_INFO "Detecting Argon ONE fan...\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE_DATA | I2C_FUNC_SMBUS_QUICK))
		return -ENODEV;

	if (address != 0x1a)
		return -ENODEV;

	ret = i2c_smbus_xfer(client->adapter, client->addr, 0, I2C_SMBUS_WRITE, 0, I2C_SMBUS_QUICK, NULL);
	if (ret < 0)
		return -ENODEV;

	printk(KERN_INFO "Detected Argon ONE fan.\n");

	return 0;
}
#endif

static ssize_t argonone_fan_attr_min_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	return sprintf(buffer, "0\n");
}

static ssize_t argonone_fan_attr_max_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	return sprintf(buffer, "100\n");
}

static ssize_t argonone_fan_attr_input_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	struct i2c_client *client;
	struct argonone_fan_data *data;

	client = dev_get_drvdata(device);
	data = i2c_get_clientdata(client);

	return sprintf(buffer, "%u\n", data->throttle);
}

static ssize_t argonone_fan_attr_target_store(struct device *device, struct device_attribute *attribute, const char *buffer, size_t count)
{
	struct i2c_client *client;
	struct argonone_fan_data *data;
	u8 target;
	int err;

	if (kstrtou8(buffer, 10, &target) || target > 100)
		return -EINVAL;

	client = dev_get_drvdata(device);

	err = i2c_smbus_write_byte(client, target);
	if (err)
		return err;

	data = i2c_get_clientdata(client);
	data->throttle = target;

	return 0;
}

static int argonone_fan_get_max_state(struct thermal_cooling_device *device, unsigned long *target)
{
	*target = 100;
	return 0;
}

static int argonone_fan_get_cur_state(struct thermal_cooling_device *device, unsigned long *target) {
	struct i2c_client *client;
	struct argonone_fan_data *data;

	client = device->devdata;
	data = i2c_get_clientdata(client);

	*target = data->throttle;

	return 0;
}

static int argonone_fan_set_cur_state(struct thermal_cooling_device *device, unsigned long target)
{
	int err;
	struct i2c_client *client;
	struct argonone_fan_data *data;

	if (target > 100)
		return -EINVAL;

	client = device->devdata;
	err = i2c_smbus_write_byte(client, target);
	if (err)
		return err;

	data = i2c_get_clientdata(client);
	data->throttle = target;

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony Fettes <tonyfettes@tonyfettes.com>");
MODULE_DESCRIPTION("Driver for fan on Argon ONE case for Raspberry Pi");
