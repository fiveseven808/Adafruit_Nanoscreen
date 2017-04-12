all: nanoscreen

CFLAGS=-Ofast -fomit-frame-pointer \
 -I/opt/vc/include \
 -I/opt/vc/include/interface/vcos/pthreads \
 -I/opt/vc/include/interface/vmcs_host \
 -I/opt/vc/include/interface/vmcs_host/linux \
 -L/opt/vc/lib
LIBS=-pthread -lrt -lm -lbcm_host

#nanoscreen: nanoscreen.c
#	cc $(CFLAGS) nanoscreen.c $(LIBS) -o nanoscreen
#	strip nanoscreen

nanoscreen: nanoscreen_ssd1351.c
		cc $(CFLAGS) nanoscreen_ssd1351.c $(LIBS) -o nanoscreen_ssd1351
		strip nanoscreen_ssd1351

#install:
#	mv nanoscreen /usr/local/bin

#clean:
#	rm -f nanoscreen
