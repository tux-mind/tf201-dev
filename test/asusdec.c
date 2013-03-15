/*
 * ASUS Dock EC driver.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/cdev.h>
#include <linux/gpio_event.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <asm/gpio.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/power_supply.h>
#include <../gpio-names.h>
#include <mach/board-cardhu-misc.h>

#include "asusdec.h"
#include "elan_i2c_asus.h"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

/*
 * functions declaration
 */
static int asusdec_i2c_write_data(struct i2c_client *client, u16 data);
static int asusdec_i2c_read_data(struct i2c_client *client);
static void asusdec_reset_dock(void);
static int asusdec_is_init_running(void);
static int asusdec_chip_init(struct i2c_client *client);
static void asusdec_dock_status_report(void);
static void asusdec_lid_report_function(struct work_struct *dat);
static void asusdec_work_function(struct work_struct *dat);
static void asusAudiodec_work_function(struct work_struct *dat);
static void asusdec_dock_init_work_function(struct work_struct *dat);
static void asusdec_fw_update_work_function(struct work_struct *dat);
static int __devinit asusdec_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int __devexit asusdec_remove(struct i2c_client *client);
static ssize_t asusdec_show_dock(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_tp_status_show(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_tp_enable_show(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_info_show(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_store_led(struct device *class,
		struct device_attribute *attr,const char *buf, size_t count);
static ssize_t asusdec_charging_led_store(struct device *class,
		struct device_attribute *attr,const char *buf, size_t count);
static ssize_t asusdec_led_show(struct device *class,
	struct device_attribute *attr,char *buf);
static ssize_t asusdec_store_ec_wakeup(struct device *class,
		struct device_attribute *attr,const char *buf, size_t count);
static ssize_t asusdec_show_drain(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show_dock_battery(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show_dock_battery_status(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show_dock_battery_all(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show_dock_control_flag(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusdec_show_lid_status(struct device *class,
		struct device_attribute *attr,char *buf);
static ssize_t asusAudiodec_info_show(struct device *class,
		struct device_attribute *attr,char *buf);
static int asusdec_keypad_get_response(struct i2c_client *client, int res);
static int asusdec_keypad_enable(struct i2c_client *client);
static int asusdec_touchpad_get_response(struct i2c_client *client, int res);
static int asusdec_touchpad_enable(struct i2c_client *client);
static int asusdec_touchpad_disable(struct i2c_client *client);
//static int asusdec_touchpad_reset(struct i2c_client *client);
static int asusdec_suspend(struct i2c_client *client, pm_message_t mesg);
static int asusdec_resume(struct i2c_client *client);
static int asusdec_open(struct inode *inode, struct file *flip);
static int asusdec_release(struct inode *inode, struct file *flip);
static long asusdec_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);
static void asusdec_enter_factory_mode(void);
static ssize_t ec_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t ec_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
static void BuffPush(char data);
static int asusdec_input_device_create(struct i2c_client *client);
static int asusdec_lid_input_device_create(struct i2c_client *client);
static ssize_t asusdec_switch_name(struct switch_dev *sdev, char *buf);
static ssize_t asusdec_switch_state(struct switch_dev *sdev, char *buf);
static int asusdec_event(struct input_dev *dev, unsigned int type, unsigned int code, int value);
static int asusdec_dock_battery_get_capacity(union power_supply_propval *val);
static int asusdec_dock_battery_get_status(union power_supply_propval *val);
static int asusdec_dock_battery_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val);
int asusAudiodec_cable_type_callback(void);
/*-----------------------------------------*/
#if DOCK_SPEAKER
extern int audio_dock_event(int status);
#endif
#if BATTERY_DRIVER
extern int docking_callback(int status);
extern void battery_callback(unsigned usb_cable_state);
#endif
#if DOCK_USB
extern void fsl_dock_ec_callback(void);
extern void tegra_usb3_smi_backlight_on_callback(void);
#endif
#if AUDIO_DOCK_STAND
extern int audio_dock_in_out(u8 docktype);
#endif
/*
* extern variable
*/
extern unsigned int factory_mode;

/*
 * global variable
 */
bool isDockIn = 0;
char* switch_value[]={"0", "10", "11", "12"}; //0: no dock, 1:mobile dock, 2:audio dock, 3: audio stand

EXPORT_SYMBOL(isDockIn);

static unsigned int asusdec_apwake_gpio = TEGRA_GPIO_PS7;
static unsigned int asusdec_ecreq_gpio = TEGRA_GPIO_PQ6;
static unsigned int asusdec_dock_in_gpio = TEGRA_GPIO_PU4;
static unsigned int asusdec_hall_sensor_gpio = TEGRA_GPIO_PS6;

static char host_to_ec_buffer[EC_BUFF_LEN];
static char ec_to_host_buffer[EC_BUFF_LEN];
static int h2ec_count;
static int buff_in_ptr;	  // point to the next free place
static int buff_out_ptr;	  // points to the first data

static struct i2c_client dockram_client;
static struct class *asusdec_class;
static struct device *asusdec_device ;
static struct asusdec_chip *ec_chip;

struct cdev *asusdec_cdev ;
static dev_t asusdec_dev ;
static int asusdec_major = 0 ;
static int asusdec_minor = 0 ;

static struct workqueue_struct *asusdec_wq;
struct delayed_work asusdec_stress_work;

static const struct i2c_device_id asusdec_id[] = {
	{"asusdec", 0},
	{}
};

static enum power_supply_property asusdec_dock_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
};

static struct power_supply asusdec_power_supply[] = {
	{
		.name		= "dock_battery",
		.type		= POWER_SUPPLY_TYPE_DOCK_BATTERY,
		.properties	= asusdec_dock_properties,
		.num_properties	= ARRAY_SIZE(asusdec_dock_properties),
		.get_property	= asusdec_dock_battery_get_property,
	},
};

MODULE_DEVICE_TABLE(i2c, asusdec_id);

struct file_operations asusdec_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = asusdec_ioctl,
	.open = asusdec_open,
	.write = ec_write,
	.read = ec_read,
	.release = asusdec_release,
};

static struct i2c_driver asusdec_driver = {
	.class	= I2C_CLASS_HWMON,
	.driver	 = {
		.name = "asusdec",
		.owner = THIS_MODULE,
	},
	.probe	 = asusdec_probe,
	.remove	 = __devexit_p(asusdec_remove),
	.suspend = asusdec_suspend,
	.resume = asusdec_resume,
	.id_table = asusdec_id,
};


static DEVICE_ATTR(ec_status, S_IWUSR | S_IRUGO, asusdec_show,NULL);
static DEVICE_ATTR(ec_tp_status, S_IWUSR | S_IRUGO, asusdec_tp_status_show,NULL);
static DEVICE_ATTR(ec_tp_enable, S_IWUSR | S_IRUGO, asusdec_tp_enable_show,NULL);
static DEVICE_ATTR(ec_info, S_IWUSR | S_IRUGO, asusdec_info_show,NULL);
static DEVICE_ATTR(ec_dock, S_IWUSR | S_IRUGO, asusdec_show_dock,NULL);
static DEVICE_ATTR(ec_dock_led, S_IWUSR | S_IRUGO, asusdec_led_show,asusdec_store_led);
static DEVICE_ATTR(ec_charging_led, S_IWUSR | S_IRUGO, NULL, asusdec_charging_led_store);
static DEVICE_ATTR(ec_wakeup, S_IWUSR | S_IRUGO, NULL,asusdec_store_ec_wakeup);
static DEVICE_ATTR(ec_dock_discharge, S_IWUSR | S_IRUGO, asusdec_show_drain,NULL);
static DEVICE_ATTR(ec_dock_battery, S_IWUSR | S_IRUGO, asusdec_show_dock_battery,NULL);
static DEVICE_ATTR(ec_dock_battery_status, S_IWUSR | S_IRUGO, asusdec_show_dock_battery_status,NULL);
static DEVICE_ATTR(ec_dock_battery_all, S_IWUSR | S_IRUGO, asusdec_show_dock_battery_all,NULL);
static DEVICE_ATTR(ec_dock_control_flag, S_IWUSR | S_IRUGO, asusdec_show_dock_control_flag,NULL);
static DEVICE_ATTR(ec_lid, S_IWUSR | S_IRUGO, asusdec_show_lid_status,NULL);
static DEVICE_ATTR(ec_audio_dock_mcu_info, S_IWUSR | S_IRUGO, asusAudiodec_info_show,NULL);

static struct attribute *asusdec_smbus_attributes[] = {
	&dev_attr_ec_status.attr,
	&dev_attr_ec_tp_status.attr,
	&dev_attr_ec_tp_enable.attr,
	&dev_attr_ec_info.attr,
	&dev_attr_ec_dock.attr,
	&dev_attr_ec_dock_led.attr,
	&dev_attr_ec_charging_led.attr,
	&dev_attr_ec_wakeup.attr,
	&dev_attr_ec_dock_discharge.attr,
	&dev_attr_ec_dock_battery.attr,
	&dev_attr_ec_dock_battery_status.attr,
	&dev_attr_ec_dock_battery_all.attr,
	&dev_attr_ec_dock_control_flag.attr,
	&dev_attr_ec_lid.attr,
	&dev_attr_ec_audio_dock_mcu_info.attr,
NULL
};


static const struct attribute_group asusdec_smbus_group = {
	.attrs = asusdec_smbus_attributes,
};

/*
 * functions definition
 */
static void asusdec_dockram_init(struct i2c_client *client){
	dockram_client.adapter = client->adapter;
	dockram_client.addr = 0x1b;
	dockram_client.detected = client->detected;
	dockram_client.dev = client->dev;
	dockram_client.driver = client->driver;
	dockram_client.flags = client->flags;
	strcpy(dockram_client.name,client->name);
}

static int asusdec_dockram_write_data(int cmd, int length)
{
	int ret = 0;

	if (ec_chip->dock_in == 0){
		return -1;
	}

	ret = i2c_smbus_write_i2c_block_data(&dockram_client, cmd, length, ec_chip->i2c_dm_data);
	if (ret < 0) {
		ASUSDEC_ERR("Fail to read dockram data, status %d\n", ret);
	}
	return ret;
}

static int asusdec_dockram_read_data(int cmd)
{
	int ret = 0;

	if (ec_chip->dock_in == 0){
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(&dockram_client, cmd, 32, ec_chip->i2c_dm_data);
	if (ret < 0) {
		ASUSDEC_ERR("Fail to read dockram data, status %d\n", ret);
	}
	return ret;
}

static int asusdec_i2c_write_data(struct i2c_client *client, u16 data)
{
	int ret = 0;

	if (ec_chip->dock_in == 0){
		return -1;
	}

	ret = i2c_smbus_write_word_data(client, 0x64, data);
	if (ret < 0) {
		ASUSDEC_ERR("Fail to write data, status %d\n", ret);
	}
	return ret;
}

static int asusdec_i2c_read_data(struct i2c_client *client)
{
	int ret = 0;

	if (ec_chip->dock_in == 0){
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, 0x6A, 8, ec_chip->i2c_data);
	if (ret < 0) {
		ASUSDEC_ERR("Fail to read data, status %d\n", ret);
	}
	return ret;
}

static int asusdec_keypad_get_response(struct i2c_client *client, int res)
{
	int retry = ASUSDEC_RETRY_COUNT;

	while(retry-- > 0){
		asusdec_i2c_read_data(client);
		ASUSDEC_I2C_DATA(ec_chip->i2c_data, ec_chip->index);
		if ((ec_chip->i2c_data[1] & ASUSDEC_OBF_MASK) &&
			(!(ec_chip->i2c_data[1] & ASUSDEC_AUX_MASK))){
			if (ec_chip->i2c_data[2]  == res){
				goto get_asusdec_keypad_i2c;
			}
		}
		msleep(CONVERSION_TIME_MS/5);
	}
	return -1;

get_asusdec_keypad_i2c:
	return 0;

}

static int asusdec_keypad_enable(struct i2c_client *client)
{
	int retry = ASUSDEC_RETRY_COUNT;

	while(retry-- > 0){
		asusdec_i2c_write_data(client, 0xF400);
		if(!asusdec_keypad_get_response(client, ASUSDEC_PS2_ACK)){
			goto keypad_enable_ok;
		}
	}
	ASUSDEC_ERR("fail to enable keypad");
	return -1;

keypad_enable_ok:
	return 0;
}

static int asusdec_keypad_disable(struct i2c_client *client)
{
	int retry = ASUSDEC_RETRY_COUNT;

	while(retry-- > 0){
		asusdec_i2c_write_data(client, 0xF500);
		if(!asusdec_keypad_get_response(client, ASUSDEC_PS2_ACK)){
			goto keypad_disable_ok;
		}
	}

	ASUSDEC_ERR("fail to disable keypad");
	return -1;

keypad_disable_ok:
	return 0;
}

static void asusdec_keypad_led_on(struct work_struct *dat)
{
	ec_chip->kbc_value = 1;
	ASUSDEC_INFO("send led cmd 1\n");
	msleep(250);
	asusdec_i2c_write_data(ec_chip->client, 0xED00);
}


static void asusdec_keypad_led_off(struct work_struct *dat)
{
	ec_chip->kbc_value = 0;
	ASUSDEC_INFO("send led cmd 1\n");
	msleep(250);
	asusdec_i2c_write_data(ec_chip->client, 0xED00);
}


static int asusdec_touchpad_get_response(struct i2c_client *client, int res)
{
	int retry = ASUSDEC_RETRY_COUNT;

	msleep(CONVERSION_TIME_MS);
	while(retry-- > 0){
		asusdec_i2c_read_data(client);
		ASUSDEC_I2C_DATA(ec_chip->i2c_data, ec_chip->index);
		if ((ec_chip->i2c_data[1] & ASUSDEC_OBF_MASK) &&
			(ec_chip->i2c_data[1] & ASUSDEC_AUX_MASK)){
			if (ec_chip->i2c_data[2] == res){
				goto get_asusdec_touchpad_i2c;
			}
		}
		msleep(CONVERSION_TIME_MS/5);
	}

	ASUSDEC_ERR("fail to get touchpad response");
	return -1;

get_asusdec_touchpad_i2c:
	return 0;

}

static int asusdec_touchpad_enable(struct i2c_client *client)
{
	ec_chip->tp_wait_ack = 1;
	asusdec_i2c_write_data(client, 0xF4D4);
	return 0;
}

static int asusdec_touchpad_disable(struct i2c_client *client)
{
	int retry = 5;

	while(retry-- > 0){
		asusdec_i2c_write_data(client, 0xF5D4);
		if(!asusdec_touchpad_get_response(client, ASUSDEC_PS2_ACK)){
			goto touchpad_disable_ok;
		}
	}

	ASUSDEC_ERR("fail to disable touchpad");
	return -1;

touchpad_disable_ok:
	return 0;
}

static void asusdec_fw_clear_buf(void){
	int i;

	for (i = 0; i < 64; i++){
		i2c_smbus_read_byte_data(&dockram_client, 0);
	}
}

static void asusdec_fw_reset_ec_op(void){
	char i2c_data[32];
	int i;
	//int r_data[32];

	asusdec_fw_clear_buf();

	i2c_data[0] = 0x01;
	i2c_data[1] = 0x21;
	for (i = 0; i < i2c_data[0]+1 ; i++){
		i2c_smbus_write_byte_data(&dockram_client, i2c_data[i],0);
	}
	msleep(CONVERSION_TIME_MS*4);
}

static void asusdec_fw_address_set_op(void){
	char i2c_data[32];
	int i;
	//int r_data[32];

	asusdec_fw_clear_buf();

	i2c_data[0] = 0x05;
	i2c_data[1] = 0xa0;
	i2c_data[2] = 0x00;
	i2c_data[3] = 0x00;
	i2c_data[4] = 0x02;
	i2c_data[5] = 0x00;
	for (i = 0; i < i2c_data[0]+1 ; i++){
		i2c_smbus_write_byte_data(&dockram_client, i2c_data[i],0);
	}
	msleep(CONVERSION_TIME_MS*4);
}

static void asusdec_fw_enter_op(void){
	char i2c_data[32];
	int i;
	//int r_data[32];

	asusdec_fw_clear_buf();

	i2c_data[0] = 0x05;
	i2c_data[1] = 0x10;
	i2c_data[2] = 0x55;
	i2c_data[3] = 0xaa;
	i2c_data[4] = 0xcd;
	i2c_data[5] = 0xbe;
	for (i = 0; i < i2c_data[0]+1 ; i++){
		i2c_smbus_write_byte_data(&dockram_client, i2c_data[i],0);
	}
	msleep(CONVERSION_TIME_MS*4);
}

static int asusdec_fw_cmp_id(void){
	char i2c_data[32];
	int i;
	int r_data[32];
	int ret_val = 0;

	asusdec_fw_clear_buf();

	i2c_data[0] = 0x01;
	i2c_data[1] = 0xC0;
	for (i = 0; i < i2c_data[0]+1 ; i++){
		i2c_smbus_write_byte_data(&dockram_client, i2c_data[i],0);
	}
	msleep(CONVERSION_TIME_MS*10);

	for (i = 0; i < 5; i++){
		r_data[i] = i2c_smbus_read_byte_data(&dockram_client, 0);
	}

	for (i = 0; i < 5; i++){
		ASUSDEC_NOTICE("r_data[%d] = 0x%x\n", i, r_data[i]);
	}

	if (r_data[0] == 0xfa &&
		r_data[1] == 0xf0 &&
		r_data[2] == 0x12 &&
		r_data[3] == 0xef &&
		r_data[4] == 0x12){
		ret_val = 0;
	} else {
		ret_val = 1;
	}

	return ret_val;
}

static void asusdec_fw_reset(void){

	if (asusdec_fw_cmp_id() == 0){
		asusdec_fw_enter_op();
		asusdec_fw_address_set_op();
		asusdec_fw_reset_ec_op();
		asusdec_fw_clear_buf();
		if (ec_chip->re_init == 0){
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_dock_init_work, HZ/2);
			ec_chip->re_init = 1;
		}
	}
}
static int asusdec_i2c_test(struct i2c_client *client){
	return asusdec_i2c_write_data(client, 0x0000);
}

static void asusdec_reset_dock(void){
	ec_chip->dock_init = 0;
	ASUSDEC_NOTICE("send EC_Request\n");
	gpio_set_value(asusdec_ecreq_gpio, 0);
	msleep(20);
	gpio_set_value(asusdec_ecreq_gpio, 1);
}
static int asusdec_is_init_running(void){
	int ret_val;

	mutex_lock(&ec_chip->dock_init_lock);
	ret_val = ec_chip->dock_init;
	ec_chip->dock_init = 1;
	mutex_unlock(&ec_chip->dock_init_lock);
	return ret_val;
}

static void asusdec_clear_i2c_buffer(struct i2c_client *client){
	int i;
	for ( i=0; i<8; i++){
		asusdec_i2c_read_data(client);
	}
}
static int asusdec_chip_init(struct i2c_client *client)
{
	int ret_val = 0;
	int i;

	if(asusdec_is_init_running()){
		return 0;
	}

	wake_lock(&ec_chip->wake_lock);
	memset(ec_chip->ec_model_name, 0, 32);
	memset(ec_chip->ec_version, 0, 32);
	disable_irq_nosync(gpio_to_irq(asusdec_apwake_gpio));

	for ( i = 0; i < 3; i++){
		ret_val = asusdec_i2c_test(client);
		if (ret_val < 0)
			msleep(1000);
		else
			break;
	}
	if(ret_val < 0){
		goto fail_to_access_ec;
	}

	for ( i=0; i<8; i++){
		asusdec_i2c_read_data(client);
	}

	if (asusdec_dockram_read_data(0x01) < 0){
		goto fail_to_access_ec;
	}
	strcpy(ec_chip->ec_model_name, &ec_chip->i2c_dm_data[1]);
	ASUSDEC_NOTICE("Model Name: %s\n", ec_chip->ec_model_name);

	if (asusdec_dockram_read_data(0x02) < 0){
		goto fail_to_access_ec;
	}
	strcpy(ec_chip->ec_version, &ec_chip->i2c_dm_data[1]);
	ASUSDEC_NOTICE("EC-FW Version: %s\n", ec_chip->ec_version);

	if (asusdec_dockram_read_data(0x03) < 0){
		goto fail_to_access_ec;
	}
	ASUSDEC_INFO("EC-Config Format: %s\n", &ec_chip->i2c_dm_data[1]);

	if (asusdec_dockram_read_data(0x04) < 0){
		goto fail_to_access_ec;
	}
	strcpy(ec_chip->dock_pid, &ec_chip->i2c_dm_data[1]);
	ASUSDEC_INFO("PID Version: %s\n", ec_chip->dock_pid);

	if (asusdec_dockram_read_data(0x0A) < 0){
		goto fail_to_access_ec;
	}
	ec_chip->dock_behavior = ec_chip->i2c_dm_data[2] & 0x02;
	ASUSDEC_NOTICE("EC-FW Behavior: %s\n", ec_chip->dock_behavior ?
		"susb on when receive ec_req" : "susb on when system wakeup");

	ec_chip->tf_dock = 1;

	if(factory_mode == 2)
		asusdec_enter_factory_mode();

	if(asusdec_input_device_create(client)){
		goto fail_to_access_ec;
	}

	if (201){
		if (ec_chip->init_success == 0){
			msleep(750);
		}
		asusdec_clear_i2c_buffer(client);
		asusdec_touchpad_disable(client);
	}

	asusdec_keypad_disable(client);

#if TOUCHPAD_ELAN
#if TOUCHPAD_MODE
	if (1){
		asusdec_clear_i2c_buffer(client);
		if ((!elantech_detect(ec_chip)) && (!elantech_init(ec_chip))){
		    ec_chip->touchpad_member = ELANTOUCHPAD;
		} else {
			ec_chip->touchpad_member = -1;
		}
	}
#endif
#endif

	ASUSDEC_NOTICE("touchpad and keyboard init\n");
	ec_chip->d_index = 0;

	asusdec_keypad_enable(client);
	asusdec_clear_i2c_buffer(client);

	enable_irq(gpio_to_irq(asusdec_apwake_gpio));
	ec_chip->init_success = 1;

	if ((201) && ec_chip->tp_enable){
		asusdec_touchpad_enable(client);
	}

	ec_chip->status = 1;
	asusdec_dock_status_report();
	wake_unlock(&ec_chip->wake_lock);
	return 0;

fail_to_access_ec:
	if (asusdec_dockram_read_data(0x00) < 0){
		ASUSDEC_NOTICE("No EC detected\n");
		ec_chip->dock_in = 0;
	} else {
		ASUSDEC_NOTICE("Need EC FW update\n");
		asusdec_fw_reset();
	}
	enable_irq(gpio_to_irq(asusdec_apwake_gpio));
	wake_unlock(&ec_chip->wake_lock);
	return -1;

}


static irqreturn_t asusdec_interrupt_handler(int irq, void *dev_id){

	int gpio = irq_to_gpio(irq);

	if (gpio == asusdec_apwake_gpio){
		disable_irq_nosync(irq);
		if (ec_chip->op_mode){
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_fw_update_work, 0);
		}
		else if ((ec_chip->dock_type == AUDIO_DOCK)||(ec_chip->dock_type == AUDIO_STAND)){
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_audio_work, 0);
		}
		else{
			if (ec_chip->suspend_state){
				ec_chip->wakeup_lcd = 1;
				ec_chip->ap_wake_wakeup = 1;
			}
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_work, 0);
		}
	}
	else if (gpio == asusdec_dock_in_gpio){
		ec_chip->dock_in = 0;
		ec_chip->dock_det++;
		queue_delayed_work(asusdec_wq, &ec_chip->asusdec_dock_init_work, 0);
	} else if (gpio == asusdec_hall_sensor_gpio){
		queue_delayed_work(asusdec_wq, &ec_chip->asusdec_hall_sensor_work, 0);
	}
	return IRQ_HANDLED;
}

static int asusdec_irq_hall_sensor(struct i2c_client *client)
{
	int rc = 0 ;
	unsigned gpio = asusdec_hall_sensor_gpio;
	unsigned irq = gpio_to_irq(asusdec_hall_sensor_gpio);
	const char* label = "asusdec_hall_sensor" ;
	unsigned int pad_pid = tegra3_get_project_id();

	ASUSDEC_INFO("gpio = %d, irq = %d\n", gpio, irq);
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	tegra_gpio_enable(gpio);
	rc = gpio_request(gpio, label);
	if (rc) {
		ASUSDEC_ERR("gpio_request failed for input %d\n", gpio);
	}

	rc = gpio_direction_input(gpio) ;
	if (rc) {
		ASUSDEC_ERR("gpio_direction_input failed for input %d\n", gpio);
		goto err_gpio_direction_input_failed;
	}
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	rc = request_irq(irq, asusdec_interrupt_handler,IRQF_SHARED|IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING/*|IRQF_TRIGGER_HIGH|IRQF_TRIGGER_LOW*/, label, client);
	if (rc < 0) {
		ASUSDEC_ERR("Could not register for %s interrupt, irq = %d, rc = %d\n", label, irq, rc);
		rc = -EIO;
		goto err_gpio_request_irq_fail ;
	}

	if ((pad_pid == TEGRA3_PROJECT_TF300T) || (pad_pid == TEGRA3_PROJECT_TF300TG)){
		ASUSDEC_NOTICE("Disable hall sensor wakeup function due to pid = %u\n", pad_pid);
	} else {
		enable_irq_wake(irq);
	}

	ASUSDEC_INFO("LID irq = %d, rc = %d\n", irq, rc);

	if (gpio_get_value(gpio)){
		ASUSDEC_NOTICE("LID open\n");
	} else{
		ASUSDEC_NOTICE("LID close\n");
	}

	return 0 ;

err_gpio_request_irq_fail :
	gpio_free(gpio);
err_gpio_direction_input_failed:
	return rc;
}


static int asusdec_irq_dock_in(struct i2c_client *client)
{
	int rc = 0 ;
	unsigned gpio = asusdec_dock_in_gpio;
	unsigned irq = gpio_to_irq(asusdec_dock_in_gpio);
	const char* label = "asusdec_dock_in" ;

	ASUSDEC_INFO("gpio = %d, irq = %d\n", gpio, irq);
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	tegra_gpio_enable(gpio);
	rc = gpio_request(gpio, label);
	if (rc) {
		ASUSDEC_ERR("gpio_request failed for input %d\n", gpio);
	}

	rc = gpio_direction_input(gpio) ;
	if (rc) {
		ASUSDEC_ERR("gpio_direction_input failed for input %d\n", gpio);
		goto err_gpio_direction_input_failed;
	}
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	rc = request_irq(irq, asusdec_interrupt_handler,IRQF_SHARED|IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING/*|IRQF_TRIGGER_HIGH|IRQF_TRIGGER_LOW*/, label, client);
	if (rc < 0) {
		ASUSDEC_ERR("Could not register for %s interrupt, irq = %d, rc = %d\n", label, irq, rc);
		rc = -EIO;
		goto err_gpio_request_irq_fail ;
	}
	ASUSDEC_INFO("request irq = %d, rc = %d\n", irq, rc);

	return 0 ;

err_gpio_request_irq_fail :
	gpio_free(gpio);
err_gpio_direction_input_failed:
	return rc;
}

static int asusdec_irq(struct i2c_client *client)
{
	int rc = 0 ;
	unsigned gpio = asusdec_apwake_gpio;
	unsigned irq = gpio_to_irq(asusdec_apwake_gpio);
	const char* label = "asusdec_input" ;

	ASUSDEC_INFO("gpio = %d, irq = %d\n", gpio, irq);
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	tegra_gpio_enable(gpio);
	rc = gpio_request(gpio, label);
	if (rc) {
		ASUSDEC_ERR("gpio_request failed for input %d\n", gpio);
		goto err_request_input_gpio_failed;
	}

	rc = gpio_direction_input(gpio) ;
	if (rc) {
		ASUSDEC_ERR("gpio_direction_input failed for input %d\n", gpio);
		goto err_gpio_direction_input_failed;
	}
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	rc = request_irq(irq, asusdec_interrupt_handler,/*IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING|IRQF_TRIGGER_HIGH|*/IRQF_TRIGGER_LOW, label, client);
	if (rc < 0) {
		ASUSDEC_ERR("Could not register for %s interrupt, irq = %d, rc = %d\n", label, irq, rc);
		rc = -EIO;
		goto err_gpio_request_irq_fail ;
	}
	enable_irq_wake(irq);
	ASUSDEC_INFO("request irq = %d, rc = %d\n", irq, rc);

	return 0 ;

err_gpio_request_irq_fail :
	gpio_free(gpio);
err_gpio_direction_input_failed:
err_request_input_gpio_failed :
	return rc;
}

static int asusdec_irq_ec_request(struct i2c_client *client)
{
	int rc = 0 ;
	unsigned gpio = asusdec_ecreq_gpio;
#if ASUSDEC_DEBUG
	unsigned irq = gpio_to_irq(asusdec_apwake_gpio);
#endif
	const char* label = "asusdec_request" ;

	ASUSDEC_INFO("gpio = %d, irq = %d\n", gpio, irq);
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	tegra_gpio_enable(gpio);
	rc = gpio_request(gpio, label);
	if (rc) {
		ASUSDEC_ERR("gpio_request failed for input %d\n", gpio);
		goto err_exit;
	}

	rc = gpio_direction_output(gpio, 1) ;
	if (rc) {
		ASUSDEC_ERR("gpio_direction_output failed for input %d\n", gpio);
		goto err_exit;
	}
	ASUSDEC_INFO("GPIO = %d , state = %d\n", gpio, gpio_get_value(gpio));

	return 0 ;

err_exit:
	return rc;
}


static unsigned short default_keypad_mapping[] = {

		0,	0,	0,	0,	0,	0,	0,	0,  0,	0,	0,	0,	0, 15, 41,	0,
	  0, 56, 42, 93, 29, 16,  2,  0,  0,  0, 44, 31, 30, 17,	3,  0,
	  0, 46, 45, 32, 18,  5,  4,	0,  0, 57, 47, 33, 20, 19,  6,	0,
	  0, 49, 48, 35, 34, 21,  7,	0,  0,  0, 50, 36, 22,  8,  9,	0,
	  0, 51, 37, 23, 24, 11, 10,  0,  0, 52, 53, 38, 39, 25, 12,  0,
	  0, 89, 40,  0, 26, 13,  0,  0, 58, 54, 28, 27,  0, 43,  0, 85,
	  0, 86,	0,	0, 92,  0, 14, 94,  0,	0,124,	0, 71,	0,  0,  0,
		0,	0,	0,	0,	0,	0,158,	0,	0,	0,	0,	0,	0,	0,	0,	0,

	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,100,	0,  0, 97,	0,  0,  0,	0,  0,  0,  0,  0,  0,  0,172,
		0,	0,  0,	0,  0,  0,  0,217,	0,  0,  0,	0,  0,  0,  0,139,
		0,142,238,237, 60,224,225, 61,212,  0,	0,	0,  0,  0,  0,	0,
	150, 62,165,164,163,113,114,115,	0,  0,	0,  0,  0,	0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,	0,	0,  0,  0,  0,	0,  0,
	  0,  0,  0,  0,  0,  0,  0,	0,  0,107,  0,105,102,  0,  0,	0,
		0,	0,108,	0,106,103,  0,	0,  0,	0,109,  0,	0,104,	0,  0,
};

static void asusdec_reset_counter(unsigned long data){
	ec_chip->d_index = 0;
}

static int asusdec_tp_control(int arg){

	int ret_val;

	ret_val = 0;

	if(arg == ASUSDEC_TP_ON){
		if (ec_chip->tp_enable == 0){
			ec_chip->tp_wait_ack = 1;
			ec_chip->tp_enable = 1;
			asusdec_i2c_write_data(ec_chip->client, 0xF4D4);
			ec_chip->d_index = 0;
		}
		if (ec_chip->touchpad_member == -1){
			ec_chip->susb_on = 1;
			ec_chip->init_success = -1;
			asusdec_reset_dock();
		}
	} else if (arg == ASUSDEC_TP_OFF){
		ec_chip->tp_wait_ack = 1;
		ec_chip->tp_enable = 0;
		asusdec_i2c_write_data(ec_chip->client, 0xF5D4);
		ec_chip->d_index = 0;
	} else if (arg == ASUSDEC_TP_TOOGLE){
		ec_chip->tp_wait_ack = 1;
		ec_chip->d_index = 0;
		if(ec_chip->tp_enable)
		{
			ec_chip->tp_enable = 0;
			asusdec_i2c_write_data(ec_chip->client, 0xF5D4);
		}
		else
		{
			ec_chip->tp_enable = 1;
			asusdec_i2c_write_data(ec_chip->client, 0xF4D4);
		}
	} else
		ret_val = -ENOTTY;

	return ret_val;

}
#if (!TOUCHPAD_MODE)
static void asusdec_tp_rel(void){

	ec_chip->touchpad_data.x_sign = (ec_chip->ec_data[0] & X_SIGN_MASK) ? 1:0;
	ec_chip->touchpad_data.y_sign = (ec_chip->ec_data[0] & Y_SIGN_MASK) ? 1:0;
	ec_chip->touchpad_data.left_btn = (ec_chip->ec_data[0] & LEFT_BTN_MASK) ? 1:0;
	ec_chip->touchpad_data.right_btn = (ec_chip->ec_data[0] & RIGHT_BTN_MASK) ? 1:0;
	ec_chip->touchpad_data.delta_x =
		(ec_chip->touchpad_data.x_sign) ? (ec_chip->ec_data[1] - 0xff):ec_chip->ec_data[1];
	ec_chip->touchpad_data.delta_y =
		(ec_chip->touchpad_data.y_sign) ? (ec_chip->ec_data[2] - 0xff):ec_chip->ec_data[2];

	input_report_rel(ec_chip->indev, REL_X, ec_chip->touchpad_data.delta_x);
	input_report_rel(ec_chip->indev, REL_Y, (-1) * ec_chip->touchpad_data.delta_y);
	input_report_key(ec_chip->indev, BTN_LEFT, ec_chip->touchpad_data.left_btn);
	input_report_key(ec_chip->indev, KEY_BACK, ec_chip->touchpad_data.right_btn);
	input_sync(ec_chip->indev);

}
#endif

#if TOUCHPAD_MODE
static void asusdec_tp_abs(void){
	unsigned char SA1,A1,B1,SB1,C1,D1;
	static unsigned char SA1_O=0,A1_O=0,B1_O=0,SB1_O=0,C1_O=0,D1_O=0;
	static int Null_data_times = 0;

	if ((ec_chip->tp_enable) && (ec_chip->touchpad_member == ELANTOUCHPAD)){
		SA1= ec_chip->ec_data[0];
		A1 = ec_chip->ec_data[1];
		B1 = ec_chip->ec_data[2];
		SB1= ec_chip->ec_data[3];
		C1 = ec_chip->ec_data[4];
		D1 = ec_chip->ec_data[5];
		ASUSDEC_INFO("SA1=0x%x A1=0x%x B1=0x%x SB1=0x%x C1=0x%x D1=0x%x \n",SA1,A1,B1,SB1,C1,D1);
		if ( (SA1 == 0xC4) && (A1 == 0xFF) && (B1 == 0xFF) &&
		     (SB1 == 0x02) && (C1 == 0xFF) && (D1 == 0xFF)){
			Null_data_times ++;
			goto asusdec_tp_abs_end;
		}

		if(!(SA1 == SA1_O && A1 == A1_O && B1 == B1_O &&
		   SB1 == SB1_O && C1 == C1_O && D1 == D1_O)) {
			elantech_report_absolute_to_related(ec_chip, &Null_data_times);
		}

asusdec_tp_abs_end:
		SA1_O = SA1;
		A1_O = A1;
		B1_O = B1;
		SB1_O = SB1;
		C1_O = C1;
		D1_O = D1;
	} else if (ec_chip->touchpad_member == -1){
		ec_chip->susb_on = 1;
		ec_chip->init_success = -1;
		asusdec_reset_dock();
	}
}
#endif

static void asusdec_touchpad_processing(void){
	int i;
	int length = 0;
	int tp_start = 0;
	ASUSDEC_I2C_DATA(ec_chip->i2c_data,ec_chip->index);

#if TOUCHPAD_MODE
	length = ec_chip->i2c_data[0];
	if (ec_chip->tp_wait_ack){
		ec_chip->tp_wait_ack = 0;
		tp_start = 1;
		ec_chip->d_index = 0;
	} else {
		tp_start = 0;
	}

	for( i = tp_start; i < length - 1 ; i++){
		ec_chip->ec_data[ec_chip->d_index] = ec_chip->i2c_data[i+2];
		ec_chip->d_index++;
		if (ec_chip->d_index == 6){
			asusdec_tp_abs();
			ec_chip->d_index = 0;
		}
	}


	if (ec_chip->d_index)
		mod_timer(&ec_chip->asusdec_timer,jiffies+(HZ * 1/20));
#else
	length = ec_chip->i2c_data[0];
	for( i = 0; i < length -1 ; i++){
		ec_chip->ec_data[ec_chip->d_index] = ec_chip->i2c_data[i+2];
		ec_chip->d_index++;
		if (ec_chip->d_index == 3){
			asusdec_tp_rel();
			ec_chip->d_index = 0;
		}
	}
#endif
}

#if BATTERY_DRIVER
static int cable_voltage_to_cable_type(int v)
{
	int type;

	switch (v)
	{
		case CABLE_0V:
			type  = BAT_CABLE_OUT; break;
		case CABLE_5V:
			type  = BAT_CABLE_USB; break;
		case CABLE_12V:
		case CABLE_15V:
			type  = BAT_CABLE_AC; break;
		default:
			type  = BAT_CABLE_UNKNOWN; break;
	}
	return type;
}
#endif

static void asusdec_kp_wake(void){
	ASUSDEC_NOTICE("ASUSEC WAKE\n");
	if (asusdec_input_device_create(ec_chip->client)){
		return ;
	}
	input_report_key(ec_chip->indev, KEY_MENU, 1);
	input_sync(ec_chip->indev);
	input_report_key(ec_chip->indev, KEY_MENU, 0);
	input_sync(ec_chip->indev);
}

static void asusdec_kp_smi(void){
	int mcu_cable_type = 0, bat_cable_type = 0;

	if (ec_chip->i2c_data[2] == ASUSDEC_SMI_HANDSHAKING){
		ASUSDEC_NOTICE("ASUSDEC_SMI_HANDSHAKING\n");
		ec_chip->ec_in_s3 = 0;
		isDockIn = 1;
		ec_chip->dock_type = MOBILE_DOCK;
		if (ec_chip->susb_on){
			asusdec_chip_init(ec_chip->client);
		}
	} else if (ec_chip->i2c_data[2] == ASUSDEC_SMI_RESET){
		ASUSDEC_NOTICE("ASUSDEC_SMI_RESET\n");
		ec_chip->init_success = 0;
		asusdec_dock_init_work_function(NULL);
	} else if (ec_chip->i2c_data[2] == ASUSDEC_SMI_WAKE){
		asusdec_kp_wake();
		ASUSDEC_NOTICE("ASUSDEC_SMI_WAKE\n");
	} else if (ec_chip->i2c_data[2] == ASUSDEC_SMI_ADAPTER_EVENT){
		ASUSDEC_NOTICE("ASUSDEC_SMI_ADAPTER_EVENT\n");
#if DOCK_USB
		fsl_dock_ec_callback();
#endif
	} else if (ec_chip->i2c_data[2] == ASUSDEC_SMI_BACKLIGHT_ON){
		ASUSDEC_NOTICE("ASUSDEC_SMI_BACKLIGHT_ON\n");
		ec_chip->susb_on = 1;
		asusdec_reset_dock();
#if DOCK_USB
		tegra_usb3_smi_backlight_on_callback();
#endif
	} else if (ec_chip->i2c_data[2] == ASUSDEC_SMI_AUDIO_DOCK_IN){
		ASUSDEC_NOTICE("ASUSDEC_SMI_ASUSDEC_SMI_AUDIO_DOCK_IN\n");
		if (ec_chip->i2c_data[7] == 0x41) {
			ec_chip->dock_type = AUDIO_DOCK;
			ASUSDEC_NOTICE("dock_type = AUDIO_DOCK\n");
		} else if (ec_chip->i2c_data[7] == 0x53) {
			ec_chip->dock_type = AUDIO_STAND;
			ASUSDEC_NOTICE("dock_type = AUDIO_STAND\n");
		} else {
			ASUSDEC_ERR("DOCK TYPE: UNKNOW!\n");
		}

		memset(&ec_chip->mcu_fw_version, 0, 5);
		strcpy(ec_chip->mcu_fw_version, &ec_chip->i2c_data[3]);
		asusdec_dock_status_report();
#if BATTERY_DRIVER
		if (ec_chip->dock_type == AUDIO_DOCK) {
			mcu_cable_type = asusAudiodec_cable_type_callback();
			bat_cable_type = cable_voltage_to_cable_type(mcu_cable_type);
			if (bat_cable_type < 0)
				ASUSDEC_ERR("battery_callback cable type unknown !\n");
			else
				battery_callback(bat_cable_type);
		}
#endif
	}
}

static void asusdec_kp_kbc(void){
	if (ec_chip->i2c_data[2] == ASUSDEC_PS2_ACK){
		if (ec_chip->kbc_value == 0){
			ASUSDEC_INFO("send led cmd 2\n");
			asusdec_i2c_write_data(ec_chip->client, 0x0000);
		} else {
			ASUSDEC_INFO("send led cmd 2\n");
			asusdec_i2c_write_data(ec_chip->client, 0x0400);
		}
	}
}
static void asusdec_kp_sci(void){
	/* HACK:
	 * because from 0xB0 to 0xE0 key mapping were empty
	 * i decided to use these 32 shorts for mapping the the
	 * functions buttons ( WLAN, WWW, PREVIOUSSONG.. )
	 */
	int ec_signal = 0xB0 + ec_chip->i2c_data[2];

	ec_chip->keypad_data.input_keycode = (int)((unsigned short *)ec_chip->indev->keycode)[ec_signal];//asusdec_kp_sci_table[ec_signal];
	if(ec_chip->keypad_data.input_keycode > 0){
		ASUSDEC_INFO("input_keycode = 0x%x\n", ec_chip->keypad_data.input_keycode);
		input_report_key(ec_chip->indev, ec_chip->keypad_data.input_keycode, 1);
		input_sync(ec_chip->indev);
		input_report_key(ec_chip->indev, ec_chip->keypad_data.input_keycode, 0);
		input_sync(ec_chip->indev);
	}else{
		ASUSDEC_INFO("Unknown ec_signal = 0x%x\n", ec_signal);
	}
}
static void asusdec_kp_key(void){
	int scancode = 0;

	if (ec_chip->i2c_data[2] == ASUSDEC_KEYPAD_KEY_EXTEND){
		ec_chip->keypad_data.extend = 1;
		ec_chip->bc = 3;
	}else{
		ec_chip->keypad_data.extend = 0;
		ec_chip->bc = 2;
	}
	if(ec_chip->i2c_data[ec_chip->bc] == ASUSDEC_KEYPAD_KEY_BREAK){
		ec_chip->keypad_data.value = 0;
		ec_chip->bc++;
	}else{
		ec_chip->keypad_data.value = 1;
	}

	if (ec_chip->keypad_data.extend == 1){
		/* found extended key ( a key that have multiple functions )
		 * HACK:
		 * uppper keyboard layout were empty...
		 * use it! :)
		 */
		scancode = ( 0x80 + ec_chip->i2c_data[ec_chip->bc]);
	} else {
		scancode = ec_chip->i2c_data[ec_chip->bc];
	}
	if (ec_chip->i2c_data[0] == 6){
		if ((ec_chip->i2c_data[2] == 0xE0) &&
			(ec_chip->i2c_data[3] == 0xF0) &&
			(ec_chip->i2c_data[4] == 0x12)){
			/* left shift + extended key
			 * because these keys have the same
			 * scancode of the extended ones use the same HACK.
			 */
			scancode = 0x80 + ec_chip->i2c_data[6];
			ec_chip->keypad_data.value = 1;
		}
		else if ((ec_chip->i2c_data[2] == 0xE0) &&
			(ec_chip->i2c_data[3] == 0xF0) &&
			(ec_chip->i2c_data[4] == 0x59)){
			/* right shift + extended key, read above */
			scancode = 0x80 + ec_chip->i2c_data[6];
			ec_chip->keypad_data.value = 1;
		}
	}
	ASUSDEC_INFO("scancode = 0x%x\n", scancode);

	/* just an extra check, i love error control */
	if(scancode < ec_chip->indev->keycodemax)
	{
		ec_chip->keypad_data.input_keycode = (int)((unsigned short *)ec_chip->indev->keycode)[scancode];
		if(ec_chip->keypad_data.input_keycode > 0){
			ASUSDEC_INFO("input_keycode = 0x%x, input_value = %d\n",
					ec_chip->keypad_data.input_keycode, ec_chip->keypad_data.value);
			input_report_key(ec_chip->indev,
				ec_chip->keypad_data.input_keycode, ec_chip->keypad_data.value);
			input_sync(ec_chip->indev);
		}else{
			ASUSDEC_INFO("Unknown scancode = 0x%x\n", scancode);
		}
	}
}

static void asusdec_keypad_processing(void){

	ASUSDEC_I2C_DATA(ec_chip->i2c_data,ec_chip->index);
	if (ec_chip->i2c_data[1] & ASUSDEC_KBC_MASK)
		asusdec_kp_kbc();
	else if (ec_chip->i2c_data[1] & ASUSDEC_SCI_MASK)
		asusdec_kp_sci();
	else
		asusdec_kp_key();
}

static void asusdec_dock_status_report(void){
	ASUSDEC_INFO("dock_in = %d\n", ec_chip->dock_in);
	switch_set_state(&ec_chip->dock_sdev, (int)switch_value[ec_chip->dock_type]);
#if BATTERY_DRIVER
	queue_delayed_work(asusdec_wq, &ec_chip->asusdec_pad_battery_report_work, 0);
#endif
#if DOCK_SPEAKER
	queue_delayed_work(asusdec_wq, &ec_chip->asusdec_audio_report_work, 0);
#endif
#if AUDIO_DOCK_STAND
	queue_delayed_work(asusdec_wq, &ec_chip->audio_in_out_work, 0);
#endif
}

/*
static int asusdec_get_version_num(void){
	int i;
	int v_num = 0;
	int v_len = strlen(ec_chip->ec_version);
	char *v_str = ec_chip->ec_version;

	if (ec_chip->tf_dock){
		for ( i = v_len - 4; i < v_len; i++)
			v_num = v_num * 10 + v_str[i] - '0';
	}
	ASUSDEC_INFO("v_num = %d\n", v_num);
	return v_num ;
}
*/
#if BATTERY_DRIVER
static void asusdec_pad_battery_report_function(struct work_struct *dat)
{
	int ret_val = 0;
	int dock_in = ec_chip->dock_in;

	ret_val = docking_callback(dock_in);
	ASUSDEC_NOTICE("dock_in = %d, ret_val = %d\n", dock_in, ret_val);
	if (ret_val < 0)
		queue_delayed_work(asusdec_wq, &ec_chip->asusdec_pad_battery_report_work, 2*HZ);
}
#endif

#if DOCK_SPEAKER
static void asusdec_audio_report_function(struct work_struct *dat)
{
	int ret_val = 0;
	int dock_in = ec_chip->dock_in;
	int dock_speaker = (!strncmp(ec_chip->ec_version, "TF201-", 6));

	ret_val = audio_dock_event(dock_in && dock_speaker);
	ASUSDEC_NOTICE("dock_in = %d, dock_speaker = %d, ret_val = %d\n", dock_in, dock_speaker, ret_val);
	if (ret_val < 0)
		queue_delayed_work(asusdec_wq, &ec_chip->asusdec_audio_report_work, 2*HZ);
}
#endif

static void asusdec_lid_report_function(struct work_struct *dat)
{
	int value = 0;

	if (ec_chip->lid_indev == NULL){
		ASUSDEC_ERR("LID input device doesn't exist\n");
		return;
	}
	msleep(CONVERSION_TIME_MS);
	value = gpio_get_value(asusdec_hall_sensor_gpio);
	input_report_switch(ec_chip->lid_indev, SW_LID, !value);
	input_sync(ec_chip->lid_indev);
	ASUSDEC_NOTICE("SW_LID report value = %d\n", !value);
}

static void asusdec_stresstest_work_function(struct work_struct *dat)
{
	asusdec_i2c_read_data(ec_chip->client);
	if (ec_chip->i2c_data[1] & ASUSDEC_OBF_MASK){
		if (ec_chip->i2c_data[1] & ASUSDEC_AUX_MASK){
			asusdec_touchpad_processing();
		}else{
			asusdec_keypad_processing();
		}
	}

	queue_delayed_work(asusdec_wq, &asusdec_stress_work, HZ/ec_chip->polling_rate);
}

static void asusdec_dock_init_work_function(struct work_struct *dat)
{
	int gpio = asusdec_dock_in_gpio;
	//int irq = gpio_to_irq(gpio);
	int i = 0;
	int d_counter = 0;
	int gpio_state = 0;

	ASUSDEC_INFO("Dock-init function\n");
	wake_lock(&ec_chip->wake_lock_init);
	if (201){
		ASUSDEC_NOTICE("TF201 dock-init\n");
		if (ec_chip->dock_det){
			gpio_state = gpio_get_value(gpio);
			for(i = 0; i < 40; i++){
				msleep(50);
				if (gpio_state == gpio_get_value(gpio)){
					d_counter++;
				} else {
					gpio_state = gpio_get_value(gpio);
					d_counter = 0;
				}
				if (d_counter > 4){
					break;
				}
			}
			ec_chip->dock_det--;
			ec_chip->re_init = 0;
		}

		mutex_lock(&ec_chip->input_lock);
		if (gpio_get_value(gpio)){
			ASUSDEC_NOTICE("No dock detected\n");
			ec_chip->dock_in = 0;
			isDockIn = 0;
			ec_chip->init_success = 0;
			ec_chip->tp_enable = 1;
			ec_chip->tf_dock = 0;
			ec_chip->op_mode = 0;
			ec_chip->dock_behavior = 0;
			if ((ec_chip->dock_type == AUDIO_DOCK) || (ec_chip->dock_type == AUDIO_STAND)) {
				memset(ec_chip->mcu_fw_version, 0, 5);
			}
			ec_chip->dock_type = DOCK_UNKNOWN;

			memset(ec_chip->ec_model_name, 0, 32);
			memset(ec_chip->ec_version, 0, 32);
			ec_chip->touchpad_member = -1;
			if (ec_chip->indev){
				input_unregister_device(ec_chip->indev);
				ec_chip->indev = NULL;
			}
			if (ec_chip->private->abs_dev){
				input_unregister_device(ec_chip->private->abs_dev);
				ec_chip->private->abs_dev = NULL;
			}
			asusdec_dock_status_report();
		} else {
			ASUSDEC_NOTICE("Dock-in detected\n");
			if (gpio_get_value(asusdec_hall_sensor_gpio) || (!ec_chip->status)){
				if (ec_chip->init_success == 0){
					if ((!ec_chip->tf_dock) || (!ec_chip->dock_behavior)){
						ec_chip->susb_on = 1;
						ec_chip->dock_type = DOCK_UNKNOWN;
						msleep(200);
						asusdec_reset_dock();
					}
				}
			} else {
				ASUSDEC_NOTICE("Keyboard is closed\n");
			}
		}
		mutex_unlock(&ec_chip->input_lock);
	}
	wake_unlock(&ec_chip->wake_lock_init);
}

static void asusdec_fw_update_work_function(struct work_struct *dat)
{
	int smbus_data;
	int gpio = asusdec_apwake_gpio;
	int irq = gpio_to_irq(gpio);

	mutex_lock(&ec_chip->lock);
	smbus_data = i2c_smbus_read_byte_data(&dockram_client, 0);
	enable_irq(irq);
	BuffPush(smbus_data);
	mutex_unlock(&ec_chip->lock);
}

static void asusdec_work_function(struct work_struct *dat)
{
	int gpio = asusdec_apwake_gpio;
	int irq = gpio_to_irq(gpio);
	int ret_val = 0;

	ec_chip->dock_in = gpio_get_value(asusdec_dock_in_gpio) ? 0 : 1;

	if (ec_chip->wakeup_lcd){
		if (gpio_get_value(asusdec_hall_sensor_gpio)){
			ec_chip->wakeup_lcd = 0;
			wake_lock_timeout(&ec_chip->wake_lock, 3*HZ);
			msleep(500);
		}
	}

	ret_val = asusdec_i2c_read_data(ec_chip->client);
	enable_irq(irq);

	if (ret_val < 0){
		return ;
	}

	if (ec_chip->i2c_data[1] & ASUSDEC_OBF_MASK){
		if (ec_chip->i2c_data[1] & ASUSDEC_SMI_MASK){
			asusdec_kp_smi();
			return ;
		}
	}

	mutex_lock(&ec_chip->input_lock);
	if (ec_chip->indev == NULL){
		mutex_unlock(&ec_chip->input_lock);
		return;
	}
	if (ec_chip->i2c_data[1] & ASUSDEC_OBF_MASK){
		if (ec_chip->i2c_data[1] & ASUSDEC_AUX_MASK){
			if (ec_chip->private->abs_dev)
				asusdec_touchpad_processing();
		}else{
			asusdec_keypad_processing();
		}
	}
	mutex_unlock(&ec_chip->input_lock);
}

static void asusAudiodec_work_function(struct work_struct *dat)
{
	//int ret_val = 0;
	int mcu_cable_type = 0, bat_cable_type = 0;
	int gpio = asusdec_apwake_gpio;
	int irq = gpio_to_irq(gpio);

	// Audio Dock EC function Service
	ASUSDEC_NOTICE("cmd 0x6a i2c_smbus read 8 byte datas\n");

	/*ret_val =*/ asusdec_i2c_read_data(ec_chip->client);
#if BATTERY_DRIVER
		if (ec_chip->dock_type == AUDIO_DOCK) {
			mcu_cable_type = asusAudiodec_cable_type_callback();
			bat_cable_type = cable_voltage_to_cable_type(mcu_cable_type);
			if (bat_cable_type < 0)
				ASUSDEC_ERR("battery_callback cable type unknown !\n");
			else
				battery_callback(bat_cable_type);
		}
#endif
	enable_irq(irq);
}

#if AUDIO_DOCK_STAND
static void asusAudiodec_in_out_work_function(struct work_struct *dat)
{
	if (ec_chip->dock_in) {
		audio_dock_in_out(ec_chip->dock_type);
	} else {
		audio_dock_in_out(DOCK_OUT);
	}
	ASUSDEC_NOTICE("audio dock %s\n", ec_chip->dock_in? "in":"out");
}
#endif

int asusAudiodec_i2c_read_data(char *data, int length)
{
	int ret;
	if ((ec_chip->dock_in == 0)||
		((ec_chip->dock_type != AUDIO_DOCK)&&(ec_chip->dock_type != AUDIO_STAND))){
		return -1;
	}

	ret = i2c_master_send(ec_chip->client, data, 1);
	if(ret<0){
		ASUSDEC_ERR("Fail to send data, errno %d\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ec_chip->client, data, length);
	if(ret<0)
		ASUSDEC_ERR("Fail to receive data, errno %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(asusAudiodec_i2c_read_data);

int asusAudiodec_i2c_write_data(char *data, int length)
{
	int ret;
	if ((ec_chip->dock_in == 0)||
		((ec_chip->dock_type != AUDIO_DOCK)&&(ec_chip->dock_type != AUDIO_STAND))){
		return -1;
	}

	ret = i2c_master_send(ec_chip->client, data, length);
	if(ret<0)
		ASUSDEC_ERR("Fail to write data, errno %d\n", ret);
	return ret;
}
EXPORT_SYMBOL(asusAudiodec_i2c_write_data);

static void asusdec_keypad_set_input_params(struct input_dev *dev)
{
	int i = 0;
	set_bit(EV_KEY, dev->evbit);
	for ( i = 0; i < 246; i++)
		set_bit(i,dev->keybit);

	dev->keycodesize = sizeof(unsigned short);
	dev->keycodemax = ARRAY_SIZE(default_keypad_mapping);
	dev->keycode = default_keypad_mapping;

	input_set_capability(dev, EV_LED, LED_CAPSL);
}

static void asusdec_lid_set_input_params(struct input_dev *dev)
{
	set_bit(EV_SW, dev->evbit);
	set_bit(SW_LID, dev->swbit);
}

static int asusdec_input_device_create(struct i2c_client *client){
	int err = 0;

	if (ec_chip->indev){
		return 0;
	}
	ec_chip->indev = input_allocate_device();
	if (!ec_chip->indev) {
		ASUSDEC_ERR("input_dev allocation fails\n");
		err = -ENOMEM;
		goto exit;
	}

	ec_chip->indev->name = "asusdec";
	ec_chip->indev->phys = "/dev/input/asusdec";
	ec_chip->indev->dev.parent = &client->dev;
	ec_chip->indev->event = asusdec_event;

	asusdec_keypad_set_input_params(ec_chip->indev);
	err = input_register_device(ec_chip->indev);
	if (err) {
		ASUSDEC_ERR("input registration fails\n");
		goto exit_input_free;
	}
	return 0;

exit_input_free:
	input_free_device(ec_chip->indev);
	ec_chip->indev = NULL;
exit:
	return err;

}

static int asusdec_lid_input_device_create(struct i2c_client *client){
	int err = 0;

	ec_chip->lid_indev = input_allocate_device();
	if (!ec_chip->lid_indev) {
		ASUSDEC_ERR("lid_indev allocation fails\n");
		err = -ENOMEM;
		goto exit;
	}

	ec_chip->lid_indev->name = "lid_input";
	ec_chip->lid_indev->phys = "/dev/input/lid_indev";
	ec_chip->lid_indev->dev.parent = &client->dev;

	asusdec_lid_set_input_params(ec_chip->lid_indev);
	err = input_register_device(ec_chip->lid_indev);
	if (err) {
		ASUSDEC_ERR("lid_indev registration fails\n");
		goto exit_input_free;
	}
	return 0;

exit_input_free:
	input_free_device(ec_chip->lid_indev);
	ec_chip->lid_indev = NULL;
exit:
	return err;

}

static int __devinit asusdec_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;

	ASUSDEC_INFO("asusdec probe\n");
	err = sysfs_create_group(&client->dev.kobj, &asusdec_smbus_group);
	if (err) {
		ASUSDEC_ERR("Unable to create the sysfs\n");
		goto exit;
	}

	ec_chip = kzalloc(sizeof (struct asusdec_chip), GFP_KERNEL);
	if (!ec_chip) {
		ASUSDEC_ERR("Memory allocation fails\n");
		err = -ENOMEM;
		goto exit;
	}
	ec_chip->private = kzalloc(sizeof(struct elantech_data), GFP_KERNEL);
	if (!ec_chip->private) {
		ASUSDEC_ERR("Memory allocation (elantech_data) fails\n");
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, ec_chip);
	ec_chip->client = client;
	ec_chip->client->driver = &asusdec_driver;
	ec_chip->client->flags = 1;

	mutex_init(&ec_chip->lock);
	mutex_init(&ec_chip->kbc_lock);
	mutex_init(&ec_chip->input_lock);
	mutex_init(&ec_chip->dock_init_lock);

	init_timer(&ec_chip->asusdec_timer);
	ec_chip->asusdec_timer.function = asusdec_reset_counter;

	wake_lock_init(&ec_chip->wake_lock, WAKE_LOCK_SUSPEND, "asusdec_wake");
	wake_lock_init(&ec_chip->wake_lock_init, WAKE_LOCK_SUSPEND, "asusdec_wake_init");

	ec_chip->status = 0;
	ec_chip->dock_det = 0;
	ec_chip->dock_in = 0;
	ec_chip->dock_init = 0;
	ec_chip->d_index = 0;
	ec_chip->suspend_state = 0;
	ec_chip->init_success = 0;
	ec_chip->wakeup_lcd = 0;
	ec_chip->tp_wait_ack = 0;
	ec_chip->tp_enable = 1;
	ec_chip->re_init = 0;
	ec_chip->ec_wakeup = 0;
	ec_chip->dock_behavior = 0;
	ec_chip->ec_in_s3 = 1;
	ec_chip->susb_on = 1;
	ec_chip->dock_type = DOCK_UNKNOWN;
	ec_chip->indev = NULL;
	ec_chip->lid_indev = NULL;
	ec_chip->private->abs_dev = NULL;
	asusdec_dockram_init(client);

	cdev_add(asusdec_cdev,asusdec_dev,1) ;

	ec_chip->dock_sdev.name = DOCK_SDEV_NAME;
	ec_chip->dock_sdev.print_name = asusdec_switch_name;
	ec_chip->dock_sdev.print_state = asusdec_switch_state;
	if(switch_dev_register(&ec_chip->dock_sdev) < 0){
		ASUSDEC_ERR("switch_dev_register for dock failed!\n");
		goto exit;
	}
	switch_set_state(&ec_chip->dock_sdev, 0);

	err = power_supply_register(&client->dev, &asusdec_power_supply[0]);
	if (err){
		ASUSDEC_ERR("fail to register power supply for dock\n");
		goto exit;
	}

	asusdec_lid_input_device_create(ec_chip->client);
	asusdec_wq = create_singlethread_workqueue("asusdec_wq");
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_hall_sensor_work, asusdec_lid_report_function);
#if BATTERY_DRIVER
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_pad_battery_report_work, asusdec_pad_battery_report_function);
#endif
#if DOCK_SPEAKER
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_audio_report_work, asusdec_audio_report_function);
#endif
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_work, asusdec_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_dock_init_work, asusdec_dock_init_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_fw_update_work, asusdec_fw_update_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_led_on_work, asusdec_keypad_led_on);
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_led_off_work, asusdec_keypad_led_off);
	INIT_DELAYED_WORK_DEFERRABLE(&asusdec_stress_work, asusdec_stresstest_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->asusdec_audio_work, asusAudiodec_work_function);
#if AUDIO_DOCK_STAND
	INIT_DELAYED_WORK_DEFERRABLE(&ec_chip->audio_in_out_work, asusAudiodec_in_out_work_function);
#endif
	asusdec_irq_dock_in(client);
	asusdec_irq_ec_request(client);
	asusdec_irq_hall_sensor(client);
	asusdec_irq(client);

	queue_delayed_work(asusdec_wq, &ec_chip->asusdec_dock_init_work, 0);
	queue_delayed_work(asusdec_wq, &ec_chip->asusdec_hall_sensor_work, 0);

	return 0;

exit:
	return err;
}

