// Nanoscreen is a framebuffer-copy-to-96x64-pixel-RGB-OLED utility,
// though using the term "utility" loosely here, as in practice such a
// tiny screen isn't of much practical use as a framebuffer substitute.
// This is just a hacky kludgey userspace thing written on a lark for
// a goofy project and should not be taken too seriously. Please.

// IT IS SPECIFICALLY FOR THIS SMALL SSD1331-BASED SCREEN AND WILL NOT
// WORK WITH ANY OTHER SCREEN TYPES OR SIZES, PERIOD, NEVER WILL.
// For most other displays there are device tree overlays or kernel
// modules that can work along with the 'fbcp' utility to get you going.

// Must be run as root (e.g. sudo ./nanoscreen) because hardware.

// Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
// MIT license. Some insights came from Tasanakorn's fbcp utility:
// https://github.com/tasanakorn/rpi-fbcp

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <bcm_host.h>

#include "nanoscreen.h"


// CONFIGURATION AND GLOBAL STUFF ------------------------------------------

#define WIDTH     128
#define HEIGHT    128
#define DC_PIN    23
#define RESET_PIN 24
#define BITRATE   32000000
#define SPI_MODE  SPI_MODE_0
#define FPS       60

// From GPIO example code by Dom and Gert van Loo on elinux.org:
#define PI1_BCM2708_PERI_BASE 0x20000000
#define PI1_GPIO_BASE         (PI1_BCM2708_PERI_BASE + 0x200000)
#define PI2_BCM2708_PERI_BASE 0x3F000000
#define PI2_GPIO_BASE         (PI2_BCM2708_PERI_BASE + 0x200000)
#define BLOCK_SIZE            (4*1024)
#define INP_GPIO(g)          *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g)          *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))

static volatile unsigned
  *gpio = NULL, // Memory-mapped GPIO peripheral
  *gpioSet,     // Write bitmask of GPIO pins to set here
  *gpioClr;     // Write bitmask of GPIO pins to clear here

uint32_t resetMask, dcMask; // GPIO pin bitmasks
int      fdSPI0;            // /dev/spidev0.0 file descriptor

static struct spi_ioc_transfer
 cmd = { // SPI transfer structure used when issuing commands to OLED
  .rx_buf        = 0,
  .delay_usecs   = 0,
  .bits_per_word = 8,
  .pad           = 0,
  .speed_hz      = BITRATE,
  .tx_nbits      = 0,
  .rx_nbits      = 0,
  .cs_change     = 0 },
 dat = { // SPI transfer structure used when issuing data
  .rx_buf        = 0,
  .delay_usecs   = 0,
  .bits_per_word = 8,
  .len           = 4096,
  .pad           = 0,
  .speed_hz      = BITRATE,
  .tx_nbits      = 0,
  .rx_nbits      = 0,
  .cs_change     = 0 };

