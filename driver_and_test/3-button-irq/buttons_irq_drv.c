
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/device.h>

	





static unsigned int major;
static struct class *buttons_class;
static struct class_device *buttons_class_devices;

volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);
static volatile int ev_press = 0;



static unsigned char key_val;

/* 键值: 按下时, 0x01, 0x02, 0x03, 0x04 */
/* 键值: 松开时, 0x81, 0x82, 0x83, 0x84 */
static struct pin_desc pins_desc[4] = {
	{S3C2410_GPF0, 0x01},
	{S3C2410_GPF2, 0x02},
	{S3C2410_GPG3, 0x03},
	{S3C2410_GPG11,0x04},
};

static ssize_t buttons_irq_drv_read (struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	if (count != 1)
		return -EINVAL;

	wait_event_interruptible(button_waitq,ev_press);
	
	if (copy_to_user(buf,&key_val,1))
		return -EFAULT;

	ev_press = 0;

	return 1;	
}

static irqreturn_t buttons_irq(int irq, void *dev_id)
{
	struct pin_desc * pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval){
		//松开
		key_val = 0x80 | pindesc->key_val;	
	}else{
		//按下
		key_val = pindesc->key_val;
	}

	ev_press = 1;
	wake_up_interruptible(&button_waitq);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int buttons_irq_drv_open (struct inode *inode, struct file *file)
{
	printk("---  %s ---\r\n",__func__);
	
	request_irq(IRQ_EINT0,  buttons_irq, IRQT_BOTHEDGE, "S2", &pins_desc[0]);
	request_irq(IRQ_EINT2,  buttons_irq, IRQT_BOTHEDGE, "S3", &pins_desc[1]);
	request_irq(IRQ_EINT11, buttons_irq, IRQT_BOTHEDGE, "S4", &pins_desc[2]);
	request_irq(IRQ_EINT19, buttons_irq, IRQT_BOTHEDGE, "S5", &pins_desc[3]);	

	return 0;	
}

static struct file_operations buttons_irq_drv_fops = {
	.owner = THIS_MODULE,
	.open = buttons_irq_drv_open,
	.read = buttons_irq_drv_read,
};

static int __init buttons_irq_drv_init(void)
{
	printk("---  %s ---\r\n",__func__);
	
	major = register_chrdev(0,"buttons_irq_drv",&buttons_irq_drv_fops);

	buttons_class = class_create(THIS_MODULE,"buttons_irq_drv");
	buttons_class_devices = class_device_create(buttons_class,NULL,MKDEV(major,0),NULL,"buttons_irq");

	gpfcon = (volatile unsigned long *)ioremap(0x56000050,16);
	gpfdat = gpfcon + 1;

	gpgcon = (volatile unsigned long *)ioremap(0x56000060,16);
	gpgdat = gpgcon + 1;

	return 0;
}

static void __exit buttons_irq_drv_exit(void)
{
	printk("---  %s ---\r\n",__func__);
	
	unregister_chrdev(major,"buttons_irq_drv");
	class_device_unregister(buttons_class_devices);
	class_destroy(buttons_class);
	iounmap(gpfcon);
	iounmap(gpgcon);

	free_irq(IRQ_EINT0, &pins_desc[0]);
	free_irq(IRQ_EINT2, &pins_desc[1]);
	free_irq(IRQ_EINT11, &pins_desc[2]);
	free_irq(IRQ_EINT19, &pins_desc[3]);
}

module_init(buttons_irq_drv_init);
module_exit(buttons_irq_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhuangzebin");
MODULE_DESCRIPTION("Buttons test");
