#define FBDEV "/dev/fb0"
#define BACKGROUND "/data/background.bmp" //can put on /data to allow the user to set their own background
#define BITMAP_DEPTH 24
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

//TODO: The struct was nice for passing to functions, but we are using a global variable.
//      do we still need this?
typedef struct _fb_info
{
	int fbfd;
	uint8_t *fbp;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
} fb_info;

typedef struct _pixel {
	uint8_t b,g,r; // bmp lists colors backwards
} pixel;

void fb_init(void);
void fb_destroy(void);
void fb_background(void);
void fb_refresh(int,int, int,int);
void fb_crefresh(int,int, int,int);