static int __devexit asusdec_remove(struct i2c_client *client)
{
	struct asusdec_chip *chip = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s()\n", __func__);
	input_unregister_device(chip->indev);
	kfree(chip);
	return 0;
}

static ssize_t asusdec_info_show(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%s\n", ec_chip->ec_version);
}

static ssize_t asusdec_show(struct device *class,struct device_attribute *attr,char *buf)
{
	int ret_val = 0;
	ret_val = asusdec_i2c_test(ec_chip->client);
	if(ret_val >= 0){
		return sprintf(buf, "1\n");
	} else {
		return sprintf(buf, "0\n");
	}
}

static ssize_t asusdec_tp_status_show(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", (ec_chip->touchpad_member == ELANTOUCHPAD));
}

static ssize_t asusdec_tp_enable_show(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", ec_chip->tp_enable);
}

static ssize_t asusdec_show_dock(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "dock detect = %d\n", ec_chip->dock_in);
}

static ssize_t asusdec_store_led(struct device *class,struct device_attribute *attr,const char *buf, size_t count)
{
	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		if (buf[0] == '0')
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_led_off_work, 0);
		else
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_led_on_work, 0);
	}

	return 0 ;
}

static ssize_t asusdec_charging_led_store(struct device *class,struct device_attribute *attr,const char *buf, size_t count)
{
	int ret_val = 0;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		asusdec_dockram_read_data(0x0A);
		if (buf[0] == '0'){
			ec_chip->i2c_dm_data[0] = 8;
			ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] & 0xF9;
			ret_val = asusdec_dockram_write_data(0x0A,9);
			if (ret_val < 0)
				ASUSDEC_NOTICE("Fail to diable led test\n");
			else
				ASUSDEC_NOTICE("Diable led test\n");
		} else if (buf[0] == '1'){
			asusdec_dockram_read_data(0x0A);
			ec_chip->i2c_dm_data[0] = 8;
			ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] & 0xF9;
			ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] | 0x02;
			ret_val = asusdec_dockram_write_data(0x0A,9);
			if (ret_val < 0)
				ASUSDEC_NOTICE("Fail to enable orange led test\n");
			else
				ASUSDEC_NOTICE("Enable orange led test\n");
		} else if (buf[0] == '2'){
			asusdec_dockram_read_data(0x0A);
			ec_chip->i2c_dm_data[0] = 8;
			ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] & 0xF9;
			ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] | 0x04;
			ret_val = asusdec_dockram_write_data(0x0A,9);
			if (ret_val < 0)
				ASUSDEC_NOTICE("Fail to enable green led test\n");
			else
				ASUSDEC_NOTICE("Enable green led test\n");
		}
	} else {
		ASUSDEC_NOTICE("Fail to enter led test\n");
	}

	return count;
}

