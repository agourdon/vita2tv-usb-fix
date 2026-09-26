/*
 * Copyright (c) 2026 Vita2TV contributors
 * SPDX-License-Identifier: MIT
 */

/* Firmware-specific EP0 phase-order correction. No timeout, retry,
 * synthetic completion or hot installation. Remove from configuration and
 * reboot to undo; never unload a trampoline reachable by firmware. */
#include "phase_guard.h"
#include "firmware_guard.h"
#include "phase_install.h"

#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <taihen.h>
#include <stdio.h>
#include <string.h>

int module_get_export_func(SceUID pid, const char *module_name,
                          uint32_t library_nid, uint32_t function_nid,
                          uintptr_t *function);

/* Published before the code injection; immutable until the next reboot. */
volatile uintptr_t phase_resume;
volatile uintptr_t phase_skip;
void phase_checkpoint(void);
static SceUID patch_id = -1;

static int write_all(SceUID fd, const char *data, unsigned size)
{
    while (size) {
        int written = ksceIoWrite(fd, data, size);
        if (written <= 0 || (unsigned)written > size)
            return -1;
        data += written;
        size -= (unsigned)written;
    }
    return 0;
}

static int open_log(void)
{
    (void)ksceIoMkdir("ur0:data/vita2tv_usb_fix", 0777);
    unsigned now = (unsigned)(ksceKernelGetSystemTimeWide() / 1000);
    char path[96];
    for (unsigned i = 0; i < 16; ++i) {
        int size = snprintf(path, sizeof(path),
            "ur0:data/vita2tv_usb_fix/boot-%08x-%u.log", now, i);
        if (size < 0 || (unsigned)size >= sizeof(path))
            return -1;
        SceUID fd = ksceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0666);
        if (fd >= 0)
            return fd;
    }
    return -1;
}

static int get_info(SceUID modid, SceKernelModuleInfo *info)
{
    uintptr_t address = 0;
    int result = module_get_export_func(KERNEL_PID, "SceKernelModulemgr",
                                       0x92c9ffc2, 0xdaa90093, &address);
    if (result < 0 || !address)
        result = module_get_export_func(KERNEL_PID, "SceKernelModulemgr",
                                       0xc445fa63, 0xd269f915, &address);
    if (result < 0 || !address)
        return -1;
    typedef int (*get_info_fn)(SceUID, SceUID, SceKernelModuleInfo *);
    return ((get_info_fn)address)(KERNEL_PID, modid, info);
}

static int stopped(uintptr_t data)
{
    return *(const volatile uint32_t *)(data + 0x60c + 0x100) == 0x10 &&
           *(const volatile uint32_t *)(data + 2 * 0x60c + 0x100) == 0x10;
}

static void fail_stop(SceUID fd)
{
    static const char message[] =
        "FATAL: boot patch state uncertain; startup stopped; reboot holding L\n";
    (void)write_all(fd, message, sizeof(message) - 1);
    (void)ksceIoClose(fd);
    /* Only reached on unrecoverable installation failure, never from IRQ.
     * Do not return and let a USB plugin enable a controller against unknown code. */
    for (;;)
        (void)ksceKernelDelayThread(1000000);
}

