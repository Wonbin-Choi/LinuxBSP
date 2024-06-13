#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>          
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/fcntl.h>       
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/moduleparam.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include "ioctl_test.h"

#define LEDKEY_DEV_NAME            "ledkey_dev"
#define LEDKEY_DEV_MAJOR            230      
#define OFF 0
#define ON 1
#define GPIOLEDCNT 8
#define GPIOKEYCNT 8
#define DEBUG 1
static int gpioLed[GPIOLEDCNT] = {6,7,8,9,10,11,12,13};
static int gpioKey[GPIOKEYCNT] = {16,17,18,19,20,21,22,23};
//static int key_irq[GPIOKEYCNT];
//static int keyNumber=0;
typedef struct {
	int keyNumber;
	int key_irq[GPIOKEYCNT];
} keyDataStruct;

static unsigned long timerVal;
unsigned long ledVal;
struct timer_list timerLed;

static char flag = 0;

static int  gpioLedInit(void);
static void gpioLedSet(long);
static void gpioLedFree(void);
static int  gpioKeyInit(void);
//static int gpioKeyGet(void);
static void gpioKeyFree(void);
static int gpioKeyIrqInit(keyDataStruct * pKeyData);
static void gpioKeyIrqFree(keyDataStruct * pKeyData);
irqreturn_t key_isr(int irq, void * data);
DECLARE_WAIT_QUEUE_HEAD(WaitQueue_Read);
static __poll_t ledkey_poll(struct file * filp, struct poll_table_struct * wait);

static void kerneltimer_func(struct timer_list *t);
static void kerneltimer_registertimer(unsigned long timerover);
static int kerneltimer_init(void);
static void kerneltimer_exit(void);

static int gpioLedInit(void)
{
	int i;
	int ret=0;
	char gpioName[10];
	for(i=0;i<GPIOLEDCNT;i++)
	{
		sprintf(gpioName,"led%d",i);
		ret = gpio_request(gpioLed[i],gpioName);
		if(ret < 0) {
			printk("Failed gpio_request() gpio%d error \n",gpioLed[i]);
			return ret;
		}

		ret = gpio_direction_output(gpioLed[i],OFF);
		if(ret < 0) {
			printk("Failed gpio_direction_output() gpio%d error \n",gpioLed[i]);
			return ret;
		}
	}
	return ret;
}

static void gpioLedSet(long val)
{
	int i;
	for(i=0;i<GPIOLEDCNT;i++)
	{
//		gpio_set_value(gpioLed[i],(val>>i) & 0x1);
//		gpio_set_value(gpioLed[i],(val & (0x01<<i)));
		gpio_set_value(gpioLed[i],(val>>i) & 0x1);
	}
}
static void gpioLedFree(void)
{
	int i;
	for(i=0;i<GPIOLEDCNT;i++)
	{
		gpio_free(gpioLed[i]);
	}
}
static int gpioKeyInit(void)
{
	int i;
	int ret=0;
	char gpioName[10];
	for(i=0;i<GPIOKEYCNT;i++)
	{
		sprintf(gpioName,"key%d",i);
		ret = gpio_request(gpioKey[i], gpioName);
		if(ret < 0) {
			printk("Failed Request gpio%d error\n", gpioKey[i]);
			return ret;
		}
	}
	for(i=0;i<GPIOKEYCNT;i++)
	{
		ret = gpio_direction_input(gpioKey[i]);
		if(ret < 0) {
			printk("Failed direction_output gpio%d error\n", gpioKey[i]);
       	 	return ret;
		}
	}
	return ret;
}
static int gpioKeyIrqInit(keyDataStruct * pKeyData)
{
	int i;
	for(i=0;i<GPIOKEYCNT;i++)
	{
		pKeyData->key_irq[i] = gpio_to_irq(gpioKey[i]);
		if(pKeyData->key_irq[i] < 0) {
			printk("Failed gpio_to_irq() gpio%d error\n", gpioKey[i]);
			return pKeyData->key_irq[i];
		}
	}
	return 0;
}
/*
static int	gpioKeyGet(void)
{
	int i;
	int ret;
	int keyData=0;
	for(i=0;i<GPIOKEYCNT;i++)
	{
//		ret=gpio_get_value(gpioKey[i]) << i;
//		keyData |= ret;
		ret=gpio_get_value(gpioKey[i]);
		keyData = keyData | ( ret << i );
	}
	return keyData;
}
*/
static void gpioKeyFree(void)
{
	int i;
	for(i=0;i<GPIOKEYCNT;i++)
	{
		gpio_free(gpioKey[i]);
	}
}