/*
uint8_t
 initCommands[] = { // OLED init sequence
  0xAE,             // SSD1331_CMD_DISPLAYOFF
  0xA0, 0x72,       // SSD1331_CMD_SETREMAP RGB color 01110010
                      // A0 = 0 Horizontal increment
                      // A1 = 1 RAM Column 0 to 95 maps to Pin Seg (SA,SB,SC) 95 to 0
                      // A2 = 0 normal order SA,SB,SC (e.g. RGB)
                      // A3 = 0 Disable left-right swapping on COM
                      // A4 = 1 Scan from COM [N-1] to COM0. Where N is the multiplex ratio.
                      // A5 = 1 Enable COM Split Odd Even
                      // A6 = 1 65k color format
                      // A7 = 0 65k color format
  0xA1, 0x00,       // SSD1331_CMD_STARTLINE
  0xA2, 0x00,       // SSD1331_CMD_DISPLAYOFFSET
  0xA4,             // SSD1331_CMD_NORMALDISPLAY
  0xA8, 0x3F,       // SSD1331_CMD_SETMULTIPLEX
                      // Set MUX ratio to N = 63d = 3Fh
  0xAD, 0x8E,       // SSD1331_CMD_SETMASTER
  0xB0, 0x0B,       // SSD1331_CMD_POWERMODE
  0xB1, 0x31,       // SSD1331_CMD_PRECHARGE
  0xB3, 0xF0,       // SSD1331_CMD_CLOCKDIV
  0x8A, 0x64,       // SSD1331_CMD_PRECHARGEA
  0x8B, 0x78,       // SSD1331_CMD_PRECHARGEB
  0x8C, 0x64,       // SSD1331_CMD_PRECHARGEC
  0xBB, 0x3A,       // SSD1331_CMD_PRECHARGELEVEL
  0xBE, 0x3E,       // SSD1331_CMD_VCOMH
  0x87, 0x06,       // SSD1331_CMD_MASTERCURRENT
  0x81, 0x91,       // SSD1331_CMD_CONTRASTA
  0x82, 0x50,       // SSD1331_CMD_CONTRASTB
  0x83, 0x7D,       // SSD1331_CMD_CONTRASTC
  0xAF },           // SSD1331_CMD_DISPLAYON
 areaCommands[] = { // OLED memory-fill sequence
  0x75, 0, 63,      // SSD1331_CMD_SETROW
  0x15, 0, 95 };    // SSD1331_CMD_SETCOLUMN
*/

 uint8_t
   initCommands[] = { // OLED init sequence
    0xAE,             // SSD1351_CMD_DISPLAYOFF
    0xA0, 0x72,       // SSD1351_CMD_SETREMAP RGB color 01110010
                        // A0 = 0 Horizontal increment
                        // A1 = 1 RAM Column 0 to 95 maps to Pin Seg (SA,SB,SC) 95 to 0
                        // A2 = 0 normal order SA,SB,SC (e.g. RGB)
                        // A3 = 0 Reserved (leaving as 0)
                        // A4 = 1 Scan from COM [N-1] to COM0. Where N is the multiplex ratio.
                        // A5 = 1 Enable COM Split Odd Even
                        // A6 = 1 65k color format
                        // A7 = 0 65k color format
    0xA1, 0x00,       // SSD1351_CMD_STARTLINE
    //0xA2, 0x00,       // SSD1351_CMD_DISPLAYOFFSET
                        // This command is locked by Command FDh by default. To unlock it, please refer to Command FDh.
    0xA6,             // SSD1351_CMD_NORMALDISPLAY
                        // A6h : Reset to normal display [reset]
    0xCA, 0x7F,       // SSD1351_CMD_SETMULTIPLEX
                        // Set MUX ratio to N = 127d = 7Fh
    //0xAD, 0x8E,       // SSD1351_CMD_SETMASTER
                        // No Equivilant
    //0xB0, 0x0B,       // SSD1351_CMD_POWERMODE
                        //No Equivilant
    //0xB1, 0x31,       // SSD1351_CMD_PRECHARGE
                        // Phase 1 and 2 period adjustment
                        // 31h = 00110001 for SSD1331
                        // Phase 1 = 1d DCLK (5-31 for 1351)
                        // Phase 2 = 3d DCLK (3-15 for 1351)
                        // This command is locked by Command FDh by default. To unlock it, please refer to Command FDh.
    //0xB3, 0x04,       // SSD1351_CMD_CLOCKDIV
                        // 1331 is F0 = 15 + 1 = 16d
                        // 1351 0x04 = 16d
                        // 1351 0x0Ah is highest divide 1024
                        // 1351 first 4 bits are oscillator freq 1101b? = Dh = 13d
                        // This command is locked by Command FDh by default. To unlock it, please refer to Command FDh.
    0x8A, 0x64,       // SSD1351_CMD_PRECHARGEA
    0x8B, 0x78,       // SSD1351_CMD_PRECHARGEB
    0x8C, 0x64,       // SSD1351_CMD_PRECHARGEC
    0xBB, 0x3A,       // SSD1351_CMD_PRECHARGELEVEL
    0xBE, 0x3E,       // SSD1351_CMD_VCOMH
    0xC7, 0x06,       // SSD1351_CMD_MASTERCURRENT
                        // 0x87 is 0xC7 on 1351
                        // This makes the color dimmer though...
    0xC1, 0x91, 0x50, 0x7d, // Setting contrast according to the values below
                            // May not be necessary since reset sets them
    //0x81, 0x91,       // SSD1351_CMD_CONTRASTA
    //0x82, 0x50,       // SSD1351_CMD_CONTRASTB
    //0x83, 0x7D,       // SSD1351_CMD_CONTRASTC
    0xAF },           // SSD1351_CMD_DISPLAYON
   areaCommands[] = { // OLED memory-fill sequence
    0x75, 0, 127,      // SSD1351_CMD_SETROW
    0x15, 0, 127 };    // SSD1351_CMD_SETCOLUMN