static ssize_t asusAudiodec_info_show(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%s\n", ec_chip->mcu_fw_version);
}

static ssize_t asusdec_led_show(struct device *class,struct device_attribute *attr,char *buf)
{
	int ret_val = 0;

	asusdec_dockram_read_data(0x0A);
	ec_chip->i2c_dm_data[0] = 8;
	ec_chip->i2c_dm_data[6] = ec_chip->i2c_dm_data[6] | 0x01;
	ret_val = asusdec_dockram_write_data(0x0A,9);
	if (ret_val < 0)
		return sprintf(buf, "Fail to EC LED Blink\n");
	else
		return sprintf(buf, "EC LED Blink\n");
}

static ssize_t asusdec_store_ec_wakeup(struct device *class,struct device_attribute *attr,const char *buf, size_t count)
{
	if (buf[0] == '0'){
		ec_chip->ec_wakeup = 0;
		ASUSDEC_NOTICE("Set EC shutdown when PAD in LP0\n");
	}
	else{
		ec_chip->ec_wakeup = 1;
		ASUSDEC_NOTICE("Keep EC active when PAD in LP0\n");
	}

	return 0 ;
}

static ssize_t asusdec_show_drain(struct device *class,struct device_attribute *attr,char *buf)
{
	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		asusdec_dockram_read_data(0x0A);

		ec_chip->i2c_dm_data[0] = 8;
		ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] | 0x8;
		asusdec_dockram_write_data(0x0A,9);
		ASUSDEC_NOTICE("discharging 15 seconds\n");
		return sprintf(buf, "discharging 15 seconds\n");
	}

	return 0;
}

