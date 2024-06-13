#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>          
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/fcntl.h>       
#include <linux/gpio.h>

#define CALL_DEV_NAME            "call_dev"
#define CALL_DEV_MAJOR            230      
#define OFF 0
#define ON 1
#define GPIOLEDCNT 8
#define GPIOKEYCNT 8

static int gpioLed[GPIOLEDCNT] = {6,7,8,9,10,11,12,13};
static int gpioKey[GPIOKEYCNT] = {16,17,18,19,20,21,22,23};

static int  gpioLedInit(void);
static void gpioLedSet(long);
static void gpioLedFree(void);
static int  gpioKeyInit(void);
static int gpioKeyGet(void);
static void gpioKeyFree(void);

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
		gpio_set_value(gpioLed[i],(val & (0x01<<i)));
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
static void gpioKeyFree(void)
{
	int i;
	for(i=0;i<GPIOKEYCNT;i++)
	{
		gpio_free(gpioKey[i]);
	}
}

int call_open (struct inode *inode, struct file *filp)
{
    int num0 = MAJOR(inode->i_rdev); 
    int num1 = MINOR(inode->i_rdev); 
    printk( "call open -> major : %d\n", num0 );
    printk( "call open -> minor : %d\n", num1 );
	try_module_get(THIS_MODULE);
    return 0;
}

loff_t call_llseek (struct file *filp, loff_t off, int whence )
{
    printk( "call llseek -> off : %08X, whenec : %08X\n", (unsigned int)off, whence );
    return 0x23;
}

ssize_t call_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	char kbuf[128];
	int ret;
    printk( "call read -> buf : %08X, count : %08X \n", (unsigned int)buf, count );
	kbuf[0] = gpioKeyGet();
//	for(int i=0;i<count;i++)
//		put_user(kbuf[i],buf++);
	ret = copy_to_user(buf,kbuf,count);
    return count;
}

ssize_t call_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	char kbuf[128];
	int ret;
    printk( "call write -> buf : %08X, count : %08X  %d\n", (unsigned int)buf, count, *buf );
//	gpioLedSet(*buf);
//	for(int i=0;i<count;i++)
//		get_user(kbuf[i],buf++);
	ret = copy_from_user(kbuf,buf,count);
	gpioLedSet(kbuf[0]);

    return count;
}

//int call_ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
static long call_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{

    printk( "call ioctl -> cmd : %08X, arg : %08X \n", cmd, (unsigned int)arg );
    return 0x53;
}

int call_release (struct inode *inode, struct file *filp)
{
    printk( "call release \n" );
	module_put(THIS_MODULE);
    return 0;
}

struct file_operations call_fops =
{
//    .owner    = THIS_MODULE,
    .open     = call_open,     
    .read     = call_read,     
    .write    = call_write,    
	.unlocked_ioctl = call_ioctl,
    .llseek   = call_llseek,   
    .release  = call_release,  
};

int call_init(void)
{
    int result;

    printk( "call call_init \n" );    

    result = register_chrdev( CALL_DEV_MAJOR, CALL_DEV_NAME, &call_fops);
    if (result < 0) return result;

	result = gpioLedInit();
	if(result < 0)
        return result;
	result = gpioKeyInit();
	if(result < 0)
        return result;
    return 0;
}

void call_exit(void)
{
    printk( "call call_exit \n" );    
    unregister_chrdev( CALL_DEV_MAJOR, CALL_DEV_NAME );
	gpioLedFree();
	gpioKeyFree();
}

module_init(call_init);
module_exit(call_exit);

MODULE_LICENSE("Dual BSD/GPL");
