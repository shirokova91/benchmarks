#include <generated/autoconf.h>
#ifndef _CBMC_
    #define assert(expr) \
            if (!(expr)) { \
            printk( "Assertion failed! %s,%s,%s,line=%d\n",\
            #expr,__FILE__,__func__,__LINE__); \
            BUG(); \
            }
#endif

#include <linux/rtc.h>
#include <asm/mc146818rtc.h>		/* register access macros */

/* Linux files */
/* We include it here because we want to propagate the autoconf */
#include <rtc/rtc-cmos.c>   /* Implements generic rtc-cmos.c driver*/
#include <rtc.c>            /* Implements x86 rtc (/kernel) */
#include <bcd.c>
#include <base/platform.c>
//#include <resource.c>
//#include <base/driver.c> - conflict with rtc-cmos

/* QEMU Model */
#include <rtc/mc146818rtc_regs.h>
#include <rtc/qemu-timer.h>
#include "rtc/qemu-timer.c"
#include <rtc/qverify.h>
#include "rtc/qverify.c"
#include <rtc/mc146818rtc.h>
#include "rtc/mc146818rtc.c"

/* PC cmos mappings */
#define REG_EQUIPMENT_BYTE          0x14

#include <stdint.h>

typedef uint64_t ram_addr_t;

QEMUClock *rtc_clock;

#ifdef _KLEE_
ram_addr_t ram_size;
int smp_cpus;
int use_rt_clock;
#endif

// The following two structs are from kernel/resource.c
struct resource ioport_resource = {
        .name   = "PCI IO",
        .start  = 0,
        .end    = IO_SPACE_LIMIT,
        .flags  = IORESOURCE_IO,
};
EXPORT_SYMBOL(ioport_resource);

struct resource iomem_resource = {
        .name   = "PCI mem",
        .start  = 0,
        .end    = -1,
        .flags  = IORESOURCE_MEM,
};
EXPORT_SYMBOL(iomem_resource);

/* use non-deterministic values */
#ifdef _CBMC_
int nondet_int();
uint64_t nondet_uint64();

ram_addr_t ram_size = nondet_uint64();
int smp_cpus = nondet_int();
int use_rt_clock = nondet_int();
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// See also pc_cmos_init() in pc.c
static void pc_cmos_init_for_rtc(RTCState *s)
{
    int val, nb;

    ram_addr_t above_4g_mem_size = 0;

    /* various important CMOS locations needed by PC/Bochs bios */

    /* memory size */
    /* base memory (first MiB) */
    val = MIN(ram_size / 1024, 640);
    _rtc_set_memory(s, 0x15, val);
    _rtc_set_memory(s, 0x16, val >> 8);
    /* extended memory (next 64MiB) */
    if (ram_size > 1024 * 1024) {
        val = (ram_size - 1024 * 1024) / 1024;
    } else {
        val = 0;
    }
    if (val > 65535)
        val = 65535;
    _rtc_set_memory(s, 0x17, val);
    _rtc_set_memory(s, 0x18, val >> 8);
    _rtc_set_memory(s, 0x30, val);
    _rtc_set_memory(s, 0x31, val >> 8);
    /* memory between 16MiB and 4GiB */
    if (ram_size > 16 * 1024 * 1024) {
        val = (ram_size - 16 * 1024 * 1024) / 65536;
    } else {
        val = 0;
    }
    if (val > 65535)
        val = 65535;
    _rtc_set_memory(s, 0x34, val);
    _rtc_set_memory(s, 0x35, val >> 8);
    /* memory above 4GiB */
    val = above_4g_mem_size / 65536;
    _rtc_set_memory(s, 0x5b, val);
    _rtc_set_memory(s, 0x5c, val >> 8);
    _rtc_set_memory(s, 0x5d, val >> 16);

    /* set the number of CPU */
    _rtc_set_memory(s, 0x5f, smp_cpus - 1);

    nb = 0; /* use non-determinism */
    switch (nb) {
    case 0:
        break;
    case 1:
        val |= 0x01; /* 1 drive, ready for boot */
        break;
    case 2:
        val |= 0x41; /* 2 drives, ready for boot */
        break;
    }
    val |= 0x02; /* FPU is there */
    val |= 0x04; /* PS/2 mouse installed */
    _rtc_set_memory(s, REG_EQUIPMENT_BYTE, val);
}

int main (int argc, char** argv) {
    int ok;

#ifdef _KLEE_
    klee_make_symbolic(&ram_size, sizeof(ram_size), "ram_size");
    klee_make_symbolic(&smp_cpus, sizeof(smp_cpus), "smp_cpus");
    klee_make_symbolic(&use_rt_clock, sizeof(use_rt_clock), "use_rt_clock");
#endif

    qverify_start();
    rtc_clock = vm_clock;
    _rtc_init(2000);
    pc_cmos_init_for_rtc(global_qverify->rtc_state);

    ok = cmos_init();

#ifdef CONFIG_FALSE_POSITIVE
    // symbolic execution should report a false positive because
    // driver_register(struct device_driver *) is non-deterministic.
    // That is, the actual function definition is too complex to analyze.
    assert(ok == 0);
#endif

#ifdef _CBMC_
    __CPROVER_assume(ok == 0);
#endif

#ifdef _KLEE_
    klee_assume(ok == 0);
#endif

    // test cases
    struct rtc_time t;
    //cmos_read_time (NULL, &t);
    //cmos_set_time (NULL, &t);
    get_rtc_time (&t);
    set_rtc_time (&t);
    //assert (rtc_is_updating());

    //struct cmos_rtc *cmos = dev_get_drvdata(dev);
    cmos_exit();

    return 0;
}