static ssize_t asusdec_show_dock_battery(struct device *class,struct device_attribute *attr,char *buf)
{
	int bat_percentage = 0;
	int ret_val = 0;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		ret_val = asusdec_dockram_read_data(0x14);

		if (ret_val < 0)
			return sprintf(buf, "-1\n");
		else{
			bat_percentage = (ec_chip->i2c_dm_data[14] << 8 )| ec_chip->i2c_dm_data[13];
			return sprintf(buf, "%d\n", bat_percentage);
		}
	}

	return sprintf(buf, "-1\n");
}

static ssize_t asusdec_show_dock_battery_status(struct device *class,struct device_attribute *attr,char *buf)
{
	//int bat_percentage = 0;
	int ret_val = 0;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		if (ec_chip->ec_in_s3 && ec_chip->status){
			msleep(200);
		}

		ret_val = asusdec_dockram_read_data(0x0A);

		if (ret_val < 0){
			return sprintf(buf, "-1\n");
		}
		else {
			if (ec_chip->i2c_dm_data[1] & 0x4)
				return sprintf(buf, "1\n");
			else
				return sprintf(buf, "0\n");;
		}
	}
	return sprintf(buf, "-1\n");
}


static ssize_t asusdec_show_dock_battery_all(struct device *class,struct device_attribute *attr,char *buf)
{
	int i = 0;
	char temp_buf[64];
	int ret_val = 0;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		ret_val = asusdec_dockram_read_data(0x14);

		if (ret_val < 0)
			return sprintf(buf, "fail to get dock-battery info\n");
		else{
			sprintf(temp_buf, "byte[0] = 0x%x\n", ec_chip->i2c_dm_data[i]);
			strcpy(buf, temp_buf);
			for (i = 1; i < 17; i++){
				sprintf(temp_buf, "byte[%d] = 0x%x\n", i, ec_chip->i2c_dm_data[i]);
				strcat(buf, temp_buf);
			}
			return strlen(buf);
		}
	}

	return sprintf(buf, "fail to get dock-battery info\n");
}

