all: vga_switch prime

CFLAGS?=-Wall -Wextra -std=gnu99 -O2 -g -flto=4

CFLAGS+=$(shell pkg-config --cflags libdrm)
LIBS=$(shell pkg-config --libs libdrm)

OBJS =  \
	switch.o \
#

OBJS_PRIME = \
	     prime.o \
#

OBJS_ALL = ${OBJS_PRIME} ${OBJS}

prime: ${OBJS_PRIME}
	${CC} ${CFLAGS} ${LIBS} $< -o $@

vga_switch: ${OBJS}
	${CC} ${CFLAGS} ${LIBS} $< -o $@

clean:
	${RM} ${OBJS_ALL} vga_switch prime

install: vga_switch
	install -m 6755 vga_switch /sbin/vga_switch
	install -m 0755 prime /usr/bin/vga_prime
	install -m 0644 98vga_switch_hack /etc/X11/Xsession.d/98vga_switch_hack
	install -m 0755 init.script /etc/init.d/vga_switch
	update-rc.d vga_switch defaults

uninstall:
	${RM} /sbin/vga_switch
	${RM} /usr/bin/vga_prime
	${RM} /etc/X11/Xsession.d/98vga_switch_hack
	update-rc.d vga_switch remove
	${RM} /etc/init.d/vga_switch
