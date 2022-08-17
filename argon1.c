#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/moduleparam.h>
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#define SUCCESS 0

#define ARGON1_DEVICE_NAME "argon1-fan"

static struct i2c_device_id const argon1_i2c_id_table[] = {
	{ ARGON1_DEVICE_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, argon1_i2c_id_table);

static unsigned short const argon1_i2c_address = 0x1a;
static unsigned short const argon1_i2c_address_list[] = {
	argon1_i2c_address, I2C_CLIENT_END
};

static int argon1_i2c_probe(struct i2c_client *client);
static int argon1_i2c_remove(struct i2c_client *client);
static int argon1_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static void argon1_i2c_shutdown(struct i2c_client *client);
static int argon1_i2c_suspend(struct device *device);
static int argon1_i2c_resume(struct device *device);

static ssize_t argon1_i2c_attr_fan1_min_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argon1_i2c_attr_fan1_min = {
	.attr = { .name = "fan1_min", .mode = 0444, },
	.show = argon1_i2c_attr_fan1_min_show,
};

static ssize_t argon1_i2c_attr_fan1_max_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argon1_i2c_attr_fan1_max = {
	.attr = { .name = "fan1_max", .mode = 0444, },
	.show = argon1_i2c_attr_fan1_max_show,
};

static ssize_t argon1_i2c_attr_fan1_input_show(struct device *device, struct device_attribute *attribute, char *buffer);
static struct device_attribute argon1_i2c_attr_fan1_input = {
	.attr = { .name = "fan1_input", .mode = 0444, },
	.show = argon1_i2c_attr_fan1_input_show,
};

static ssize_t argon1_i2c_attr_fan1_target_store(struct device *device, struct device_attribute *attribute, const char *buffer, size_t count);
static struct device_attribute argon1_i2c_attr_fan1_target = {
	.attr = { .name = "fan1_target", .mode = 0644, },
	.show = argon1_i2c_attr_fan1_input_show,
	.store = argon1_i2c_attr_fan1_target_store,
};

static struct attribute *argon1_i2c_attributes[] = {
	&argon1_i2c_attr_fan1_input.attr,
	&argon1_i2c_attr_fan1_target.attr,
	&argon1_i2c_attr_fan1_min.attr,
	&argon1_i2c_attr_fan1_max.attr,
	NULL,
};

static const struct attribute_group argon1_i2c_group = {
	.attrs = argon1_i2c_attributes,
};

static const struct attribute_group *argon1_i2c_groups[] = {
	&argon1_i2c_group,
	NULL
};

static int argon1_i2c_get_max_state(struct thermal_cooling_device *device, unsigned long *target);
static int argon1_i2c_get_cur_state(struct thermal_cooling_device *device, unsigned long *target);
static int argon1_i2c_set_cur_state(struct thermal_cooling_device *device, unsigned long target);

static struct thermal_cooling_device_ops const argon1_i2c_cl_dev_ops = {
	.get_max_state = argon1_i2c_get_max_state,
	.get_cur_state = argon1_i2c_get_cur_state,
	.set_cur_state = argon1_i2c_set_cur_state,
};

static struct dev_pm_ops argon1_i2c_pm_ops = {
	.suspend = argon1_i2c_suspend,
	.resume = argon1_i2c_resume,
};

static struct i2c_driver argon1_i2c_driver = {
	.driver = {
		.name = ARGON1_DEVICE_NAME,
		.pm = &argon1_i2c_pm_ops,
	},
	.id_table = argon1_i2c_id_table,
	.probe_new = argon1_i2c_probe,
	.remove = argon1_i2c_remove,
	.class = I2C_CLASS_HWMON,
	// .detect = argon1_i2c_detect,
	.address_list = argon1_i2c_address_list,
	.shutdown = argon1_i2c_shutdown,
};

struct argon1_i2c_data {
	struct thermal_cooling_device *cl_dev;
	struct device *hwmon_dev;
	unsigned short throttle;
};

static int argon1_i2c_probe(struct i2c_client *client)
{
	int err;
	struct argon1_i2c_data *data;
	struct device *dev;
	struct device_node *np;

	dev = &client->dev;
	np = dev->of_node;

	data = devm_kmalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->hwmon_dev = devm_hwmon_device_register_with_groups(dev, ARGON1_DEVICE_NAME, client, argon1_i2c_groups);
	if (IS_ERR(data->hwmon_dev))
		return PTR_ERR(data->hwmon_dev);

	data->cl_dev = devm_thermal_of_cooling_device_register(dev, np, "argon1-fan", client, &argon1_i2c_cl_dev_ops);
	if (IS_ERR(data->cl_dev))
		return PTR_ERR(data->cl_dev);

	err = i2c_smbus_write_byte(client, 0);
	if (err)
		return err;

	data->throttle = 0;

	/* If everything works well, attach driver data to the client. */
	i2c_set_clientdata(client, data);

	return SUCCESS;
}

static int argon1_i2c_remove(struct i2c_client *client)
{
	return i2c_smbus_write_byte(client, 0);
}

static void argon1_i2c_shutdown(struct i2c_client *client)
{
	i2c_smbus_write_byte(client, 0);
}

static int argon1_i2c_suspend(struct device *device)
{
	int err;
	struct i2c_client *client;

	client = to_i2c_client(device);
	err = i2c_smbus_write_byte(client, 0);
	if (err)
		return err;

	return SUCCESS;
}

static int argon1_i2c_resume(struct device *device)
{
	int err;
	struct i2c_client *client;
	struct argon1_i2c_data *data;

	client = to_i2c_client(device);
	data = dev_get_drvdata(device);
	err = i2c_smbus_write_byte(client, data->throttle);
	if (err)
		return err;

	return SUCCESS;
}

#ifdef CONFIG_ARGON1_AUTODETECT
static int argon1_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
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

