MODULE_NAME := stubout

ifneq ($(KBUILD_CFLAGS),)
# Kbuild
obj-m  := $(MODULE_NAME).o
$(MODULE_NAME)-y := module.o
else
# normal Makefile
KVER := $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
MODULE := $(MODULE_NAME).ko

.PHONY: all
all: $(MODULE)
$(MODULE): module.c
	$(MAKE) -C $(KDIR) M=$$PWD $@

busybox/busybox:
	$(MAKE) -C busybox defconfig
	sed -i -e 's/^# CONFIG_STATIC .*/CONFIG_STATIC=y/' busybox/.config
	$(MAKE) -C busybox oldconfig
	$(MAKE) -C busybox

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	$(MAKE) -C busybox clean
	rm -rf rootfs rootfs.initrd.gz

.PHONY: run
run: $(MODULE)
	sudo bash -c 'before=$$(mktemp); after=$$(mktemp); trap "rm $$before $$after" EXIT; dmesg > $$before; insmod $(MODULE); rmmod $(MODULE); dmesg > $$after; comm --nocheck-order -13 $$before $$after'

rootfs: busybox/busybox init
	mkdir -p rootfs/bin
	cp busybox/busybox rootfs/bin
	busybox/busybox --list|while read applet; do ln -sf busybox rootfs/bin/$$applet; done
	cp init rootfs/init
	chmod +x rootfs/init

rootfs.initrd.gz: $(MODULE) rootfs
	cp $(MODULE) rootfs/$(MODULE)
	(cd rootfs; find|cpio -ov -Hnewc | gzip -c) > rootfs.initrd.gz

APPEND=
QEMUARGS?=

.PHONY: run-qemu
run-qemu: rootfs.initrd.gz
	qemu-system-x86_64 -kernel /boot/vmlinuz-$(KVER) \
		-m 64M \
		-append "console=ttyS0 $(APPEND)" \
		-chardev stdio,mux=on,id=char0 \
		-initrd rootfs.initrd.gz \
		-mon chardev=char0,mode=readline \
		-serial chardev:char0 \
		-nographic $(QEMUARGS)

endif
