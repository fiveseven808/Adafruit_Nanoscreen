# Adafruit_Nanoscreen
Raspberry Pi framebuffer copy to 96x64 pixel RGB OLED:
https://www.adafruit.com/products/684

This is UNSUPPORTED CODE and will not work on other screen types or sizes, please don't ask.

Recommended /boot/config.txt settings:

```# Not all monitors can handle this resolution,
# but the framebuffer is there and correct and
# nanoscreen will work with it.
disable_overscan=1
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=87
hdmi_cvt=384 256 60 1 0 0 0
# Optional, for 'portrait' video:
display_rotate=3
```
