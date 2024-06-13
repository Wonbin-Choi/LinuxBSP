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
#include <linux/gpio.h>
#include <linux/moduleparam.h>

#define DEBUG 1

static int timerVal = 100;
module_param(timerVal,int,0);
static int ledVal = 0;
module_param(ledVal,int,0);
struct timer_list timerLed;

void kerneltimer_func(struct timer_list *t);
void kerneltimer_registertimer(unsigned long timerover){
	timer_setup(&timerLed, kerneltimer_func, 0);
	timerLed.expires = get_jiffies_64() + timerover;
	add_timer(&timerLed);
}
void kerneltimer_func(struct timer_list *t){
#if DEBUG
	printk("ledVal ; %#04x\n",(unsigned int)(ledVal));
#endif
	ledVal = ~ledVal & 0xff;
	mod_timer(t,get_jiffies_64() + timerVal);
}

int kerneltimer_init(void){
#if DEBUG
	printk("timerVal : %d , sec : %d \n",timerVal, timerVal/HZ);
#endif
	kerneltimer_registertimer(timerVal);
	return 0;
}
void kerneltimer_exit(void){
	if(timer_pending(&timerLed))
		del_timer(&timerLed);
}
module_init(kerneltimer_init);
module_exit(kerneltimer_exit);
MODULE_AUTHOR("KCCI-AIOT");
MODULE_DESCRIPTION(	"LED KET TEST MODULE");
MODULE_LICENSE("Dual BSD/GPL");