static void gpioKeyIrqFree(keyDataStruct * pKeyData)
{
	int i;
	for(i=0;i<GPIOKEYCNT;i++)
	{
		free_irq(pKeyData->key_irq[i],pKeyData);
	}
}

static void kerneltimer_registertimer(unsigned long timerover){
    timer_setup(&timerLed, kerneltimer_func, 0);
    timerLed.expires = get_jiffies_64() + timerover;
    add_timer(&timerLed);
}
static void kerneltimer_func(struct timer_list *t){
#if DEBUG
    printk("ledVal ; %#04x\n",(unsigned int)(ledVal));
#endif
    gpioLedSet(ledVal);
    ledVal = ~ledVal & 0xff;
    mod_timer(t,get_jiffies_64() + timerVal);
}

static int kerneltimer_init(void){
#if DEBUG
    printk("timerVal : %ld , sec : %ld \n",timerVal, timerVal/HZ);
#endif
    kerneltimer_registertimer(timerVal);
    return 0;
}
static void kerneltimer_exit(void){
    if(timer_pending(&timerLed))
        del_timer(&timerLed);
}

int ledkey_open (struct inode *inode, struct file *filp)
{
	int i;
	int result;
    int num0 = MAJOR(inode->i_rdev); 
    int num1 = MINOR(inode->i_rdev); 
	char * irqName[GPIOKEYCNT] = {"irqKey0","irqKey1","irqKey2","irqKey3","irqKey4","irqKey5","irqKey6","irqKey7"};
	keyDataStruct * pKeyData;
    printk( "ledkey open -> major : %d\n", num0 );
    printk( "ledkey open -> minor : %d\n", num1 );
	try_module_get(THIS_MODULE);

	pKeyData = (keyDataStruct *)kmalloc(sizeof(keyDataStruct),GFP_KERNEL);
	if(!pKeyData)
		return -ENOMEM;
	pKeyData->keyNumber = 0;

	result = gpioKeyIrqInit(pKeyData);
	if(result < 0)
		return result;

	for(i=0;i<GPIOKEYCNT;i++)
	{
		result = request_irq(pKeyData->key_irq[i],key_isr,IRQF_TRIGGER_RISING,irqName[i],pKeyData);
		if(result < 0)
		{
			printk("request_irq() failed irq %d\n",pKeyData->key_irq[i]);
			return result;
		}
	}

	filp->private_data = pKeyData;
    return 0;
}

ssize_t ledkey_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
//	int i;
//	char kbuf[128];
//	char kbuf;
	keyDataStruct * pKeyData = (keyDataStruct *)filp->private_data;
//    printk( "ledkey read -> buf : %08X, count : %08X \n", (unsigned int)buf, count );
//	kbuf = gpioKeyGet();


	if(pKeyData->keyNumber == 0)
	{
		if(!(filp->f_flags & O_NONBLOCK))
		{
			wait_event_interruptible(WaitQueue_Read,pKeyData->keyNumber);
		}
		else return 0;
	}

 	put_user(pKeyData->keyNumber,buf);
	if(pKeyData->keyNumber != 0)
		pKeyData->keyNumber = 0;
//	for(i=0;i<count;i++)
//		put_user(kbuf[i],buf++);
//	i=copy_to_user(buf,kbuf,count);
//	i=copy_to_user(buf,&kbuf,count);
/*	if (i != 0) {
        return -EFAULT;
    }
*/
    return count;
}

ssize_t ledkey_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
//	int i;
	char kbuf;
