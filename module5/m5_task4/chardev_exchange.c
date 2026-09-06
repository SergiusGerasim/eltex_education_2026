// SPDX-License-Identifier: GPL-2.0-only
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define MODULE_NAME "chardev_exchange"
#define DEVICE_NAME "chardev_exchange"
#define MESSAGE_BUFFER_SIZE 256 // 255 + 1 резервный под '\0'

static dev_t device_number; // major/minor
static struct cdev exchange_cdev;
static struct class *exchange_class;
static struct device *exchange_device;
static char message_buffer[MESSAGE_BUFFER_SIZE];
static size_t message_length;
static DEFINE_MUTEX(message_lock);

static int exchange_open(struct inode *inode, struct file *file)
{
	// сообщаем ядру, что /dev/chardev_exchange рассматривается как последовательный поток данных
	return nonseekable_open(inode, file);
}

static int exchange_release(struct inode *inode, struct file *file)
{
	(void)inode;
	(void)file;

	return 0;
}

static ssize_t exchange_read(struct file *file, char __user *user_buffer,
			     size_t count, loff_t *offset)
{
	ssize_t result;

	(void)file;
	if (mutex_lock_interruptible(&message_lock))
		return -ERESTARTSYS;

	result = simple_read_from_buffer(user_buffer, count, offset,
					 message_buffer, message_length);
	// возвращает 0 при достижении конца -> cat не уйдёт в бесконечный цикл
	mutex_unlock(&message_lock);

	return result;
}

static ssize_t exchange_write(struct file *file,
			      const char __user *user_buffer, size_t count,
			      loff_t *offset)
{
	char new_message[MESSAGE_BUFFER_SIZE];
	// сначала данные копируем сюда, во временный буфер 
	// если успешно, то заменяем уже данные в message_buffer
	(void)file;
	if (count >= MESSAGE_BUFFER_SIZE)
		return -ENOSPC;
	// копируем байты из пользовательской памяти
	if (copy_from_user(new_message, user_buffer, count))
		return -EFAULT;

	new_message[count] = '\0';
	if (mutex_lock_interruptible(&message_lock))
		return -ERESTARTSYS;

	memcpy(message_buffer, new_message, count + 1);
	message_length = count;
	*offset = count;
	mutex_unlock(&message_lock);

	return count;
}

// таблица обработчиков
// чтобы связать системные вызовы с функциями модуля
static const struct file_operations exchange_file_operations = {
	.owner = THIS_MODULE,
	.open = exchange_open,
	.release = exchange_release,
	.read = exchange_read,
	.write = exchange_write,
};

static int __init chardev_exchange_init(void)
{
	int error;
	// выделяет номер устройству, пока просто резервируем
	error = alloc_chrdev_region(&device_number, 0, 1, DEVICE_NAME);
	// даёт больше контроля чем register_chrdev
	if (error)
		return error;
	// связка cdev с таблицей обработчиков
	cdev_init(&exchange_cdev, &exchange_file_operations);
	exchange_cdev.owner = THIS_MODULE;
	// (регестрируем) добовляем устройство в подсистему символьных устройств ядра
	error = cdev_add(&exchange_cdev, device_number, 1);
	if (error)
		goto unregister_device_number;

	exchange_class = class_create(MODULE_NAME);
	if (IS_ERR(exchange_class)) {
		error = PTR_ERR(exchange_class);
		exchange_class = NULL;
		goto delete_cdev;
	}
	// объект с заданным классом и номером устройства
	exchange_device = device_create(exchange_class, NULL, device_number, NULL,
					DEVICE_NAME);
	if (IS_ERR(exchange_device)) {
		error = PTR_ERR(exchange_device);
		exchange_device = NULL;
		goto destroy_class;
	}
	// запись в журнал ядра
	pr_info(MODULE_NAME ": created /dev/%s with major %u and minor %u\n",
		DEVICE_NAME, MAJOR(device_number), MINOR(device_number));
	return 0;

destroy_class:
	class_destroy(exchange_class);
	exchange_class = NULL;
delete_cdev:
	cdev_del(&exchange_cdev);
unregister_device_number:
	unregister_chrdev_region(device_number, 1);
	return error;
}

static void __exit chardev_exchange_exit(void)
{
	// выгрузка модуля
	device_destroy(exchange_class, device_number);
	exchange_device = NULL;
	class_destroy(exchange_class);
	exchange_class = NULL;
	cdev_del(&exchange_cdev);
	unregister_chrdev_region(device_number, 1);
	pr_info(MODULE_NAME ": removed /dev/%s\n", DEVICE_NAME);
}

module_init(chardev_exchange_init);
module_exit(chardev_exchange_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerasimov Sergei");
MODULE_DESCRIPTION("Kernel module for exchanging data through a character device");
MODULE_VERSION("1.0");
