#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

static int onevalue = 1;
static char *twostring = NULL;

module_param(onevalue,int,0);
module_param(twostring,charp,0);

static int gpioLedInit(void);
static void gpioLedSet(long val);
static void gpioLedFree(void);

static int gpioKeyInit(void);
static int gpioKeyGet(void);
static void gpioKeyFree(void);

static char gpioName[10];
static int gpioLed[] = {6,7,8,9,10,11,12,13};
static int gpioKey[] = {16,17,18,19,20,21,22,23};

static int led_init(void){
	int ret = 0;
	printk("Hello world [onevalue=%d,twostring=%s] \n",onevalue,twostring);
	ret = gpioLedInit();
    if(ret<0) return ret;

	gpioLedSet(onevalue);
	/*
    gpioLedFree();

    ret = gpioKeyInit();
    if(ret<0) return ret;

    val = gpioKeyGet();
    gpioKeyFree();*/
	return 0;	
}

static int gpioLedInit(void){
	int i;
	int ret=0;
	for(i=0;i<8;i++){
		sprintf(gpioName,"led%d",i);
		ret = gpio_request(gpioLed[i],gpioName);
		if(ret < 0){
			printk("Failed request gpio%d error\n",gpioLed[i]);
			return ret;
		}
	}
	for(i=0;i<8;i++){
		ret = gpio_direction_output(gpioLed[i],0);
		if(ret < 0){
			printk("Failed direction_output gpio%d error\n",gpioLed[i]);
			return ret;
		}
	}
	return ret;
}

static void gpioLedSet(long val){
	int i;
	for(i=0;i<8;i++)
		gpio_set_value(gpioLed[i],val & (0x01 << i));
}

static void gpioLedFree(void){
	int i;
	for(i=0;i<8;i++){
		gpio_set_value(gpioLed[i],0);
		gpio_free(gpioLed[i]);
	}
}

static int gpioKeyInit(void){
	int i;
	int ret=0;
	for(i=0;i<8;i++){
		sprintf(gpioName,"key%d",i);
		ret = gpio_request(gpioKey[i],gpioName);
		if(ret < 0){
			printk("Failed request gpio%d error\n",gpioKey[i]);
			return ret;
		}
	}
	for(i=0;i<8;i++){
		ret = gpio_direction_input(gpioKey[i]);
		if(ret < 0){
			printk("Failed direction_output gpio%d error\n",gpioKey[i]);
			return ret;
		}
	}
	return ret;
}
static int gpioKeyGet(void){
	int sum = 0;
	int i;
	for(i=7;i>=0;i--){
		sum += gpio_get_value(gpioKey[i]) << i;
	//	sum =  sum | (gpio_get_value(gpioKey[i]) << i);	
	}
	return sum;
}
static void gpioKeyFree(void){
	int i;
	for(i=0;i<8;i++)
		gpio_free(gpioKey[i]);
}

module_init(led_init);
module_exit(gpioLedFree);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Module Parameter Test Module");
MODULE_AUTHOR("KCCI");