static void runInit() {
    // Initialization Sequence
        writeCommands(SSD1351_CMD_COMMANDLOCK);  // set command lock
        writeData(0x12);
        writeCommands(SSD1351_CMD_COMMANDLOCK);  // set command lock
        writeData(0xB1);

        writeCommands(SSD1351_CMD_DISPLAYOFF);  		// 0xAE

        writeCommands(SSD1351_CMD_CLOCKDIV);  		// 0xB3
        writeCommands(0xF1);  						// 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)

        writeCommands(SSD1351_CMD_MUXRATIO);
        writeData(127);

        writeCommands(SSD1351_CMD_SETREMAP);
        writeData(0x74);

        writeCommands(SSD1351_CMD_SETCOLUMN);
        writeData(0x00);
        writeData(0x7F);
        writeCommands(SSD1351_CMD_SETROW);
        writeData(0x00);
        writeData(0x7F);

        writeCommands(SSD1351_CMD_STARTLINE); 		// 0xA1
        if (SSD1351HEIGHT == 96) {
          writeData(96);
        } else {
          writeData(0);
        }


        writeCommands(SSD1351_CMD_DISPLAYOFFSET); 	// 0xA2
        writeData(0x0);

        writeCommands(SSD1351_CMD_SETGPIO);
        writeData(0x00);

        writeCommands(SSD1351_CMD_FUNCTIONSELECT);
        writeData(0x01); // internal (diode drop)
        //writeData(0x01); // external bias

    //    writeCommands(SSSD1351_CMD_SETPHASELENGTH);
    //    writeData(0x32);

        writeCommands(SSD1351_CMD_PRECHARGE);  		// 0xB1
        writeCommands(0x32);

        writeCommands(SSD1351_CMD_VCOMH);  			// 0xBE
        writeCommands(0x05);

        writeCommands(SSD1351_CMD_NORMALDISPLAY);  	// 0xA6

        writeCommands(SSD1351_CMD_CONTRASTABC);
        writeData(0xC8);
        writeData(0x80);
        writeData(0xC8);

        writeCommands(SSD1351_CMD_CONTRASTMASTER);
        writeData(0x0F);

        writeCommands(SSD1351_CMD_SETVSL );
        writeData(0xA0);
        writeData(0xB5);
        writeData(0x55);

        writeCommands(SSD1351_CMD_PRECHARGE2);
        writeData(0x01);

        writeCommands(SSD1351_CMD_DISPLAYON);		//--turn on oled panel
    }


// UTILITY FUNCTIONS -------------------------------------------------------

// Detect Pi board type.  Doesn't return super-granular details,
// just the most basic distinction needed for GPIO compatibility:
// 0: Pi 1 Model B revision 1
// 1: Pi 1 Model B revision 2, Model A, Model B+, Model A+
// 2: Pi 2 Model B
static int boardType(void) {
	FILE *fp;
	char  buf[1024], *ptr;
	int   n, board = 1; // Assume Pi1 Rev2 by default

	// Relies on info in /proc/cmdline.  If this becomes unreliable
	// in the future, alt code below uses /proc/cpuinfo if any better.
#if 1
	if((fp = fopen("/proc/cmdline", "r"))) {
		while(fgets(buf, sizeof(buf), fp)) {
			if((ptr = strstr(buf, "mem_size=")) &&
			   (sscanf(&ptr[9], "%x", &n) == 1) &&
			   (n == 0x3F000000)) {
				board = 2; // Appears to be a Pi 2
				break;
			} else if((ptr = strstr(buf, "boardrev=")) &&
			          (sscanf(&ptr[9], "%x", &n) == 1) &&
			          ((n == 0x02) || (n == 0x03))) {
				board = 0; // Appears to be an early Pi
				break;
			}
		}
		fclose(fp);
	}
#else
	char s[8];
	if((fp = fopen("/proc/cpuinfo", "r"))) {
		while(fgets(buf, sizeof(buf), fp)) {
			if((ptr = strstr(buf, "Hardware")) &&
			   (sscanf(&ptr[8], " : %7s", s) == 1) &&
			   (!strcmp(s, "BCM2709"))) {
				board = 2; // Appears to be a Pi 2
				break;
			} else if((ptr = strstr(buf, "Revision")) &&
			          (sscanf(&ptr[8], " : %x", &n) == 1) &&
			          ((n == 0x02) || (n == 0x03))) {
				board = 0; // Appears to be an early Pi
				break;
			}
		}
		fclose(fp);
	}
#endif

	return board;
}

