#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>

#define READ_POINT_DELAY 15
#define KEY_NUM 6

struct it7236_priv {
	struct device     *dev;
	struct gpio_desc  *gpio_touch_intr;
	struct i2c_client *client;
	struct task_struct *handler;
	struct input_dev *input_dev;
};

static int it7236_keycode[KEY_NUM] = {
	KEY_1,  KEY_2,  KEY_3,  KEY_4, KEY_5,  KEY_6
};

static int it7236_i2c_read(struct i2c_client *client, unsigned char reg,
				unsigned char data[], unsigned short len)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data
		}
	};

	memset(data, 0xFF, len);
	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 1)
		dev_err(&client->dev, "%s client transfer failed, reg:%#x\n", __func__, reg);

	return ret;
}

static int it7236_i2c_write(struct i2c_client *client, unsigned char reg,
				unsigned char const data[], unsigned short len)
{
	unsigned char buf[256];
	int ret = 0, retry = 3;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = buf;

	if(len < 256) {
		buf[0] = reg;
		memcpy(&(buf[1]), data, len);

		do {
			ret = i2c_transfer(client->adapter, &msg, 1);
			retry--;
		} while((ret != 1) && (retry > 0));

		if(ret != 1)
			dev_err(&client->dev, "%s : i2c_transfer error\n", __func__);
	} else {
		dev_err(&client->dev, "%s : i2c_transfer error , out of size\n", __func__);
		return -1;
	}

	return ret;
}

static int it7236_write_command(struct i2c_client *client,
				unsigned char const data[], unsigned short len)
{
	char buf[256] = {0};
	char val = 0;
	int retry = 0, ret = 0;

	if (len > 255)
		return -1;

	buf[0] = 0x55;
	it7236_i2c_write(client, 0xFB, buf, 1);
	mdelay(1);

	buf[0] = 0x0;
	it7236_i2c_write(client, 0xF0, buf, 1);

	it7236_i2c_read(client, 0xF1, buf, 1);
	buf[0] = (buf[0] | (1 << 7));
	while(retry < 10) {
		it7236_i2c_write(client, 0xF1, buf, 1);

		it7236_i2c_read(client, 0xFA, &val, 1);
		if (val & 0x1)
			break;
		retry++;
	}

	buf[0] = 0x01; //command
	memcpy(&(buf[1]), data, len);
	ret = it7236_i2c_write(client, 0x40, buf, (len + 1));

	it7236_i2c_read(client, 0xF1, buf, 1);
	buf[0] = (buf[0] | (1 << 6));
	it7236_i2c_write(client, 0xF1, buf, 1);

	return 0;
}

static bool it7236_wait_read_response(struct i2c_client *client)
{
	unsigned char val = 0x00;
	unsigned int retry = 0;

	while (retry < 10) {
		it7236_i2c_read(client, 0xF3, &val, 1);
		if ((val >> 6) & 0x1)
			return true;
		retry++;
	}

	return  false;
}

static int it7236_read_response(struct i2c_client *client,
				unsigned char data[], unsigned short len)
{
	char buf[256] = {0};
	u8 val;
	int retry = 0, ret = 0;

	if (!it7236_wait_read_response(client)) {
		dev_err(&client->dev, "it7236_wait_read_response error\n");
		return -1;
	}

	buf[0] = 0x0;
	it7236_i2c_write(client, 0xF0, buf, 1);

	it7236_i2c_read(client, 0xF1, buf, 1);
	buf[0] = (buf[0] | (1 << 7));
	while(retry < 10) {
		it7236_i2c_write(client, 0xF1, buf, 1);

		it7236_i2c_read(client, 0xFA, &val, 1);
		if (val & 0x1)
			break;
		retry++;
	}

	ret = it7236_i2c_read(client, 0x40, data, len);

	it7236_i2c_read(client, 0xF3, buf, 1);
	buf[0] = (buf[0] & (~(1 << 6)));
	it7236_i2c_write(client, 0xF3, buf, 1);

	it7236_i2c_read(client, 0xF1, buf, 1);
	buf[0] = (buf[0] | (1 << 6));
	it7236_i2c_write(client, 0xF1, buf, 1);

	return ret;
}

static int it7236_check(struct i2c_client *client)
{
	char buf[23] = {0};

	buf[0] = 0x01; //get device info
	it7236_write_command(client, buf, 1);
	it7236_read_response(client, buf, 23);

	if (buf[0] != 0x0 || buf[0] != 0x0) {
		dev_err(&client->dev, "get device info error\n");
		return -1;
	}

	if (buf[2] != 'I' || buf[3] != 'T' ||
			buf[4] != '7' || buf[5] != '2' ||
			buf[6] != '3' || buf[7] != '6') {
		dev_err(&client->dev, "get device name error\n");
		return -1;
	}

	return 0;
}

