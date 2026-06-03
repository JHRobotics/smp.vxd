#ifndef __VXD_H__INCLUDED__
#define __VXD_H__INCLUDED__

/* Table of "known" drivers
 * https://fd.lod.bz/rbil/interrup/windows/2f1684.html#sect-4579
 * This looks like free
 */

#define VXD_DEVICE_ID 0x2961
#define VXD_DEVICE_NAME "SMP"

#define VXD_MAJOR_VER 4
#define VXD_MINOR_VER 0

#define VXD_PM16_VERSION                      0
#define VXD_PM16_VMM_VERSION                  1

#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include "vmm.h"
#include "vpicd.h"
#include "vxd.h"
#include "vxd_lib.h"
#include "vxd_debug.h"
#include "vxd_terror.h"
#include "smp.h"
#include "taskswitch.h"
#include "smp9x.h"

#endif /* __VXD_H__INCLUDED__ */
