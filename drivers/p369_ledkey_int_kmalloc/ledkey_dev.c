#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>          
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/fcntl.h>       
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

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
}keyDataStruct;

static int  gpioLedInit(void);
static void gpioLedSet(long);
static void gpioLedFree(void);
static int  gpioKeyInit(void);
//static int gpioKeyGet(void);
static void gpioKeyFree(void);
static int gpioKeyIrqInit(keyDataStruct * pKeyData);
static void gpioKeyIrqFree(keyDataStruct * pKeyData);

irqreturn_t key_isr(int irq,void * data){
	keyDataStruct * pKeyData = (keyDataStruct *)data;
	for(int i=0;i<GPIOKEYCNT;i++){
		if(irq == pKeyData->key_irq[i]){
			pKeyData->keyNumber = i+1;
			break;
		}
	}
#if DEBUG
	printk("key_isr() irq : %d, keyNumber : %d\n",irq,pKeyData->keyNumber);
#endif
	return IRQ_HANDLED;
}

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
		//gpio_set_value(gpioLed[i],(val & (0x01<<i)));
		gpio_set_value(gpioLed[i],(0x01<<(val-1)) & (0x01<<i));
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
			printk("Failed gpio_to_irq%d error\n", gpioKey[i]);
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
}*/
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

int ledkey_open (struct inode *inode, struct file *filp)
{
	int i, result;
    int num0 = MAJOR(inode->i_rdev); 
    int num1 = MINOR(inode->i_rdev); 
	char * irqName[GPIOKEYCNT] = {"irqkey0","irqkey1","irqkey2","irqkey3","irqkey4","irqkey5","irqkey6","irqkey7"};
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
	for(i=0;i<GPIOKEYCNT;i++){
		result = request_irq(pKeyData->key_irq[i],key_isr,IRQF_TRIGGER_RISING,irqName[i],pKeyData);
		if(result < 0){
			printk("request_irq() failed irq %d\n",pKeyData->key_irq[i]);
			return result;
		}	
	}

	filp->private_data = pKeyData;
    return 0;
}

ssize_t ledkey_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	char kbuf;
	keyDataStruct * pKeyData = (keyDataStruct *)filp->private_data;
//	int ret;
//    printk( "ledkey read -> buf : %08X, count : %08X \n", (unsigned int)buf, count );
//	kbuf = gpioKeyGet();
	kbuf = pKeyData->keyNumber;
	put_user(kbuf,buf);
	if(pKeyData->keyNumber != 0)
		pKeyData->keyNumber = 0;
//	for(int i=0;i<count;i++)
//		put_user(kbuf[i],buf++);
//	ret = copy_to_user(buf,&kbuf,count);
    return count;
}

ssize_t ledkey_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	char kbuf;
//	int ret;
	get_user(kbuf,buf);
  //  printk( "ledkey write -> buf : %08X, count : %08X  %d\n", (unsigned int)buf, count, *buf );
//	for(int i=0;i<count;i++)
//		get_user(kbuf[i],buf++);
//	ret = copy_from_user(kbuf,buf,count);
	gpioLedSet(kbuf);

    return count;
}

static long ledkey_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{

//    printk( "ledkey ioctl -> cmd : %08X, arg : %08X \n", cmd, (unsigned int)arg );
    return 0x53;
}

int ledkey_release (struct inode *inode, struct file *filp)
{
    printk( "ledkey release \n" );
	module_put(THIS_MODULE);
	gpioKeyIrqFree(filp->private_data);
	if(filp->private_data)
		kfree(filp->private_data);
    return 0;
}

struct file_operations ledkey_fops =
{
//    .owner    = THIS_MODULE,
    .open     = ledkey_open,     
    .read     = ledkey_read,     
    .write    = ledkey_write,    
	.unlocked_ioctl = ledkey_ioctl,
   // .llseek   = ledkey_llseek,   
    .release  = ledkey_release,  
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
	gpioKeyFree();
	gpioLedFree();
}

module_init(ledkey_init);
module_exit(ledkey_exit);

MODULE_LICENSE("Dual BSD/GPL");