static int it7236_read_point_thread(void *data)
{
	struct it7236_priv *it7236 = (struct it7236_priv*)data;
	int gpio_val = 0, i = 0;
	u8 key_val = 0, old_key_val = 0;
	u8 key_status = 0, old_key_status = 0;

	while (!kthread_should_stop()) {
		msleep(READ_POINT_DELAY);

		gpio_val = gpiod_get_value(it7236->gpio_touch_intr);
		it7236_i2c_read(it7236->client, 0xFF, &key_val, 1);
		if ((!gpio_val) && (!key_val))
			continue;

		if (key_val == old_key_val)
			continue;

		for (i = 0; i < KEY_NUM; i++) {
			key_status = (key_val >> i) & 0x1;
			old_key_status = (old_key_val >> i) & 0x1;
			if (key_status != old_key_status) {
				input_report_key(it7236->input_dev, it7236_keycode[i], key_status);
				input_sync(it7236->input_dev);
			}
		}

		old_key_val = key_val;
	}

	return 0;
}

static int it7236_dt_parse(struct i2c_client *client)
{
	struct it7236_priv *it7236 = i2c_get_clientdata(client);

	it7236->gpio_touch_intr = devm_gpiod_get(&client->dev, "touch_intr", GPIOD_IN);
	if (IS_ERR(it7236->gpio_touch_intr)) {
		dev_err(&client->dev, "unable to get gpio_touch_intr\n");
		it7236->gpio_touch_intr = NULL;
		return -1;
	}

	return 0;
}

static int it7236_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct it7236_priv *it7236 = NULL;
	int i = 0;

	it7236 = devm_kzalloc(&client->dev, sizeof(struct it7236_priv), GFP_KERNEL);
	if (!it7236)
		return -ENOMEM;

	i2c_set_clientdata(client, it7236);

	if (it7236_dt_parse(client) < 0) {
		dev_err(&client->dev, "it7236_dt_parse error\n");
		goto err_dt_parse;
	}

	it7236->client = client;
	it7236->dev    = &client->dev;

	if (it7236_check(client) < 0) {
		dev_err(&client->dev, "check it7236 error\n");
		goto err_it7236_check;
	}

	it7236->input_dev = input_allocate_device();
	if (!it7236->input_dev) {
		dev_err(&client->dev, "alloc input_dev error\n");
		goto err_alloc_input;
	}
	it7236->input_dev->name = "it7236 touchkey";
	it7236->input_dev->id.bustype = BUS_I2C;
	it7236->input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	it7236->input_dev->keycode = it7236_keycode;
	it7236->input_dev->keycodesize = sizeof(it7236_keycode[0]);
	it7236->input_dev->keycodemax = ARRAY_SIZE(it7236_keycode);

	for (i = 0; i < KEY_NUM; i++)
		input_set_capability(it7236->input_dev, EV_KEY, it7236_keycode[i]);

	if (input_register_device(it7236->input_dev) < 0) {
		dev_err(&client->dev, "register input_dev error\n");
		goto err_register_input;
	}

	it7236->handler = kthread_run(it7236_read_point_thread, (void*)it7236,
					"read_point_kthread");
	if (IS_ERR(it7236->handler)) {
		dev_err(&client->dev, "creat kthread error\n");
		goto err_creat_kthread;
	}

	return 0;
err_register_input:
	input_free_device(it7236->input_dev);
err_alloc_input:
err_creat_kthread:
err_it7236_check:
	devm_gpiod_put(&client->dev, it7236->gpio_touch_intr);
err_dt_parse:
	kfree(it7236);
	return -1;
}

static int it7236_i2c_remove(struct i2c_client *client)
{
	struct it7236_priv *it7236 = i2c_get_clientdata(client);

	kthread_stop(it7236->handler);
	input_unregister_device(it7236->input_dev);
	input_free_device(it7236->input_dev);

	return 0;
}

static const struct i2c_device_id it7236_i2c_ids[] = {
	{ "it7236", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, it7236_i2c_ids);

static struct i2c_driver it7236_i2c_driver = {
	.probe		= it7236_i2c_probe,
	.remove		= it7236_i2c_remove,
	.driver = {
		.name	= "it7236",
	},
	.id_table	= it7236_i2c_ids,
};

module_i2c_driver(it7236_i2c_driver);
MODULE_LICENSE("GPL v2");
