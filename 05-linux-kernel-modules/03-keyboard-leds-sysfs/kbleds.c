#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kd.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>
#include <linux/workqueue.h>

#define MODULE_NAME "kbleds"
#define SYSFS_DIRECTORY_NAME "kbleds"
#define BLINK_DELAY_MS 500
#define KEYBOARD_LED_MASK_MAX 0x07
#define KEYBOARD_LED_STATE_RESTORE 0xFF

static unsigned int led_mask;
static bool leds_are_on;
static bool module_is_stopping;
static struct kobject *kbleds_kobject;
static DEFINE_MUTEX(state_lock);
static void blink_work_handler(struct work_struct *work);
static DECLARE_DELAYED_WORK(blink_work, blink_work_handler);

static int set_keyboard_leds(unsigned int state)
{
	struct tty_struct *tty;
	struct tty_driver *driver;

	if (!vc_cons[fg_console].d)
		return -ENODEV;

	tty = vc_cons[fg_console].d->port.tty;
	if (!tty || !tty->driver)
		return -ENODEV;

	driver = tty->driver;
	if (!driver->ops || !driver->ops->ioctl)
		return -EOPNOTSUPP;

	return driver->ops->ioctl(tty, KDSETLED, state);
}

static void blink_work_handler(struct work_struct *work)
{
	unsigned int state;
	int error;

	(void)work;
	mutex_lock(&state_lock);
	if (module_is_stopping) {
		mutex_unlock(&state_lock);
		return;
	}

	if (led_mask == 0) {
		leds_are_on = false;
		state = 0;
	} else {
		leds_are_on = !leds_are_on;
		state = leds_are_on ? led_mask : 0;
	}

	error = set_keyboard_leds(state);
	if (error)
		pr_warn_ratelimited(MODULE_NAME
				": failed to set LED state: %d\n", error);

	if (led_mask != 0)
		schedule_delayed_work(&blink_work,
				      msecs_to_jiffies(BLINK_DELAY_MS));
	mutex_unlock(&state_lock);
}

static ssize_t led_mask_show(struct kobject *kobject,
			     struct kobj_attribute *attribute, char *buffer)
{
	unsigned int value;

	(void)kobject;
	(void)attribute;
	mutex_lock(&state_lock);
	value = led_mask;
	mutex_unlock(&state_lock);

	return sysfs_emit(buffer, "%u\n", value);
}

static ssize_t led_mask_store(struct kobject *kobject,
			      struct kobj_attribute *attribute,
			      const char *buffer, size_t count)
{
	unsigned int new_mask;
	int error;

	(void)kobject;
	(void)attribute;
	error = kstrtouint(buffer, 0, &new_mask);
	if (error)
		return error;
	if (new_mask > KEYBOARD_LED_MASK_MAX)
		return -ERANGE;

	mutex_lock(&state_lock);
	led_mask = new_mask;
	leds_are_on = false;
	mutex_unlock(&state_lock);

	mod_delayed_work(system_wq, &blink_work, 0);
	return count;
}

static struct kobj_attribute led_mask_attribute =
	__ATTR(led_mask, 0660, led_mask_show, led_mask_store);

static int __init kbleds_init(void)
{
	int error;

	kbleds_kobject = kobject_create_and_add(SYSFS_DIRECTORY_NAME, kernel_kobj);
	if (!kbleds_kobject)
		return -ENOMEM;

	error = sysfs_create_file(kbleds_kobject, &led_mask_attribute.attr);
	if (error) {
		kobject_put(kbleds_kobject);
		kbleds_kobject = NULL;
		return error;
	}

	pr_info(MODULE_NAME ": loaded; use /sys/kernel/%s/led_mask\n",
		SYSFS_DIRECTORY_NAME);
	return 0;
}

static void __exit kbleds_exit(void)
{
	sysfs_remove_file(kbleds_kobject, &led_mask_attribute.attr);
	kobject_put(kbleds_kobject);
	kbleds_kobject = NULL;

	mutex_lock(&state_lock);
	module_is_stopping = true;
	mutex_unlock(&state_lock);
	cancel_delayed_work_sync(&blink_work);

	if (set_keyboard_leds(KEYBOARD_LED_STATE_RESTORE))
		pr_warn(MODULE_NAME ": failed to restore keyboard LEDs\n");

	pr_info(MODULE_NAME ": unloaded\n");
}

module_init(kbleds_init);
module_exit(kbleds_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerasimov Sergei");
MODULE_DESCRIPTION("Keyboard LED blinking controlled through sysfs");
MODULE_VERSION("1.0");