static ssize_t asusdec_show_dock_control_flag(struct device *class,struct device_attribute *attr,char *buf)
{
	int i = 0;
	char temp_buf[64];
	int ret_val = 0;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		ret_val = asusdec_dockram_read_data(0x0A);

		if (ret_val < 0)
			return sprintf(buf, "fail to get control-flag info\n");
		else{
			sprintf(temp_buf, "byte[0] = 0x%x\n", ec_chip->i2c_dm_data[i]);
			strcpy(buf, temp_buf);
			for (i = 1; i < 9; i++){
				sprintf(temp_buf, "byte[%d] = 0x%x\n", i, ec_chip->i2c_dm_data[i]);
				strcat(buf, temp_buf);
			}
			return strlen(buf);
		}
	}

	return sprintf(buf, "fail to get control-flag info\n");
}

static ssize_t asusdec_show_lid_status(struct device *class,struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", gpio_get_value(asusdec_hall_sensor_gpio));
}

static int asusdec_suspend(struct i2c_client *client, pm_message_t mesg){
	int ret_val;

	ASUSDEC_NOTICE("asusdec_suspend+\n");
	ec_chip->susb_on = 0;
	flush_workqueue(asusdec_wq);
	if (ec_chip->dock_in && (ec_chip->ec_in_s3 == 0)){
		ret_val = asusdec_i2c_test(ec_chip->client);
		if(ret_val < 0){
			goto fail_to_access_ec;
		}

		asusdec_dockram_read_data(0x0A);

		ec_chip->i2c_dm_data[0] = 8;
		ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] & 0xDF;
		ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] | 0x22;
		if (ec_chip->ec_wakeup){
			ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] | 0x80;
		} else {
			ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] & 0x7F;
		}
		asusdec_dockram_write_data(0x0A,9);
	}

