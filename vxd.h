/******************************************************************************
 * Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>                    *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/
#ifndef __VXD_H__INCLUDED__
#define __VXD_H__INCLUDED__

/* Table of "known" drivers
 * https://fd.lod.bz/rbil/interrup/windows/2f1684.html#sect-4579
 * This looks like free
 */

#define VXD_DEVICE_ID 0x2961
#define VXD_DEVICE_NAME "SMP"

#define VXD_MAJOR_VER           4
#define VXD_MINOR_VER           0

#define VXD_PM16_VERSION        0
#define VXD_PM16_VMM_VERSION    1

#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include "vmm.h"
#include "vpicd.h"
#include "vxd_lib.h"
#include "vxd_debug.h"
#include "vxd_terror.h"
#include "smp.h"
#include "taskswitch.h"
#include "smp9x.h"
#include "cpuid_smp.h"

#endif /* __VXD_H__INCLUDED__ */