// Crude error 'handler' (prints message, returns same code as passed in)
static int err(int code, char *string) {
	(void)puts(string);
	return code;
}

// Issue command (not data) to OLED
static void writeCommands(uint8_t *c, uint8_t len) {
	*gpioClr   = dcMask; // 0/low = command, 1/high = data
	cmd.tx_buf = (unsigned long)c;
	cmd.len    = len;
	(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &cmd);
}

// Issue data (not command) to OLED
static void writeData(uint8_t *c) {
	*gpioSet   = dcMask; // 0/low = command, 1/high = data
	cmd.tx_buf = (unsigned long)c;
	//cmd.len    = len;
    cmd.len    = lsizeof(c)
	(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &cmd);
}


// MAIN CODE ---------------------------------------------------------------

int main(int argc, char *argv[]) {

	uint16_t pixelBuf[WIDTH * HEIGHT];       // 16-bit pixel buffer
	uint32_t bigBuf[WIDTH * 4 * HEIGHT * 4]; // 32-bit 4X size buffer
	uint8_t  isPi2 = 0;

	// DISPMANX INIT ---------------------------------------------------

	bcm_host_init();

	DISPMANX_DISPLAY_HANDLE_T  display; // Primary framebuffer display
	DISPMANX_RESOURCE_HANDLE_T screen_resource; // Intermediary buf
	uint32_t                   handle;
	VC_RECT_T                  rect;

	if(!(display = vc_dispmanx_display_open(0))) {
		return err(1, "Can't open primary display");
	}

	// screen_resource is an intermediary between framebuffer and
	// main RAM -- VideoCore will copy the primary framebuffer
	// contents to this resource. It's 4X the OLED pixel dimensions
	// (ideally the framebuffer resolution will match this) to allow
	// for quality downsampling (see notes in code below).
	if(!(screen_resource = vc_dispmanx_resource_create(
	  VC_IMAGE_ARGB8888, WIDTH*4, HEIGHT*4, &handle))) {
		vc_dispmanx_display_close(display);
		return err(2, "Can't create screen buffer");
	}
	vc_dispmanx_rect_set(&rect, 0, 0, WIDTH*4, HEIGHT*4);

	// GPIO AND OLED SCREEN INIT ---------------------------------------

	int fd;
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
		return err(3, "Can't open /dev/mem (try 'sudo')\n");
	}
	isPi2 = (boardType() == 2);
	gpio  = (volatile unsigned *)mmap( // Memory-map I/O
	  NULL,                 // Any adddress will do
	  BLOCK_SIZE,           // Mapped block length
	  PROT_READ|PROT_WRITE, // Enable read+write
	  MAP_SHARED,           // Shared w/other processes
	  fd,                   // File to map
	  isPi2 ?
	   PI2_GPIO_BASE :      // -> GPIO registers
	   PI1_GPIO_BASE);
	close(fd);              // Not needed after mmap()
	if(gpio == MAP_FAILED) {
		return err(4, "Can't mmap()");
	}
	gpioSet   = &gpio[7];
	gpioClr   = &gpio[10];
	resetMask = 1 << RESET_PIN;
	dcMask    = 1 << DC_PIN;

	if((fdSPI0 = open("/dev/spidev0.0", O_WRONLY | O_NONBLOCK)) < 0) {
		return err(5, "Can't open /dev/spidev0.0 (try 'sudo')\n");
	}
	uint8_t mode = SPI_MODE;
	ioctl(fdSPI0, SPI_IOC_WR_MODE, &mode);
	ioctl(fdSPI0, SPI_IOC_WR_MAX_SPEED_HZ, BITRATE);

	// Set 2 pins as outputs.  Must use INP before OUT.
	INP_GPIO(RESET_PIN); OUT_GPIO(RESET_PIN);
	INP_GPIO(DC_PIN)   ; OUT_GPIO(DC_PIN);

	*gpioSet = resetMask; usleep(50); // Reset high,
	*gpioClr = resetMask; usleep(50); // low,
	*gpioSet = resetMask; usleep(50); // high

	// Initialize OLED
	writeCommands(initCommands, sizeof(initCommands));

	// MAIN LOOP -------------------------------------------------------

	struct timeval tv;
	uint32_t       timeNow, timePrev = 0, timeDelta;

	for(;;) {
		// Throttle transfer to approx FPS frames/sec.
		// usleep() avoids heavy CPU load of time polling.
		gettimeofday(&tv, NULL);
		timeNow = tv.tv_sec * 1000000 + tv.tv_usec;
		timeDelta = timeNow - timePrev;
		if(timeDelta < (1000000 / FPS)) {
			usleep((1000000 / FPS) - timeDelta);
		}
		timePrev = timeNow;

		// Framebuffer -> intermediary
		vc_dispmanx_snapshot(display, screen_resource, 0);
		// Intermediary -> main RAM
		vc_dispmanx_resource_read_data(screen_resource, &rect,
		  bigBuf, WIDTH * 4 * 4);

		// Before pushing data to SPI screen, column and row
		// ranges are reset every frame to force screen data
		// pointer back to (0,0).  Though the pointer will
		// automatically 'wrap' when the end of the screen is
		// reached, this is extra insurance in case there's
		// a glitch where a byte doesn't get through to the
		// display (which would then be out of sync in all
		// subsequent frames).
		writeCommands(areaCommands, sizeof(areaCommands));

		*gpioSet = dcMask; // DC high

		// Downsampling from 384x256 to 96x64 is done here in
		// software rather than the dispmanx code, as bilinear
		// interpolation exhibits artifacts when downscaling
		// more than 2:1 (e.g. some dots in Pac Man are entirely
		// lost, making the game unplayable, some horiz or vert
		// lines in vector games are lost). Box sampling is used
		// instead to average 16 pixels to 1, the result then
		// reduced to 16-bit (5/6/5) color. There's probably
		// ways to do this all on the GPU with a 2-step
		// downsample in dispmanx, but that's unfamiliar
		// territory and this project isn't all that serious.
		// Instead, lots of bit-twiddling...
		int      x, y, x1, y1;
		uint32_t pixel32, rb, g;
		uint16_t pixel16;
		for(y=0; y<HEIGHT; y++) {
			for(x=0; x<WIDTH; x++) {
				rb = g = 0;
				for(y1=0; y1<4; y1++) {
					for(x1=0; x1<4; x1++) {
						pixel32 =
						  bigBuf[((y*4)+y1) *
						  WIDTH*4+(x*4)+x1];
						rb += pixel32 & 0x00FF00FF;
						g  += pixel32 & 0x0000FF00;
					}
				}
				// 0000 RRRR  RRRR RRRR  0000 BBBB  BBBB BBBB
				//                       RRRR R000  000B BBBB
				// 0000 0000  0000 GGGG  GGGG GGGG  0000 0000
				//                            0GGG  GGG0 0000
				pixel16 = ((rb & 0xF800000) >> 12) |
					  ((g  & 0x00FC000) >>  9) |
					  ((rb & 0x0000F80) >>  7);
				// RRRRRGGG GGGBBBBB -> GGGBBBBB RRRRRGGG
				pixelBuf[y * WIDTH + x] =
				  (pixel16 * 0x00010001) >> 8;
			}
		}

		// Max SPI transfer size is 4096 bytes
		// 128 * 128 * 2 = 32768 bytes
		// 32768 / 4096 = 8 full transfers, no fraction needed
		dat.tx_buf = (uint32_t)pixelBuf;
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
		dat.tx_buf = (uint32_t)&pixelBuf[2048];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
		dat.tx_buf = (uint32_t)&pixelBuf[4096];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
        dat.tx_buf = (uint32_t)&pixelBuf[6144];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
        dat.tx_buf = (uint32_t)&pixelBuf[8192];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
        dat.tx_buf = (uint32_t)&pixelBuf[10240];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
        dat.tx_buf = (uint32_t)&pixelBuf[12288];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
        dat.tx_buf = (uint32_t)&pixelBuf[14336];
		(void)ioctl(fdSPI0, SPI_IOC_MESSAGE(1), &dat);
	}

	vc_dispmanx_resource_delete(screen_resource);
	vc_dispmanx_display_close(display);
	return 0;
}
