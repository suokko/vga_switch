all: vga_switch

CFLAGS?=-Wall -Wextra -std=gnu99 -O2 -g -flto=4

CFLAGS+=$(shell pkg-config --cflags libdrm)
LIBS=$(shell pkg-config --libs libdrm)

OBJS =  \
	switch.o \
#

vga_switch: ${OBJS}
	${CC} ${CFLAGS} ${LIBS} $< -o $@

clean:
	${RM} ${OBJS} vga_switch

install: vga_switch
	install -m 6755 vga_switch /sbin/vga_switch

uninstall:
	${RM} /sbin/vga_switch
