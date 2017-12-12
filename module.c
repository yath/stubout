#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#define TARGET "uptime_proc_show"

#ifndef lm_alias
#define lm_alias(x) __va(__pa_symbol(x))
#endif

const char stub[] = { 0x31, 0xc0 /* xor %eax,%eax */, 0xc3 /* retq */ };
#define STUB_SIZE (sizeof(stub))
char saved[STUB_SIZE];

void *target;
int (*make_rw)(unsigned long addr, int numpages);
int (*make_ro)(unsigned long addr, int numpages);

int assert_symbol(void **symbol, const char *name) {
    if (*symbol)
        return 1;
    *symbol = (void*)kallsyms_lookup_name(name);
    if (!*symbol) {
        printk(KERN_ERR "Symbol \"%s\" not found in kallsyms\n", name);
        return 0;
    }
    return 1;
}

static inline void *page_for(const void *addr) {
    return (void *)(((unsigned long)addr) & PAGE_MASK);
}

static int try_write(void *addr, const void *src, size_t size) {
    int okay = 0;
    void *page = page_for(addr);
    BUG_ON(page_for(addr+size) != page);

    if (!assert_symbol((void **)&make_rw, "set_memory_rw"))
        return 0;

    int ret = make_rw(page, 1);
    if (ret) {
        printk(KERN_ERR "Can't set page 0x%p R/W (%d)\n", page, ret);
        return 0;
    }

    /* Text mappings are always R/O, so write to that location in the linearly mapped physical
     * memory. */
    long ret2 = probe_kernel_write(lm_alias(addr), src, size);
    if (ret2) {
        printk(KERN_ERR "Can't write %ld bytes to memory at 0x%p (0x%p): %ld\n", size, lm_alias(addr),
                addr, ret2);
        goto out_ro;
    }
    okay = 1;

out_ro:
    if (!assert_symbol((void **)&make_ro, "set_memory_ro"))
        return 0;

    ret = make_ro(page, 1);
    if (ret) {
        printk(KERN_ERR "Can't set page 0x%p R/O (%d)\n", page, ret);
        return 0;
    }
    return okay;
}

static int __init patch (void){
    if (!assert_symbol((void **)&target, TARGET))
        return -1;

    long ret = probe_kernel_read(saved, target, STUB_SIZE);
    if (ret) {
        printk(KERN_ERR "probe_kernel_read(saved, 0x%p, %ld) = %ld\n", target, STUB_SIZE, ret);
        return -1;
    }

    if (!try_write(target, stub, STUB_SIZE))
        return -1;

    printk(KERN_INFO "Patched %ld bytes at 0x%p (%s)\n", STUB_SIZE, target, TARGET);

    return 0;
}

static void __exit unpatch(void){
    char check[sizeof(saved)];
    long ret = probe_kernel_read(check, target, STUB_SIZE);
    if (ret) {
        printk(KERN_ERR "probe_kernel_read(check, 0x%p, %ld) = %ld, can't verify for unpatching\n",
                target, sizeof(saved), ret);
        return;
    }

    if (memcmp(check, stub, STUB_SIZE)) {
        printk(KERN_ERR "verification of stub failed, not unpatching\n");
        return;
    }

    if (try_write(target, saved, STUB_SIZE))
        printk(KERN_INFO "Unpatched %ld bytes at 0x%p (%s)\n", STUB_SIZE, target, TARGET);
}

module_init(patch);
module_exit(unpatch);

MODULE_LICENSE("GPL");