fail_to_access_ec:
	flush_workqueue(asusdec_wq);
	ec_chip->suspend_state = 1;
	ec_chip->dock_det = 0;
	ec_chip->init_success = 0;
	ec_chip->ec_in_s3 = 1;
	ec_chip->touchpad_member = -1;
	ASUSDEC_NOTICE("asusdec_suspend-\n");
	return 0;
}

static int asusdec_resume(struct i2c_client *client){

	printk("asusdec_resume+\n");
	if ((gpio_get_value(asusdec_dock_in_gpio) == 0) && gpio_get_value(asusdec_apwake_gpio)) {
		ec_chip->dock_type = DOCK_UNKNOWN;
		asusdec_reset_dock();
	}

	ec_chip->suspend_state = 0;
	//queue_delayed_work(asusdec_wq, &ec_chip->asusdec_hall_sensor_work, 0);
	asusdec_lid_report_function(NULL);
	wake_lock(&ec_chip->wake_lock_init);
	ec_chip->init_success = 0;
	queue_delayed_work(asusdec_wq, &ec_chip->asusdec_dock_init_work, 0);

	printk("asusdec_resume-\n");
	return 0;
}

static int asusdec_set_wakeup_cmd(void){
	int ret_val = 0;

	if (ec_chip->dock_in){
		ret_val = asusdec_i2c_test(ec_chip->client);
		if(ret_val >= 0){
			asusdec_dockram_read_data(0x0A);
			ec_chip->i2c_dm_data[0] = 8;
			if (ec_chip->ec_wakeup){
				ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] | 0x80;
			} else {
				ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] & 0x7F;
			}
			asusdec_dockram_write_data(0x0A,9);
		}
	}
	return 0;
}
static ssize_t asusdec_switch_name(struct switch_dev *sdev, char *buf)
{
	if ((ec_chip->dock_type == AUDIO_DOCK) ||(ec_chip->dock_type == AUDIO_STAND ))
		return sprintf(buf, "%s\n", ec_chip->mcu_fw_version);
	else
		return sprintf(buf, "%s\n", ec_chip->ec_version);
}