int module_start(SceSize argc, const void *args)
{
    (void)argc;
    (void)args;
    SceUID fd = open_log();
    if (fd < 0)
        return SCE_KERNEL_START_NO_RESIDENT;
    tai_module_info_t tai = {.size = sizeof(tai)};
    SceKernelModuleInfo info = {.size = sizeof(info)};
    uintptr_t text = 0, data = 0;
    int result = taiGetModuleInfoForKernel(KERNEL_PID, "SceUdcd", &tai);
    if (result == 0)
        result = get_info(tai.modid, &info);
    if (result == 0 && strcmp(info.module_name, "SceUdcd") == 0) {
        text = (uintptr_t)info.segments[0].vaddr;
        data = (uintptr_t)info.segments[1].vaddr;
        result = guard_firmware((const uint8_t *)text, info.segments[0].memsz,
                                text, data, info.segments[1].memsz);
        if (result == 0)
            result = phase_guard((const uint8_t *)text, info.segments[0].memsz, data);
    } else {
        result = -1;
    }
    uint8_t branch[4];
    if (result == 0)
        result = phase_branch(text + PHASE_SITE,
                              (uintptr_t)phase_checkpoint, branch);
    char line[240];
    int size = snprintf(line, sizeof(line),
        "Vita2TV USB Fix 0.1; nonstandard OUT data only\n"
        "guard=%08x module_nid=%08x text=%08x data=%08x\n",
        (unsigned)result, (unsigned)tai.module_nid, (unsigned)text, (unsigned)data);
    if (size < 0 || (unsigned)size >= sizeof(line) ||
        write_all(fd, line, (unsigned)size) < 0 || result != 0)
        goto rejected;

    /* Must precede all USB activators in the boot configuration.
     * Physical USB disconnection is an additional test prerequisite.
     * SuspendIntr is NOT a drain primitive: require both sources disabled
     * already, before first activation, and both controllers stopped. */
    if (ksceKernelGetSystemTimeWide() / 1000 >= 10000 || !stopped(data))
        goto rejected;
    struct install_irq_mask mask;
    int masked = phase_mask_already_disabled(&mask, ksceKernelSuspendIntr,
                                             ksceKernelResumeIntr);
    if (masked == -2)
        fail_stop(fd);
    if (masked < 0)
        goto rejected;
    if (ksceKernelGetSystemTimeWide() / 1000 >= 10000 || !stopped(data)) {
        if (install_restore_irqs(&mask, ksceKernelResumeIntr) < 0)
            fail_stop(fd);
        goto rejected;
    }
    phase_resume = (text + PHASE_RESUME) | 1;
    phase_skip = (text + PHASE_SKIP) | 1;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    static const uint8_t original[4] = {0xd6, 0xf8, 0x14, 0x31};
    patch_id = taiInjectDataForKernel(KERNEL_PID, tai.modid, 0,
                                      PHASE_SITE, branch, sizeof(branch));
    int verified = patch_id >= 0 &&
                   memcmp((const void *)(text + PHASE_SITE), branch, 4) == 0;
    if (!verified) {
        if (patch_id >= 0)
            (void)taiInjectReleaseForKernel(patch_id);
        /* An API failure is not proof that it left instruction bytes intact. */
        if (memcmp((const void *)(text + PHASE_SITE), original, 4) != 0)
            (void)taiInjectDataForKernel(KERNEL_PID, tai.modid, 0,
                                         PHASE_SITE, original, 4);
        if (memcmp((const void *)(text + PHASE_SITE), original, 4) != 0)
            fail_stop(fd);
    }
    int restored = install_restore_irqs(&mask, ksceKernelResumeIntr);
    if (restored < 0)
        fail_stop(fd);
    size = snprintf(line, sizeof(line),
        "patch=%08x verified=%u restore=%08x boot_ms=%u site=%08x\n",
        (unsigned)patch_id, (unsigned)verified, (unsigned)restored,
        (unsigned)(ksceKernelGetSystemTimeWide() / 1000), (unsigned)PHASE_SITE);
    if (size > 0 && (unsigned)size < sizeof(line))
        (void)write_all(fd, line, (unsigned)size);
    (void)ksceIoClose(fd);
    /* Remain resident even after a failed injection and verified rollback. */
    return SCE_KERNEL_START_SUCCESS;

rejected:
    {
        static const char message[] = "installation_rejected; no patch attempted\n";
        (void)write_all(fd, message, sizeof(message) - 1);
    }
    (void)ksceIoClose(fd);
    return SCE_KERNEL_START_NO_RESIDENT;
}

int module_stop(SceSize argc, const void *args)
{
    (void)argc;
    (void)args;
    return SCE_KERNEL_STOP_CANCEL;
}
