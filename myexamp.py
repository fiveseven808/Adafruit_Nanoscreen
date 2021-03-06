import SSD1351
import Adafruit_GPIO.SPI as SPI

# SSD1351 Constants
SSD1351_CMD_SETCOLUMN = 0x15
SSD1351_CMD_SETROW = 0x75
SSD1351_CMD_WRITERAM = 0x5C
SSD1351_CMD_READRAM = 0x5D
SSD1351_CMD_SETREMAP = 0xA0
SSD1351_CMD_STARTLINE = 0xA1
SSD1351_CMD_DISPLAYOFFSET = 0xA2
SSD1351_CMD_DISPLAYALLOFF = 0xA4
SSD1351_CMD_DISPLAYALLON = 0xA5
SSD1351_CMD_NORMALDISPLAY = 0xA6
SSD1351_CMD_INVERTDISPLAY = 0xA7
SSD1351_CMD_FUNCTIONSELECT = 0xAB
SSD1351_CMD_DISPLAYOFF = 0xAE
SSD1351_CMD_DISPLAYON = 0xAF
SSD1351_CMD_PRECHARGE = 0xB1
SSD1351_CMD_DISPLAYENHANC = 0xB2
SSD1351_CMD_CLOCKDIV = 0xB3
SSD1351_CMD_SETVSL = 0xB4
SSD1351_CMD_SETGPIO = 0xB5
SSD1351_CMD_PRECHARGE2 = 0xB6
SSD1351_CMD_SETGRAY = 0xB8
SSD1351_CMD_USELUT = 0xB9
SSD1351_CMD_PRECHARGELEVEL = 0xBB
SSD1351_CMD_VCOMH = 0xBE
SSD1351_CMD_CONTRASTABC	= 0xC1
SSD1351_CMD_CONTRASTMASTER = 0xC7
SSD1351_CMD_MUXRATIO = 0xCA
SSD1351_CMD_COMMANDLOCK = 0xFD
SSD1351_CMD_HORIZSCROLL = 0x96
SSD1351_CMD_STOPSCROLL = 0x9E
SSD1351_CMD_STARTSCROLL = 0x9F

height = 12

# Raspberry Pi pin configuration:
RST = 6
# Note the following are only used with SPI:
DC = 5
SPI_PORT = 0
SPI_DEVICE = 0

# Beaglebone Black pin configuration:
# RST = 'P9_12'
# Note the following are only used with SPI:
# DC = 'P9_15'
# SPI_PORT = 1
# SPI_DEVICE = 0

# 128x32 display with hardware I2C:
#disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST)

# 128x64 display with hardware I2C:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST)

# Note you can change the I2C address by passing an i2c_address parameter like:
# disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, i2c_address=0x3C)

# Alternatively you can specify an explicit I2C bus number, for example
# with the 128x32 display you would use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, i2c_bus=2)

# 128x32 display with hardware SPI:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

# 128x64 display with hardware SPI:
disp = SSD1351.SSD1351_128_128(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

# Alternatively you can specify a software SPI implementation by providing
# digital GPIO pin numbers for all the required display pins.  For example
# on a Raspberry Pi with the 128x32 display you might use:
# disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, sclk=18, din=25, cs=22)

# Initialize library.
disp.begin()

# Clear display.
disp.clear()
disp.display()


disp.command(SSD1351_CMD_SETCOLUMN)
disp.data(0)              # Column start address. (0 = reset)
disp.data(disp.width-1)   # Column end address.
disp.command(SSD1351_CMD_SETROW)
disp.data(0)              # Row start address. (0 = reset)
disp.data(disp.height-1)
disp.command(SSD1351_CMD_WRITERAM)

disp.command(SSD1351_CMD_DISPLAYALLOFF)

for i in range(1,15000):
    disp.data(0xf0)