//	char kbuf[128];
//    printk( "ledkey write -> buf : %08X, count : %08X  %d\n", (unsigned int)buf, count, *buf );
//	for(i=0;i<count;i++)
//		get_user(kbuf[i],&buf[i]);	
//	i=copy_from_user(kbuf,buf,count);
/*	if (i != 0) {
        return -EFAULT;
    }
*/
#if DEBUG
	printk("ledkey write -> buf : %08X, count : %08X \n",(unsigned int)buf, count);
#endif
	get_user(kbuf,buf);
	ledVal = kbuf;
//	gpioLedSet(ledVal);
    return count;
}

static __poll_t ledkey_poll(struct file * filp, struct poll_table_struct * wait){

    unsigned int mask=0;
    keyDataStruct * pKeyData = (keyDataStruct *)filp->private_data;
#ifdef DEBUG
    printk("_key : %u\n",(wait->_key & POLLIN));
#endif
    if(wait->_key & POLLIN)
        poll_wait(filp, &WaitQueue_Read, wait);
    if(pKeyData->keyNumber > 0)
        mask = POLLIN;

    return mask;

}


static long ledkey_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	keyled_data data;
	int err=0, size;
	if( _IOC_TYPE( cmd ) != IOCTLTEST_MAGIC ) return -EINVAL;
	if( _IOC_NR( cmd ) >= IOCTLTEST_MAXNR ) return -EINVAL;

	size = _IOC_SIZE( cmd );
    if( size )
    {
        if( _IOC_DIR( cmd ) & _IOC_READ )
            err = access_ok( (void *) arg, size );
        if( _IOC_DIR( cmd ) & _IOC_WRITE )
            err = access_ok( (void *) arg, size );
        if( !err ) return err;
    }

	switch( cmd )
    {
        case TIMER_START :
			if(!flag)
				kerneltimer_init();
			flag = 1;
            break;
        case TIMER_STOP :
			if(flag)
				kerneltimer_exit();
			flag = 0;
            break;
        case TIMER_VALUE :
			if (copy_from_user(&data, (keyled_data __user *)arg, sizeof(data)))
                return -EFAULT;
			timerVal = data.timer_val;
            break;
		default:
            err =-E2BIG;
            break;
	}
	return err;
}

int ledkey_release (struct inode *inode, struct file *filp)
{
    printk( "ledkey release \n" );
	module_put(THIS_MODULE);
	gpioKeyIrqFree(filp->private_data);
	if(filp->private_data)
		kfree(filp->private_data);
	if(timer_pending(&timerLed)){
		if(flag)  del_timer(&timerLed);
		flag = 0;
	}
    gpioLedFree();
    return 0;
}

irqreturn_t key_isr(int irq, void * data)
{
	int i;
	keyDataStruct * pKeyData = (keyDataStruct *)data;
	for(i=0;i<GPIOKEYCNT;i++)
	{
		if(irq == pKeyData->key_irq[i])
		{
			pKeyData->keyNumber = i+1;
			break;
		}
	}
#if DEBUG
	printk("key_isr() irq : %d, keyNumber : %d\n",irq,pKeyData->keyNumber);
#endif
	wake_up_interruptible(&WaitQueue_Read);
	return IRQ_HANDLED;
}
struct file_operations ledkey_fops =
{
//    .owner    = THIS_MODULE,
    .open     = ledkey_open,     
    .read     = ledkey_read,     
    .write    = ledkey_write,    
	.unlocked_ioctl = ledkey_ioctl,
    .release  = ledkey_release,  
	.poll	  = ledkey_poll,
};

int ledkey_init(void)
{
    int result;

    printk( "ledkey ledkey_init \n" );    

    result = register_chrdev( LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME, &ledkey_fops);
    if (result < 0) return result;

	result = gpioLedInit();
	if(result < 0)
        return result;

	result = gpioKeyInit();
	if(result < 0)
        return result;

    return 0;
}

void ledkey_exit(void)
{
    printk( "ledkey ledkey_exit \n" );    
    unregister_chrdev( LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME );
	gpioLedFree();
	gpioKeyFree();
}

module_init(ledkey_init);
module_exit(ledkey_exit);
MODULE_AUTHOR("KCCI-AIOT");
MODULE_DESCRIPTION("ledkey module");
MODULE_LICENSE("Dual BSD/GPL");
