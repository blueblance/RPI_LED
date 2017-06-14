/*
 * kernel module for LED
 *
 * Author:
 * 	Wan-Ting CHEN (wanting@gmail.com)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>				// file_operations
#include <linux/cdev.h>				// Char Device Registration
#include <linux/gpio.h>				// gpio control
#include <linux/delay.h>			// mdelay
#include <linux/device.h>			// class_create
#include <linux/types.h>			// dev_t
#include <asm/uaccess.h>			// copy_from_user
#include <linux/interrupt.h>

#define LED_DRIVER_NAME "LED"
#define	BUF_SIZE	5
#define BUTTON 23
#define MY_GPIO_INT_NAME "my_button_int"
#define MY_DEV_NAME "my_device"

static dev_t driverno ;               //Default driver number
static struct cdev *gpio_cdev;
static struct class *gpio_class;
static int irq_num = 0;

static struct gpio leds[] = {
    {  2, GPIOF_OUT_INIT_LOW, "LED_0" },
    {  3, GPIOF_OUT_INIT_LOW, "LED_1" },
    {  4, GPIOF_OUT_INIT_LOW, "LED_2" },    
};

static int gpio0 = 2;
static int gpio1 = 3;
static int gpio2 = 4;

static short int button_irq = 0;
static unsigned long flags = 0;
static int led_trigger = 0;

static irqreturn_t button_isr(int irq, void *data)
{
    irq_num = irq_num ? (0) : (1);
	local_irq_save(flags);
	printk("button_isr !!!!\n");	
	gpio_set_value(gpio0, led_trigger);
	local_irq_restore(flags);

	return IRQ_HANDLED;
}


// Command line paramaters for gpio pin and driver major number
module_param(gpio0, int, S_IRUGO);
MODULE_PARM_DESC(gpio0, "GPIO-0 pin to use");
module_param(gpio1, int, S_IRUGO);
MODULE_PARM_DESC(gpio1, "GPIO-1 pin to use");
module_param(gpio2, int, S_IRUGO);
MODULE_PARM_DESC(gpio2, "GPIO-2 pin to use");

// Forward declarations
static ssize_t write_LED( struct file *, const char *,size_t,loff_t *);
//Operations that can be performed on the device
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = write_LED
};

static ssize_t write_LED( struct file *filp, const char *buf,size_t count,loff_t *f_pos){
    char kbuf[BUF_SIZE];
    unsigned int len=1, gpio;
    gpio = iminor(filp->f_path.dentry->d_inode);
    printk(KERN_INFO LED_DRIVER_NAME " GPIO: LED_%d is modified. \n", gpio);
    len = count < BUF_SIZE ? count-1 : BUF_SIZE-1;
    if(copy_from_user(kbuf, buf, len) != 0) return -EFAULT;
    kbuf[len] = '\0';
    printk(KERN_INFO LED_DRIVER_NAME " Request from user: %s\n", kbuf);
    if (strcmp(kbuf, "1") == 0) {
	printk(KERN_ALERT LED_DRIVER_NAME " LED_%d switch On \n", gpio);
	gpio_set_value(leds[gpio].gpio, 1);
    } else if (strcmp(kbuf, "0") == 0) {
	printk(KERN_ALERT LED_DRIVER_NAME " LED_%d switch Off \n", gpio);
        gpio_set_value(leds[gpio].gpio, 0);
    }
    msleep(100);
    return count;
}

static void blink(void)
{
    int i;

    printk(KERN_INFO LED_DRIVER_NAME " %s\n", __func__);

    for(i = 0; i < ARRAY_SIZE(leds); i++) {
        if(i - 1 >= 0) {
            gpio_set_value(leds[i - 1].gpio, 0); 
        }
        gpio_set_value(leds[i].gpio, 1); 
        msleep(500);
    }

    gpio_set_value(leds[i - 1].gpio, 0); 
    printk(KERN_INFO LED_DRIVER_NAME " blink ended\n");
}

static void modify_gpio(void) {
    int i;
    leds[0].gpio = gpio0;
    leds[1].gpio = gpio1;
    leds[2].gpio = gpio2;
    for (i=0; i<ARRAY_SIZE(leds); ++i) {
	printk(KERN_INFO LED_DRIVER_NAME " Set %s to GPIO %i \n", leds[i].label, leds[i].gpio);
    }
}

static int __init LED_init_module(void)
{
    int ret, i;

    // Set gpio according to the parameters you give
    printk(KERN_INFO LED_DRIVER_NAME " %s\n", __func__);
    modify_gpio();

    printk(KERN_INFO LED_DRIVER_NAME " gpio_request_array \n");
    ret = gpio_request_array(leds, ARRAY_SIZE(leds));
    if(ret < 0)
    {
	printk(KERN_ERR LED_DRIVER_NAME " Unable to request GPIOs: %d\n", ret);
	goto exit_gpio_request;
    }
    //buton register

    if((BUTTON,"BUTTON") < 0) return -1;
    printk("gpio request button ok\n");
    if(gpio_is_valid(BUTTON) < 0) return -1;
    printk("gpio valid ok\n");
    if( (button_irq = gpio_to_irq(BUTTON)) < 0 )  return -1;
    printk("irq make ok \n");
    if( request_irq( button_irq, button_isr ,IRQF_TRIGGER_RISING, MY_GPIO_INT_NAME, MY_DEV_NAME)) return -1;
    printk("request irq gen ok\n");



    // Get driver number
    printk(KERN_INFO LED_DRIVER_NAME " alloc_chrdev_region \n");
    ret=alloc_chrdev_region(&driverno,0,ARRAY_SIZE(leds),LED_DRIVER_NAME);
    if (ret) {
	printk(KERN_EMERG LED_DRIVER_NAME " alloc_chrdev_region failed\n");
	goto exit_gpio_request;
    }
    printk(KERN_INFO LED_DRIVER_NAME " DRIVER No. of %s is %d\n", LED_DRIVER_NAME, MAJOR(driverno));

    printk(KERN_INFO LED_DRIVER_NAME " cdev_alloc\n");
    gpio_cdev = cdev_alloc();
    if(gpio_cdev == NULL)
    {
        printk(KERN_EMERG LED_DRIVER_NAME " Cannot alloc cdev\n");
	ret = -ENOMEM;
        goto exit_unregister_chrdev;
    }

    printk(KERN_INFO LED_DRIVER_NAME " cdev_init\n");
    cdev_init(gpio_cdev,&fops);
    gpio_cdev->owner=THIS_MODULE;

    printk(KERN_INFO LED_DRIVER_NAME " cdev_add\n");
    ret=cdev_add(gpio_cdev,driverno,ARRAY_SIZE(leds));

    if (ret)
    {
	printk(KERN_EMERG LED_DRIVER_NAME " cdev_add failed!\n");
	goto exit_cdev;
    }

    printk(KERN_INFO LED_DRIVER_NAME " Play blink\n");
    blink();

    printk(KERN_INFO LED_DRIVER_NAME " Create class \n");
    gpio_class = class_create(THIS_MODULE, LED_DRIVER_NAME);

    if (IS_ERR(gpio_class))
    {
	printk(KERN_ERR LED_DRIVER_NAME " class_create failed\n");
	ret = PTR_ERR(gpio_class);
	goto exit_cdev;
    }

    printk(KERN_INFO LED_DRIVER_NAME " Create device \n");
    for (i=0; i<ARRAY_SIZE(leds); ++i) {
	if (device_create(gpio_class,NULL, MKDEV(MAJOR(driverno), MINOR(driverno)+i), NULL,leds[i].label)==NULL) {
	    printk(KERN_ERR LED_DRIVER_NAME " device_create failed\n");
            ret = -1;
	    goto exit_cdev;
	}
    }

    return 0;

exit_cdev:
    cdev_del(gpio_cdev);

exit_unregister_chrdev:
    unregister_chrdev_region(driverno, ARRAY_SIZE(leds));

exit_gpio_request:
    gpio_free_array(leds, ARRAY_SIZE(leds));
    return ret;
}

/*
 * Module exit function
 */
static void __exit LED_exit_module(void)
{
    int i;
    printk(KERN_INFO LED_DRIVER_NAME " %s\n", __func__);
    // turn all off
    for(i = 0; i < ARRAY_SIZE(leds); i++) {
	gpio_set_value(leds[i].gpio, 0);
	device_destroy(gpio_class, MKDEV(MAJOR(driverno), MINOR(driverno) + i));
    }

    class_destroy(gpio_class);
    cdev_del(gpio_cdev);
    unregister_chrdev_region(driverno, ARRAY_SIZE(leds));
    gpio_free_array(leds, ARRAY_SIZE(leds));
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wan-Ting CHEN");
MODULE_DESCRIPTION("LED Kernel module");
module_init(LED_init_module);
module_exit(LED_exit_module);
