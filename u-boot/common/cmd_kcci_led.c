#include <common.h>
#include <command.h>
#include <asm/io.h>

#define BCM2711_GPIO_GPFSEL0 0xFE200000
#define BCM2711_GPIO_GPFSEL1 0xFE200004
#define BCM2711_GPIO_GPFSEL2 0xFE200008
#define BCM2711_GPIO_GPSET0 0xFE20001C
#define BCM2711_GPIO_GPCLR0 0xFE200028
#define BCM2711_GPIO_GPLEV0 0xFE200034

#define GPIO6_9_MASK_BIT 0x0003ffff
#define GPIO10_13_MASK_BIT 0xfffff000
#define GPIO6_13_MASK_SET_CLR 0x00003fc0

#define GPIO6_9_SIG_OUTPUT 0x09240000
#define GPIO10_13_SIG_OUTPUT 0x00000249

#define GPIO16_19_SIG_INPUT 0x00000000
#define GPIO20_23_SIG_INPUT 0x00000000

#define GPIO16_19_MASK_BIT 0x0003ffff
#define GPIO20_23_MASK_BIT 0xfffff000


void key_control(unsigned long * key_data);

void led_init(void){
	u32 temp = readl(BCM2711_GPIO_GPFSEL0);
	temp = temp & GPIO6_9_MASK_BIT;
	temp = temp | GPIO6_9_SIG_OUTPUT;
	writel(temp,BCM2711_GPIO_GPFSEL0);

	temp = readl(BCM2711_GPIO_GPFSEL1);
	temp = temp & GPIO10_13_MASK_BIT;
	temp = temp | GPIO10_13_SIG_OUTPUT;
	writel(temp,BCM2711_GPIO_GPFSEL1);
}

void led_write(unsigned long led_data){	
	writel(GPIO6_13_MASK_SET_CLR,BCM2711_GPIO_GPCLR0);
	led_data = led_data << 6;
	writel(led_data,BCM2711_GPIO_GPSET0);
}

void key_init(void){
	u32 temp = readl(BCM2711_GPIO_GPFSEL1);
	temp &= GPIO16_19_MASK_BIT;
	temp |= GPIO16_19_SIG_INPUT;
	writel(temp, BCM2711_GPIO_GPFSEL1); // GPIO 16~19 input

	temp = readl(BCM2711_GPIO_GPFSEL2);
	temp &= GPIO20_23_MASK_BIT;
	temp |= GPIO20_23_SIG_INPUT;
	writel(temp, BCM2711_GPIO_GPFSEL2); // GPIO 20~23 input
}

void key_read(unsigned long * key_data){
	*key_data = readl(BCM2711_GPIO_GPLEV0);
	*key_data = (*key_data >> 16) & 0xff;
}

static int do_KCCI_LED(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[]){
	unsigned long led_data;
	unsigned long key_data;

	if(argc != 2){
		cmd_usage(cmdtp);
		return 1;
	}

	printf("*LED TEST START!\n");
	led_init();
	key_init();
	led_data = simple_strtoul(argv[1],NULL,16);
	led_write(led_data);
	key_control(&key_data);

	writel(GPIO6_13_MASK_SET_CLR,BCM2711_GPIO_GPCLR0);
	printf("*LED TEST END(%s : %#04x)\n\n","key",(unsigned int)key_data);

	return 0;
}

U_BOOT_CMD(
		led,2,0,do_KCCI_LED,
		"led - kcci LED Test.",
		"number - Input argument is only one.(led [0x00~0xff)\n");

void key_control(unsigned long * key_data){
	unsigned long key_data_old = 0;
	unsigned long key_data_toggle = 0;
	int i;

	do{	
		key_read(key_data);
		if(*key_data != key_data_old){
			if(*key_data){
				key_data_toggle ^= *key_data;
				puts("0:1:2:3:4:5:6:7\n");
				for(i = 0;i<8;i++){
					if(key_data_toggle & (1 << i)) putc('O');
					else putc('X');
					if(i != 7) putc(':');
					else putc('\n');
				}
				led_write(key_data_toggle);
			}
			key_data_old = *key_data;
		}
	}while(*key_data != 0x80);	
}
