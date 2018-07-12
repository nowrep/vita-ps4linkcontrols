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
static SceUID g_injects[1];

// vs0:app/NPXS10013/keymap
static int keymap_number = 0;
// RP-ControllerType
static int controller_type = 1;

// TheOfficialFloW/Adrenaline user/utils.c
#define THUMB_SHUFFLE(x) ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))
uint32_t encode_movw(uint8_t rd, uint16_t imm16)
{
    uint32_t imm4 = (imm16 >> 12) & 0xF;
    uint32_t i = (imm16 >> 11) & 0x1;
    uint32_t imm3 = (imm16 >> 8) & 0x7;
    uint32_t imm8 = imm16 & 0xFF;
    return THUMB_SHUFFLE(0xF2400000 | (rd << 8) | (i << 26) | (imm4 << 16) | (imm3 << 12) | imm8);
}

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
        // keymap
        int ret = sceIoRead(fd, &value, sizeof(value));
        if (ret == 1 && value >= '0' && value <= '7') {
            keymap_number = value - '0';
        }
        // newline
        ret = sceIoRead(fd, &value, sizeof(value));
        if (ret == 1) {
            // controller_type
            ret = sceIoRead(fd, &value, sizeof(value));
            if (ret == 1 && value >= '0' && value <= '9') {
                controller_type = value - '0';
            }
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

    uint32_t offsets[4];

    switch (info.module_nid) {
    case 0x48A4A1C1: // 3.60
        offsets[0] = 0x47a4c;
        offsets[1] = 0x4612c;
        offsets[2] = 0x457a8;
        offsets[3] = 0x48C10;
        break;

    case 0x4bc536e4: // 3.65
    case 0x75CFBD26: // 3.68
        offsets[0] = 0x47d2c;
        offsets[1] = 0x4640c;
        offsets[2] = 0x45a88;
        offsets[3] = 0x48EF0;
        break;

    default:
        LOG("ScePS4Link %X NID not recognized", info.module_nid);
        return SCE_KERNEL_START_FAILED;
    }

    load_config();

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

    // Replace LDR.W R11 [R9, 0xD24] = D9 F8 24 BD
    //      to MOVW R11, controller_type
    uint32_t mov_r11_opcode = encode_movw(11, controller_type);
    g_injects[0] = taiInjectData(info.modid,
                                 0,          // segidx
                                 offsets[3], // offset
                                 &mov_r11_opcode,
                                 sizeof(mov_r11_opcode));

    LOG("Using keymap %d", keymap_number);
    LOG("Using controller type %d", controller_type);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    LOG("Stopping module");

    if (g_hooks[0] >= 0) taiHookRelease(g_hooks[0], ref_hook0);
    if (g_hooks[1] >= 0) taiHookRelease(g_hooks[1], ref_hook1);
    if (g_hooks[2] >= 0) taiHookRelease(g_hooks[2], ref_hook2);

    if (g_injects[0] >= 0) taiInjectRelease(g_injects[0]);

    return SCE_KERNEL_STOP_SUCCESS;
}
