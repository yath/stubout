This is a simple kernel module that “stubs out” a function in the Linux kernel,
`uptime_proc_show` in this case.

Demo? Sure!

```
$ make run-qemu
[… build and boot messages …]
[    0.920600] Freeing unused kernel memory: 1732K
[    0.930779] Freeing unused kernel memory: 1236K
[    0.939976] x86/mm: Checked W+X mappings: passed, no W+X pages found.
[    1.056444] stubout: loading out-of-tree module taints kernel.
[    1.058486] stubout: module verification failed: signature and/or required key missing - tainting kernel
[    1.071855] Patched 3 bytes at 0xffffffff8be88ff0 (uptime_proc_show)
/ # cat /proc/uptime
/ # rmmod stubout.ko
[   20.389701] Unpatched 3 bytes at 0xffffffff8be88ff0 (uptime_proc_show)
/ # cat /proc/uptime
23.15 22.21
/ # insmod stubout.ko
[   28.672915] Patched 3 bytes at 0xffffffff8be88ff0 (uptime_proc_show)
/ # cat /proc/uptime
/ #
```