	return SUCCESS;
}
#endif

static ssize_t argon1_i2c_attr_fan1_min_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	return sprintf(buffer, "0\n");
}

static ssize_t argon1_i2c_attr_fan1_max_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	return sprintf(buffer, "100\n");
}

static ssize_t argon1_i2c_attr_fan1_input_show(struct device *device, struct device_attribute *attribute, char *buffer)
{
	struct i2c_client *client;
	struct argon1_i2c_data *data;

	client = dev_get_drvdata(device);
	data = i2c_get_clientdata(client);

	return sprintf(buffer, "%u\n", data->throttle);
}

static ssize_t argon1_i2c_attr_fan1_target_store(struct device *device, struct device_attribute *attribute, const char *buffer, size_t count)
{
	struct i2c_client *client;
	struct argon1_i2c_data *data;
	u8 target;
	int err;

	if (kstrtou8(buffer, 10, &target) || target > 100)
		return -EINVAL;

	client = dev_get_drvdata(device);

	err = i2c_smbus_write_byte_data(client, 0, target);
	if (err < 0)
		return err;

	data = i2c_get_clientdata(client);
	data->throttle = target;

	return SUCCESS;
}

static int argon1_i2c_get_max_state(struct thermal_cooling_device *device, unsigned long *target)
{
	*target = 100;
	return SUCCESS;
}

static int argon1_i2c_get_cur_state(struct thermal_cooling_device *device, unsigned long *target) {
	struct i2c_client *client;
	struct argon1_i2c_data *data;

	client = device->devdata;
	data = i2c_get_clientdata(client);

	*target = data->throttle;

	return SUCCESS;
}

static int argon1_i2c_set_cur_state(struct thermal_cooling_device *device, unsigned long target)
{
	int err;
	struct i2c_client *client;
	struct argon1_i2c_data *data;

	if (target > 100)
		return -EINVAL;

	client = device->devdata;
	err = i2c_smbus_write_byte(client, target);
	if (err < 0)
		return err;

	data = i2c_get_clientdata(client);
	data->throttle = target;

	return SUCCESS;
}

static int argon1_gpio_probe(struct platform_device *device);

static struct platform_driver argon1_gpio_driver = {
	.probe = argon1_gpio_probe,
};

static int argon1_gpio_probe(struct platform_device *device)
{
}

static int __init argon1_init(void)
{
	return i2c_add_driver(&argon1_i2c_driver);
}
module_init(argon1_init);

static void __exit argon1_exit(void)
{
	i2c_del_driver(&argon1_i2c_driver);
}
module_exit(argon1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony Fettes <tonyfettes@tonyfettes.com>");
MODULE_DESCRIPTION("Driver for Argon One case for Raspberry Pi");
