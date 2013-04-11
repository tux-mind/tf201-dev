#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>

#include "common3.h"
#include "fbGUI.h"

long int screensize; // number of bytes in the screen pointer
fb_info fbinfo; // framebuffer information
uint8_t *bkgdp; // pointer to a copy of the screen containing the background

void fb_init()
{
		fbinfo.fbfd = open(FBDEV, O_RDWR);
		if (!fbinfo.fbfd) {
			FATAL("cannot open framebuffer device (%s)\n", FBDEV);
		}
		if (ioctl(fbinfo.fbfd, FBIOGET_FSCREENINFO, &fbinfo.finfo)) {
			FATAL("cannot get screen info\n");
		}
		if (ioctl(fbinfo.fbfd, FBIOGET_VSCREENINFO, &fbinfo.vinfo)) {
			FATAL("cannot get variable screen info\n");
		}

		screensize = fbinfo.vinfo.xres * fbinfo.vinfo.yres * fbinfo.vinfo.bits_per_pixel / 8;

		// map the device to memory
		fbinfo.fbp = (uint8_t  *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbinfo.fbfd, 0);
        if ((int)fbinfo.fbp == -1) {
        	FATAL("failed to map framebuffer device to memory\n");
		}
}

void fb_destroy()
{
	munmap(fbinfo.fbp, screensize);
	close(fbinfo.fbfd);
	free(bkgdp);
}

void fb_background()
{
	int fd, start, rowsize;
	int width, height, x, y;
	uint8_t *dest;
	pixel *pixels;

	if(!(fd = open(BACKGROUND,O_RDONLY)))
	{
		//only a warning, default to black background when not found
		WARN("cannot open \"%s\" - %s\n",BACKGROUND,strerror(errno));
		return;
	}

	pread(fd, &start,  4, 0x0A);
	pread(fd, &width,  4, 0x12);
	pread(fd, &height, 4, 0x16);

	rowsize = ((BITMAP_DEPTH*width+31)/32)*4; //round to multiple of 4
	pixels = malloc(rowsize);
	bkgdp = malloc (screensize);

	if (!pixels || !bkgdp)
	{
		FATAL("malloc - %s\n",strerror(errno));
		return;
	}

	dest = bkgdp + (fbinfo.vinfo.xoffset)*(fbinfo.vinfo.bits_per_pixel/8) + (fbinfo.vinfo.yoffset)*fbinfo.finfo.line_length;
	for (y=0; y<height; y++) {
		pread(fd, pixels, rowsize, start+rowsize*(height-y));
		for (x=0; x<width; x++) {
			*(dest + 0) = pixels[x].r;
			*(dest + 1) = pixels[x].g;
			*(dest + 2) = pixels[x].b;
			*(dest + 3) = 0; //alpha
			dest += fbinfo.vinfo.bits_per_pixel/8; //4
		}
	}

	memcpy(fbinfo.fbp, bkgdp, screensize); // copy the background to the screen
}

pixel getpixel(uint8_t *src)
{
	pixel pix;
	pix.r = *(src + 0);
	pix.g = *(src + 1);
	pix.b = *(src + 2);
	return pix;
}

//TODO: THIS IS REALLY LAGGY with large ammounts of pixels to check
void fb_refresh(int x, int y, int w, int h)
{
	long int offset;
	int i,j;
	pixel pix;

	offset = (x+fbinfo.vinfo.xoffset)*(fbinfo.vinfo.bits_per_pixel/8) + (y+fbinfo.vinfo.yoffset)*fbinfo.finfo.line_length;
	for (j=0; j<h; j++) {
		for (i=0; i<w; i++) {
			pix = getpixel(fbinfo.fbp + offset + i*4);
			if ((pix.r + pix.g + pix.b) == 0) //only replace black
				memcpy(fbinfo.fbp+offset+i*4, bkgdp+offset+i*4, fbinfo.vinfo.bits_per_pixel/8);
		}
		offset += fbinfo.finfo.line_length; //go down a line
	}
}

// wrapper to use row and col for text output
void fb_crefresh(int col, int row, int width, int height)
{
	fb_refresh(col*CHAR_WIDTH, row*CHAR_HEIGHT, width*CHAR_WIDTH, height*CHAR_HEIGHT);
}