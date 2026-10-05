/*
 * Licensed under the Gerasimov Educational License v1.0.
 * See the LICENSE file for details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

static int __init hello_init(void)
{
    pr_info("hello: module loaded\n");
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("hello: module unloaded\n");
}
module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("Gerasimov Educational License v1.0");
MODULE_AUTHOR("Gerasimov Sergei Mikhailovich <se.gerasimov.m@gmail.com>");
MODULE_DESCRIPTION("Hello World kernel module for Eltex module 5 task 1");
