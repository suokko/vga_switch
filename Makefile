all: vga_switch vga_prime libvgaswitch.so

CFLAGS?=-Wall -Wextra -std=gnu99 -O2 -g -flto=4

CFLAGS+=$(shell pkg-config --cflags libdrm xorg-server)
LIBS=-Wl,--as-needed $(shell pkg-config --libs libdrm xorg-server)

OBJS =  \
	switch.o \
	parse.o \
#

OBJS_PRIME = \
	     prime.o \
#

XMODULE_OBJS = \
	      xmodule.o \
#

OBJS_ALL = ${OBJS_PRIME} ${OBJS}

vga_prime: ${OBJS_PRIME}
	${CC} ${CFLAGS} $^ -o $@ ${LIBS}

vga_switch: ${OBJS}
	${CC} ${CFLAGS} $^ -o $@ ${LIBS}

libvgaswitch.so: ${XMODULE_OBJS}
	${CC} -shared -fPIC ${CFLAGS} $^ -o $@ ${LIBS}

clean:
	${RM} ${OBJS_ALL} vga_switch libvgaswitch.so vga_prime

install: vga_switch libvgaswitch.so vga_prime
	install -m 6755 vga_switch /sbin/vga_switch
	install -m 0644 libvgaswitch.so /usr/lib/xorg/modules/libvgaswitch.so
	install -m 0755 vga_prime /usr/bin/vga_prime
	install -m 0644 98vga_switch_hack /etc/X11/Xsession.d/98vga_switch_hack
	install -m 0755 init.script /etc/init.d/vga_switch
	update-rc.d vga_switch defaults
	install -m 0755 pm-vga /etc/pm/sleep.d/vga
	install -m 0755 systemd-vga.sh /lib/systemd/system-sleep/vga.sh

uninstall:
	${RM} /sbin/vga_switch
	${RM} /usr/lib/xorg/modules/libvgaswitch.so
	${RM} /usr/bin/vga_prime
	${RM} /etc/X11/Xsession.d/98vga_switch_hack
	update-rc.d vga_switch remove
	${RM} /etc/init.d/vga_switch
	${RM} /etc/pm/sleep.d/vga
	${RM} /lib/systemd/system-sleep/vga.sh
