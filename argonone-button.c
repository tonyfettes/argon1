// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/platform_device.h>

#include <linux/delay.h>

#define ARGONONE_BUTTON_DEVICE_NAME "argonone-button"

struct argonone_button_data {
	struct device *dev;
	struct input_dev *in_dev;
	struct gpio_desc *gpiod;
	unsigned int active_low : 1;
	ktime_t start_time;
	int irq;
	unsigned short *keymap;
};

#if 0
static enum hrtimer_restart argonone_button_poll_timer(struct hrtimer *timer)
{
	struct argonone_button_data *data;
	int state;

	data = container_of(timer, struct argonone_button_data, poll_timer);

	state = gpiod_get_value(data->gpiod);
	if (state < 0) {
		dev_err(data->dev, "failed to get gpio state: %d\n", state);
		return HRTIMER_NORESTART;
	}

	if (state == data->active) {
		data->poll_count++;
		return HRTIMER_RESTART;
	} else {
	}
}
#endif

static irqreturn_t argonone_button_isr(int irq, void *dev_id)
{
	struct argonone_button_data *data = dev_id;
	int level;
	ktime_t got_time;
	u64 elapsed_time;
	int index;

	BUG_ON(irq != data->irq);

	got_time = ktime_get();

	pm_stay_awake(data->dev);

	level = gpiod_get_value_cansleep(data->gpiod);
	if (level < 0)
		return level;

	printk(KERN_INFO "argonone-button: level: %d\n", level);

	if (data->active_low)
		level = !level;

	if (level) {
		data->start_time = got_time;
		return IRQ_HANDLED;
	}

	elapsed_time = ktime_to_ms(got_time - data->start_time);

	printk(KERN_INFO "argonone-button: elapsed: %llu.\n", elapsed_time);

	if (elapsed_time >= 10 && elapsed_time <= 30) {
		index = 0;
	} else if (elapsed_time >= 40 && elapsed_time <= 50) {
		index = 1;
	} else {
		index = -1;
	}

	printk(KERN_INFO "argonone-button: issuing key: %d.\n", index);

	if (index != -1) {
		/* Send a press-release key sequence. */
		input_report_key(data->in_dev, data->keymap[index], 1);
		input_sync(data->in_dev);
		input_report_key(data->in_dev, data->keymap[index], 0);
		input_sync(data->in_dev);
	}

	return IRQ_HANDLED;
}

static int argonone_button_probe(struct platform_device *pl_dev)
{
	struct device *dev = &pl_dev->dev;
	struct argonone_button_data *data;
	struct input_dev *in_dev;
	int err;
	int irqflags = 0;
	unsigned int wakeup;

	data = devm_kzalloc(dev, sizeof(struct argonone_button_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "failed to allocate state.\n");
		return -ENOMEM;
	}

	in_dev = devm_input_allocate_device(dev);
	if (!in_dev) {
		dev_err(dev, "failed to allocate input device.\n");
		return -ENOMEM;
	}

	data->in_dev = in_dev;

	in_dev->name = ARGONONE_BUTTON_DEVICE_NAME;
	in_dev->phys = ARGONONE_BUTTON_DEVICE_NAME"/input0";

	in_dev->id.bustype = BUS_HOST;
	in_dev->id.vendor = 0x0001;
	in_dev->id.product = 0x0001;
	in_dev->id.version = 0x0100;

	data->keymap = devm_kcalloc(dev, 2, sizeof(data->keymap[0]), GFP_KERNEL);
	if (!data->keymap)
		return -ENOMEM;

	in_dev->keycodesize = sizeof(data->keymap[0]);
	in_dev->keycode = data->keymap;
	in_dev->keycodemax = 2;

	data->keymap[0] = KEY_RESTART;
	data->keymap[1] = KEY_POWER;
	input_set_capability(in_dev, EV_KEY, data->keymap[0]);
	input_set_capability(in_dev, EV_KEY, data->keymap[1]);

	input_set_drvdata(in_dev, data);

	wakeup = device_property_read_bool(dev, "wakeup-source");

	data->gpiod = devm_gpiod_get(dev, NULL, GPIOD_IN);
	if (IS_ERR(data->gpiod)) {
		dev_err(dev, "failed to read gpio description\n");
		return PTR_ERR(data->gpiod);
	}

	data->active_low = gpiod_is_active_low(data->gpiod);

	data->irq = gpiod_to_irq(data->gpiod);
	if (data->irq < 0) {
		dev_err(dev, "failed to get irq number from gpio description.\n");
		return data->irq;
	}

	irqflags |= IRQF_TRIGGER_RISING;
	irqflags |= IRQF_TRIGGER_FALLING;
	// irqflags |= IRQF_SHARED;

	err = devm_request_any_context_irq(dev, data->irq, argonone_button_isr, irqflags, ARGONONE_BUTTON_DEVICE_NAME, data);
	if (err) {
		dev_err(dev, "failed to request irq.\n");
		return err;
	}

	/* This regisration is also managed, so there is no need to freeing it in the remove callback. */
	err = input_register_device(in_dev);
	if (err) {
		dev_err(dev, "failed to register input device.\n");
		return err;
	}

	platform_set_drvdata(pl_dev, data);

	device_init_wakeup(dev, wakeup);

	return 0;
}

static int argonone_button_suspend(struct device *dev)
{
	struct argonone_button_data *data = dev_get_drvdata(dev);
	int err;

	if (device_may_wakeup(dev)) {
		err = enable_irq_wake(data->irq);
		if (err)
			return err;
	}
	/* This function is a no-op when device is not able to wakeup. */

	return 0;
}

static int argonone_button_resume(struct device *dev)
{
	int err;

	struct argonone_button_data *data = dev_get_drvdata(dev);
	if (device_may_wakeup(dev)) {
		err = disable_irq_wake(data->irq);
		if (err)
			return err;
	}

	return 0;
}

static struct dev_pm_ops argonone_button_pm_ops = {
	.suspend = argonone_button_suspend,
	.resume = argonone_button_resume,
};

static struct of_device_id argonone_button_of_match_table[] = {
	{ .compatible = ARGONONE_BUTTON_DEVICE_NAME },
	{ },
};

static struct platform_driver argonone_button_driver = {
	.driver = {
		.name = ARGONONE_BUTTON_DEVICE_NAME,
		.pm = &argonone_button_pm_ops,
		.of_match_table = argonone_button_of_match_table,
	},
	.probe = argonone_button_probe,
};
module_platform_driver(argonone_button_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony Fettes <tonyfettes@tonyfettes.com>");
MODULE_DESCRIPTION("Driver for power button on Argon ONE case for Raspberry Pi");
