#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>

#define PROC_ENTRY_NAME "proc_exchange"
#define MESSAGE_BUFFER_SIZE 256
#define PROC_ENTRY_MODE 0666

static char message_buffer[MESSAGE_BUFFER_SIZE];
static size_t message_length;
static struct proc_dir_entry *proc_entry;
static DEFINE_MUTEX(message_lock);

static ssize_t proc_read_handler(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    ssize_t result;
    (void)file;
    
    mutex_lock(&message_lock);
    
    result = simple_read_from_buffer(user_buffer, count, offset, message_buffer, message_length);

    mutex_unlock(&message_lock);

    return result;
}

static ssize_t proc_write_handler(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
    char new_message[MESSAGE_BUFFER_SIZE];
    (void)file;
    (void)offset;

    if (count >= MESSAGE_BUFFER_SIZE) 
        return -ENOSPC;

    if (copy_from_user(new_message, user_buffer, count) != 0) 
        return -EFAULT;

    new_message[count] = '\0';
    mutex_lock(&message_lock);

    memcpy(message_buffer, new_message, count + 1);
    message_length = count;

    mutex_unlock(&message_lock);
    return count;
}

static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read_handler,
    .proc_write = proc_write_handler,
};

static int __init proc_exchange_init(void)
{
    proc_entry = proc_create(PROC_ENTRY_NAME, PROC_ENTRY_MODE, NULL, &proc_file_ops);

    if (proc_entry == NULL) {
        pr_err("proc_exchange: failed to create /proc/%s\n", PROC_ENTRY_NAME);
        return -ENOMEM;
    }
    pr_info("proc_exchange: created /proc/%s\n", PROC_ENTRY_NAME);

    return 0;
}

static void __exit proc_exchange_exit(void)
{
    proc_remove(proc_entry);
    proc_entry = NULL;
    pr_info("proc_exchange: removed /proc/%s\n", PROC_ENTRY_NAME);
}


module_init(proc_exchange_init);
module_exit(proc_exchange_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gerasimov Sergei");
MODULE_DESCRIPTION("Kernel module for exchanging data through procfs");
MODULE_VERSION("1.0");