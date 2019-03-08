/*******************************************************************************
* Copyright (c) 2015-2019 Cadence Design Systems, Inc.
* 
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to use this Software with Cadence processor cores only and 
* not with any other processors and platforms, subject to
* the following conditions:
* 
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************************/

#ifdef __XCC__
#ifdef __TOOLS_RF2__
#include <xtensa/xos/xos.h>
#else   // #ifdef __TOOLS_RF2__
#include <xtensa/xos.h>
#endif  // #ifdef __TOOLS_RF2__
#endif  // #ifdef __XCC__

#include "xa_type_def.h"

/* ...debugging facility */
#include "xf-debug.h"

#include "xaf-test.h"
#include "xaf-api.h"
#include "xaf-mem.h"

#ifdef __XCC__
#include <xtensa/hal.h>

#ifdef __TOOLS_RF2__
#define TOOLS_SUFFIX    "_RF2"
#else
#define TOOLS_SUFFIX    "_RF3"
#endif

#if XCHAL_HAVE_HIFI4
#define BUILD_STRING "XTENSA_HIFI4" TOOLS_SUFFIX
#elif XCHAL_HAVE_HIFI3
#define BUILD_STRING "XTENSA_HIFI3" TOOLS_SUFFIX
#elif XCHAL_HAVE_HIFI_MINI
#define BUILD_STRING "XTENSA_HIFI_MINI" TOOLS_SUFFIX
#elif XCHAL_HAVE_HIFI2EP
#ifdef RTK_HW
#define BUILD_STRING "XTENSA_HIFIEP_RTK_HW" TOOLS_SUFFIX
#else
#define BUILD_STRING "XTENSA_HIFIEP_RTK" TOOLS_SUFFIX
#endif
#elif XCHAL_HAVE_HIFI2
#define BUILD_STRING "XTENSA_HIFI2" TOOLS_SUFFIX
#else
#define BUILD_STRING "XTENSA_NON_HIFI" TOOLS_SUFFIX
#endif

#else
#define BUILD_STRING "NON_XTENSA"
#endif


#ifdef XAF_PROFILE
#include <xtensa/sim.h>
#include "xaf-clk-test.h"
#endif

#ifndef STACK_SIZE 
#define STACK_SIZE          8192
#endif

#define DEVICE_ID    (0x180201FC) //Vender ID register for realtek board
#define VENDER_ID    (0x10EC)     //Vender ID value indicating realtek board

void set_wbna(int *argc, const char **argv);
int print_verinfo(pUWORD8 ver_info[],pUWORD8 app_name);
int read_input(void *p_buf, int buf_length, int *read_length, void *p_input, xaf_comp_type comp_type);
double compute_comp_mcps(unsigned int num_bytes, int comp_cycles, xaf_format_t comp_format, double *strm_duration);
int print_mem_mcps_info(mem_obj_t* mem_handle, int num_comp);
int comp_process_entry(void *arg, int wake_value);
unsigned short start_xos();
