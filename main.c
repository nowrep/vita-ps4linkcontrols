/*
 *  PS4LinkControls plugin
 *  Copyright (c) 2017 David Rosca
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
#include "log.h"

#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>
#include <taihen.h>

static SceUID g_hooks[3];

// vs0:app/NPXS10013/keymap
static int keymap_number = 0;

struct RPJob {
    char padding[436];
    int mode;
};

struct S1 {
    char padding[141];
    char image_loaded;
};

static tai_hook_ref_t ref_hook0;
static int load_keymap_image_patched(struct S1 *a1, int a2)
{
    a1->image_loaded = 0;
    return TAI_CONTINUE(int, ref_hook0, a1, keymap_number);
}

static tai_hook_ref_t ref_hook1;
static int update_rp_parameters_patched(struct RPJob *a1, int *a2)
{
    int out = TAI_CONTINUE(int, ref_hook1, a1, a2);
    a1->mode = keymap_number;
    return out;
}

static tai_hook_ref_t ref_hook2;
static int rpjob_constructor_patched(struct RPJob *a1, int a2, int a3)
{
    int out = TAI_CONTINUE(int, ref_hook2, a1, a2, a3);
    a1->mode = keymap_number;
    return out;
}

static void load_config()
{
    SceUID fd = sceIoOpen("ux0:ps4linkcontrols.txt", SCE_O_RDONLY, 0);
    if (fd >= 0) {
        char value = 0;
        sceIoRead(fd, &value, sizeof(value));
        if (value >= '0' && value <= '7') {
            keymap_number = value - '0';
        }
        sceIoClose(fd);
    }
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args)
{
    LOG("Starting module");

    tai_module_info_t info;
    info.size = sizeof(info);
    int ret = taiGetModuleInfo("ScePS4Link", &info);
    if (ret < 0) {
        LOG("taiGetModuleInfo error %X", ret);
        return SCE_KERNEL_START_FAILED;
    }

    uint32_t offsets[3];

    switch (info.module_nid) {
    case 0x4bc536e4: // 3.65
        offsets[0] = 0x47d2c;
        offsets[1] = 0x4640c;
        offsets[2] = 0x45a88;
        break;

    default:
        // FIXME: I don't know 3.60 NID so everything else falls back to 3.60 offsets
        offsets[0] = 0x47a4c;
        offsets[1] = 0x4612c;
        offsets[2] = 0x457a8;
        break;
    }

    g_hooks[0] = taiHookFunctionOffset(&ref_hook0,
                                       info.modid,
                                       0,          // segidx
                                       offsets[0], // offset
                                       1,          // thumb
                                       load_keymap_image_patched);

    g_hooks[1] = taiHookFunctionOffset(&ref_hook1,
                                       info.modid,
                                       0,          // segidx
                                       offsets[1], // offset
                                       1,          // thumb
                                       update_rp_parameters_patched);

    g_hooks[2] = taiHookFunctionOffset(&ref_hook2,
                                       info.modid,
                                       0,          // segidx
                                       offsets[2], // offset
                                       1,          // thumb
                                       rpjob_constructor_patched);

    load_config();

    LOG("Using keymap %d", keymap_number);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    LOG("Stopping module");

    if (g_hooks[0] >= 0) taiHookRelease(g_hooks[0], ref_hook0);
    if (g_hooks[1] >= 0) taiHookRelease(g_hooks[1], ref_hook1);
    if (g_hooks[2] >= 0) taiHookRelease(g_hooks[2], ref_hook2);

    return SCE_KERNEL_STOP_SUCCESS;
}