static ssize_t asusdec_switch_state(struct switch_dev *sdev, char *buf)
{
	if (201) {
		return sprintf(buf, "%s\n", switch_value[ec_chip->dock_type]);
	} else {
		return sprintf(buf, "%s\n", "0");
	}
}

static int asusdec_open(struct inode *inode, struct file *flip){
	ASUSDEC_NOTICE(" ");
	return 0;
}
static int asusdec_release(struct inode *inode, struct file *flip){
	ASUSDEC_NOTICE(" ");
	return 0;
}
static long asusdec_ioctl(struct file *flip,
					unsigned int cmd, unsigned long arg){
	int err = 1;
	char *envp[3];
	char name_buf[64];
	int env_offset = 0;
	int length = 0;

	if (_IOC_TYPE(cmd) != ASUSDEC_IOC_MAGIC)
	 return -ENOTTY;
	if (_IOC_NR(cmd) > ASUSDEC_IOC_MAXNR)
	return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	 switch (cmd) {
        case ASUSDEC_POLLING_DATA:
			if (arg == ASUSDEC_IOCTL_HEAVY){
				ASUSDEC_NOTICE("heavy polling\n");
				ec_chip->polling_rate = 80;
				queue_delayed_work(asusdec_wq, &asusdec_stress_work, HZ/ec_chip->polling_rate);
			}
			else if (arg == ASUSDEC_IOCTL_NORMAL){
				ASUSDEC_NOTICE("normal polling\n");
				ec_chip->polling_rate = 10;
				queue_delayed_work(asusdec_wq, &asusdec_stress_work, HZ/ec_chip->polling_rate);
			}
			else if  (arg == ASUSDEC_IOCTL_END){
				ASUSDEC_NOTICE("polling end\n");
		    	cancel_delayed_work_sync(&asusdec_stress_work) ;
			}
			else
				return -ENOTTY;
			break;
		case ASUSDEC_FW_UPDATE:
			if (ec_chip->dock_in){
				ASUSDEC_NOTICE("ASUSDEC_FW_UPDATE\n");
				buff_in_ptr = 0;
				buff_out_ptr = 0;
				h2ec_count = 0;
				ec_chip->suspend_state = 0;
				ec_chip->status = 0;
				asusdec_reset_dock();
				wake_lock_timeout(&ec_chip->wake_lock, 3*60*HZ);
				msleep(3000);
				ec_chip->op_mode = 1;
				ec_chip->i2c_dm_data[0] = 0x02;
				ec_chip->i2c_dm_data[1] = 0x55;
				ec_chip->i2c_dm_data[2] = 0xAA;
				i2c_smbus_write_i2c_block_data(&dockram_client, 0x40, 3, ec_chip->i2c_dm_data);
				ec_chip->init_success = 0;
				ec_chip->dock_behavior = 0;
				ec_chip->tf_dock = 0;
				msleep(1000);
			} else {
				ASUSDEC_NOTICE("No dock detected\n");
				return -1;
			}
			break;
		case ASUSDEC_INIT:
			msleep(500);
			ec_chip->status = 0;
			ec_chip->op_mode = 0;
			queue_delayed_work(asusdec_wq, &ec_chip->asusdec_dock_init_work, 0);
			msleep(2500);
			ASUSDEC_NOTICE("ASUSDEC_INIT - EC version: %s\n", ec_chip->ec_version);
			length = strlen(ec_chip->ec_version);
			ec_chip->ec_version[length] = (int)NULL;
			snprintf(name_buf, sizeof(name_buf), "SWITCH_NAME=%s", ec_chip->ec_version);
			envp[env_offset++] = name_buf;
			envp[env_offset] = NULL;
			kobject_uevent_env(&ec_chip->dock_sdev.dev->kobj, KOBJ_CHANGE, envp);
			break;
		case ASUSDEC_TP_CONTROL:
			ASUSDEC_NOTICE("ASUSDEC_TP_CONTROL\n");
			if ((ec_chip->op_mode == 0) && ec_chip->dock_in){
				err = asusdec_tp_control(arg);
				return err;
			}
			else
				return -ENOTTY;
		case ASUSDEC_EC_WAKEUP:
			msleep(500);
			ASUSDEC_NOTICE("ASUSDEC_EC_WAKEUP, arg = %d\n", arg);
			if (arg == ASUSDEC_EC_OFF){
				ec_chip->ec_wakeup = 0;
				ASUSDEC_NOTICE("Set EC shutdown when PAD in LP0\n");
				return asusdec_set_wakeup_cmd();
			}
			else if (arg == ASUSDEC_EC_ON){
				ec_chip->ec_wakeup = 1;
				ASUSDEC_NOTICE("Keep EC active when PAD in LP0\n");
				return asusdec_set_wakeup_cmd();
			}
			else {
				ASUSDEC_ERR("Unknown argument");
				return -ENOTTY;
			}
		case ASUSDEC_FW_DUMMY:
			ASUSDEC_NOTICE("ASUSDEC_FW_DUMMY\n");
			ec_chip->i2c_dm_data[0] = 0x02;
			ec_chip->i2c_dm_data[1] = 0x55;
			ec_chip->i2c_dm_data[2] = 0xAA;
			i2c_smbus_write_i2c_block_data(&dockram_client, 0x40, 3, ec_chip->i2c_dm_data);
			return 0;
        default:
            return -ENOTTY;
	}
    return 0;
}

