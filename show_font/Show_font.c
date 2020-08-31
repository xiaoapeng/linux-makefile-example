#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/fb.h>


#include "font.h"




struct fb_var_screeninfo fb_var;	/* Current var */
struct fb_fix_screeninfo fb_fix;	/* Current fix */
int screen_size;
unsigned char *fbmem;		//显存的地址
unsigned int line_width;	//x方向的宽度 以字节为单位
unsigned int pixel_width;	//每一个像素的宽度 以字节为单位 

struct stat hzk_stat;
unsigned char *hzkmem;

void lcd_put_pixel(int x, int y, unsigned int color)
{
	unsigned char *pen_8 = fbmem+x*pixel_width+y*line_width;
	unsigned short *pen_16;	
	unsigned int *pen_32;	
	pen_16 = (unsigned short *) pen_8;
	pen_32 = (unsigned int *) pen_8;
	unsigned int rad,green,blue;
	switch (fb_var.bits_per_pixel)
	{
		case 8:
			*pen_8 = (char)color;
			break;
		case 16:
			rad = (color >> 16) & 0xff;
			green = (color >> 8) & 0xff;
			blue = (color >> 0) & 0xff;
			*pen_16 = ((rad>>3)<<(5+6))|((green>>2)<<5) | ( blue >> 3);
			break;
		case 32:
			*pen_32 = (char)color;
			break;
		default:
			printf("can't surport  %dbpp ",fb_var.bits_per_pixel);
			break;
	}

	
}

void lcd_put_ascii(int x, int y, unsigned char c)
{
	unsigned char *dots = (unsigned char *)&fontdata_8x16[c*16];
	int i, b;
	unsigned char byte;
	
	for(i=0;i<16;i++)
	{
		//每行
		byte = dots[i];
		for(b=0;b<8;b++)
		{
			//每列
			if(byte & 1<<(7-b))
			{
				/* show */
				lcd_put_pixel(x+b, y+i, 0xffffff); /* 白 */
			}
			else
			{
				lcd_put_pixel(x+b, y+i, 0x0); /* 黑色 */
			}
		}
	}

}


//在位图表中每个中文占 16*16/8 = 32(字节)
void lcd_put_chinese(int x, int y, unsigned char *str)
{
	unsigned int area = str[0]- 0xA1;
	unsigned int where = str[1] - 0xA1;
	unsigned short *dots =(unsigned short *) (hzkmem + (area * 94 + where)*32);
	int i, b;
	unsigned short word;
	for(i=0;i<16;i++)
	{
		//每行
		word = dots[i];
		word = (word << 8) | (word >> 8) ;
		for(b=0;b<16;b++)
		{
			//每列
			if(word & 1<<(15-b))
			{
				/* show */
				lcd_put_pixel(x+b, y+i, 0xffffff); /* 白 */
			}
			else
			{
				lcd_put_pixel(x+b, y+i, 0x0); /* 黑色 */
			}
		}
	}

}



int main(int argc,char **argv)
{
	int fb_fd;
	int fd_hzk16;
	unsigned char str[] = "哈";
	fb_fd=open("/dev/fb0",O_RDWR);
	if( fb_fd < 0 )
	{
		perror("can't open /dev/fb0\n/dev/fb0:");
		return -1;
	}
	if( ioctl(fb_fd,FBIOGET_VSCREENINFO,&fb_var) )
	{
		perror("can't get var\n/dev/fb0 ioctl:");
		return -1;
	}
	if( ioctl(fb_fd,FBIOGET_FSCREENINFO,&fb_fix) )
	{
		perror("can't get var\n/dev/fb0 ioctl:");
		return -1;
	}
	line_width = fb_var.xres * fb_var.bits_per_pixel / 8;
	pixel_width = fb_var.bits_per_pixel / 8;
	screen_size = fb_var.xres * fb_var.yres * fb_var.bits_per_pixel / 8;
	fbmem = (unsigned char *)mmap(NULL , screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(fbmem == (unsigned char*)-1)
	{
		perror("can't mmap fbmem\n/dev/fb0 mmap");
		return -1;
	}
	
	fd_hzk16 = open("HZK16", O_RDONLY);	//只读
	if(fd_hzk16<0)
	{
		perror("can't open HZK16");
		return -1;
	}
	if(fstat(fd_hzk16, &hzk_stat))
	{
		perror("can't get fstat");
		return -1;
	}
	hzkmem =  (unsigned char *)mmap(NULL , hzk_stat.st_size, PROT_READ, MAP_SHARED, fd_hzk16, 0);
	if (hzkmem == (unsigned char *)-1)
	{
		perror("can't mmap for hzk16");
		return -1;
	}
	//全部设置为黑色
	memset(fbmem, 0, screen_size);
	lcd_put_ascii(fb_var.xres/2, fb_var.yres/2, 'A');
	
	printf("chinese code: %02x %02x\n", str[0], str[1]);
	lcd_put_chinese(fb_var.xres/2 + 8,  fb_var.yres/2, str);

	//释放操作
	munmap(hzkmem, hzk_stat.st_size);
	close(fd_hzk16);
	munmap(fbmem, screen_size);
	close(fb_fd);
	return 0;
}
