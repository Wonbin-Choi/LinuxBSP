#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_FILENAME  "/dev/calldev"

int main(void){
	int dev;
	char buff[128];
	int ret;
	int key_data[2] = {0};

	printf("1) device file open\n");
	
	dev = open(DEVICE_FILENAME, O_RDWR | O_NDELAY);
	printf("dev : %d\n",dev);
	if(dev >= 0){
		printf("2) seek function call dev:%d\n",dev);
		ret = lseek(dev,0x20,SEEK_SET);
		printf("ret = %08X\n",ret);
		printf("3) read function call\n");
		//ret = read(dev,(char *)0x30,0x31);
		//printf("ret = %08X\n",ret);
		printf("4) write function call\n");
		do{
			key_data[0]=read(dev,buff,1);
			if(key_data[0] != key_data[1])
                {		
                    key_data[1] = key_data[0];
                    if(key_data)
                    {
						ret = write(dev,(char*)key_data,1);
                        puts("0:1:2:3:4:5:6:7");
                        for(int i=0;i<8;i++)
						{
			               if(key_data[0] & (0x01 << i))
							   putchar('O');
					        else
						        putchar('X');
							if(i != 7 )
                                putchar(':');
                            else
                                putchar('\n');
                        }
						putchar('\n');
                    }
                    if(key_data[0] == 0x80)        
						break;
				}
		}while(1);
		//ret = write(dev,buff,1);
		//ret = write(dev,(char *)0x40,0x41);
		printf("ret = %08X\n",ret);
		printf("5) ioctl function call\n");
		ret = ioctl(dev,0x51,0x52);
		printf("ret = %08X\n",ret);
		printf("6) device file close\n");
		ret = close(dev);
		printf("ret = %08X\n",ret);
	}
	else
		perror("open");
	return 0;
}