static void asusdec_enter_factory_mode(void){

	ASUSDEC_NOTICE("Entering factory mode\n");
	asusdec_dockram_read_data(0x0A);
	ec_chip->i2c_dm_data[0] = 8;
	ec_chip->i2c_dm_data[5] = ec_chip->i2c_dm_data[5] | 0x40;
	asusdec_dockram_write_data(0x0A,9);
}

static int BuffDataSize(void)
{
    int in = buff_in_ptr;
    int out = buff_out_ptr;

    if (in >= out)
    {
        return (in - out);
    }
    else
    {
        return ((EC_BUFF_LEN - out) + in);
    }
}
static void BuffPush(char data)
{

    if (BuffDataSize() >= (EC_BUFF_LEN -1))
    {
        ASUSDEC_ERR("Error: EC work-buf overflow \n");
        return;
    }

    ec_to_host_buffer[buff_in_ptr] = data;
    buff_in_ptr++;
    if (buff_in_ptr >= EC_BUFF_LEN)
    {
        buff_in_ptr = 0;
    }
}

static char BuffGet(void)
{
    char c = (char)0;

    if (BuffDataSize() != 0)
    {
        c = (char) ec_to_host_buffer[buff_out_ptr];
        buff_out_ptr++;
         if (buff_out_ptr >= EC_BUFF_LEN)
         {
             buff_out_ptr = 0;
         }
    }
    return c;
}

static ssize_t ec_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int i = 0;
    int ret;
    char tmp_buf[EC_BUFF_LEN];
	static int f_counter = 0;
	static int total_buf = 0;

	mutex_lock(&ec_chip->lock);
	mutex_unlock(&ec_chip->lock);

    while ((BuffDataSize() > 0) && count)
    {
        tmp_buf[i] = BuffGet();
        count--;
        i++;
		f_counter = 0;
		total_buf++;
    }

    ret = copy_to_user(buf, tmp_buf, i);
    if (ret == 0)
    {
        ret = i;
    }


    return ret;
}

static ssize_t ec_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int err;
    int i;

    if (h2ec_count > 0)
    {                   /* There is still data in the buffer that */
        return -EBUSY;  /* was not sent to the EC */
    }
    if (count > EC_BUFF_LEN)
    {
        return -EINVAL; /* data size is too big */
    }

    err = copy_from_user(host_to_ec_buffer, buf, count);
    if (err)
    {
        ASUSDEC_ERR("ec_write copy error\n");
        return err;
    }

    h2ec_count = count;
    for (i = 0; i < count ; i++)
    {
		i2c_smbus_write_byte_data(&dockram_client, host_to_ec_buffer[i],0);
    }
    h2ec_count = 0;
    return count;

}

static int asusdec_event(struct input_dev *dev, unsigned int type, unsigned int code, int value){
	ASUSDEC_INFO("type = 0x%x, code = 0x%x, value = 0x%x\n", type, code, value);
	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		if ((type == EV_LED) && (code == LED_CAPSL)){
			if(value == 0){
				queue_delayed_work(asusdec_wq, &ec_chip->asusdec_led_off_work, 0);
				return 0;
			} else {
				queue_delayed_work(asusdec_wq, &ec_chip->asusdec_led_on_work, 0);
				return 0;
			}
		}
	}
	return -ENOTTY;
}

static int asusdec_dock_battery_get_capacity(union power_supply_propval *val){
	int bat_percentage = 0;
	int ret_val = 0;

	val->intval = -1;

	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		if (ec_chip->ec_in_s3 && ec_chip->status){
			msleep(200);
		}

		ret_val = asusdec_dockram_read_data(0x14);

		if (ret_val < 0){
			return -1;
		}
		else {
			bat_percentage = (ec_chip->i2c_dm_data[14] << 8 )| ec_chip->i2c_dm_data[13];
			bat_percentage = ((bat_percentage >= 100) ? 100 : bat_percentage);

			if(bat_percentage >70 && bat_percentage <80)
				bat_percentage-=1;
			else if(bat_percentage >60&& bat_percentage <=70)
				bat_percentage-=2;
			else if(bat_percentage >50&& bat_percentage <=60)
				bat_percentage-=3;
			else if(bat_percentage >30&& bat_percentage <=50)
				bat_percentage-=4;
			else if(bat_percentage >=0&& bat_percentage <=30)
				bat_percentage-=5;

			bat_percentage = ((bat_percentage <= 0) ? 0 : bat_percentage);
			val->intval = bat_percentage;
			ASUSDEC_NOTICE("dock battery level = %d\n", bat_percentage);
			return 0;
		}
	}
	return -1;
}

static int asusdec_dock_battery_get_status(union power_supply_propval *val){
	//int bat_percentage = 0;
	int ret_val = 0;

	val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
	if ((ec_chip->op_mode == 0) && (ec_chip->dock_in)){
		if (ec_chip->ec_in_s3 && ec_chip->status){
			msleep(200);
		}

		ret_val = asusdec_dockram_read_data(0x0A);

		if (ret_val < 0){
			return -1;
		}
		else {
			if (ec_chip->i2c_dm_data[1] & 0x4)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			return 0;
		}
	}
	return -1;
}

static int asusdec_dock_battery_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	switch (psp) {
		case POWER_SUPPLY_PROP_CAPACITY:
			if(asusdec_dock_battery_get_capacity(val) < 0)
				goto error;
			break;
		case POWER_SUPPLY_PROP_STATUS:
			if(asusdec_dock_battery_get_status(val) < 0)
				goto error;
			break;
		default:
			return -EINVAL;
	}
	return 0;

error:
	return -EINVAL;
}

int asusdec_is_ac_over_10v_callback(void){

	int ret_val, err;

	ASUSDEC_NOTICE("access dockram\n");
	if (ec_chip->dock_in && (ec_chip->dock_type == MOBILE_DOCK)){
		msleep(250);
		ret_val = asusdec_i2c_test(ec_chip->client);
		if(ret_val < 0){
			goto fail_to_access_ec;
		}
		err = asusdec_dockram_read_data(0x0A);
		ASUSDEC_NOTICE("byte[1] = 0x%x\n", ec_chip->i2c_dm_data[1]);
		if(err < 0)
			goto fail_to_access_ec;

		return ec_chip->i2c_dm_data[1] & 0x20;
	}

fail_to_access_ec:
	ASUSDEC_NOTICE("dock doesn't exist or fail to access ec\n");
	return -1;
}
EXPORT_SYMBOL(asusdec_is_ac_over_10v_callback);

int asusAudiodec_cable_type_callback(void) {

	int retval = 0, retry = 3;
	//int error;
	u8 cmd[2];

	cmd[0] = 0x55;
	cmd[1] = 0x00;

	ASUSDEC_NOTICE("access MCU to distinguish cable type\n");
	if (ec_chip->dock_in){
		if ((ec_chip->dock_type == AUDIO_DOCK) ||(ec_chip->dock_type == AUDIO_STAND)) {

			while(retry-- >0) {
				msleep(250);
				retval = asusAudiodec_i2c_read_data(cmd, 1);
				if (retval < 0)
					ASUSDEC_ERR("asusAudiodec i2c read data failed\n");

				ASUSDEC_NOTICE("cable type return value = 0x%02x\n", cmd[0]);	//0:0V, 5:5V, 12:12V, 15:15V
				if (cmd[0] != 0x55) {
					return cmd[0];
				}
			}
		}
	}
//fail_to_access_mcu:
	ASUSDEC_NOTICE("MCU doesn't exist or fail to access MCU\n");
	return -1;
}
EXPORT_SYMBOL(asusAudiodec_cable_type_callback);

static int __init asusdec_init(void)
{
	int err_code = 0;

	printk(KERN_INFO "%s+ #####\n", __func__);
	if (asusdec_major) {
		asusdec_dev = MKDEV(asusdec_major, asusdec_minor);
		err_code = register_chrdev_region(asusdec_dev, 1, "asusdec");
	} else {
		err_code = alloc_chrdev_region(&asusdec_dev, asusdec_minor, 1,"asusdec");
		asusdec_major = MAJOR(asusdec_dev);
	}

	ASUSDEC_NOTICE("cdev_alloc\n") ;
	asusdec_cdev = cdev_alloc() ;
	asusdec_cdev->owner = THIS_MODULE ;
	asusdec_cdev->ops = &asusdec_fops ;

	err_code=i2c_add_driver(&asusdec_driver);
	if(err_code){
		ASUSDEC_ERR("i2c_add_driver fail\n") ;
		goto i2c_add_driver_fail ;
	}
	asusdec_class = class_create(THIS_MODULE, "asusdec");
	if(asusdec_class <= 0){
		ASUSDEC_ERR("asusdec_class create fail\n");
		err_code = -1;
		goto class_create_fail ;
	}
	asusdec_device = device_create(asusdec_class, NULL, MKDEV(asusdec_major, asusdec_minor), NULL, "asusdec" );
	if(asusdec_device <= 0){
		ASUSDEC_ERR("asusdec_device create fail\n");
		err_code = -1;
		goto device_create_fail ;
	}

	ASUSDEC_INFO("return value %d\n", err_code) ;
	printk(KERN_INFO "%s- #####\n", __func__);
	return 0;

device_create_fail :
	class_destroy(asusdec_class) ;
class_create_fail :
	i2c_del_driver(&asusdec_driver);
i2c_add_driver_fail :
	printk(KERN_INFO "%s- #####\n", __func__);
	return err_code;

}

static void __exit asusdec_exit(void)
{
	device_destroy(asusdec_class,MKDEV(asusdec_major, asusdec_minor)) ;
	class_destroy(asusdec_class) ;
	i2c_del_driver(&asusdec_driver);
	unregister_chrdev_region(asusdec_dev, 1);
	switch_dev_unregister(&ec_chip->dock_sdev);
}

module_init(asusdec_init);
module_exit(asusdec_exit);

