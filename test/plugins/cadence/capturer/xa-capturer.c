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
/*******************************************************************************
 * xa-capturer.c
 *
 * dummy (dumping data to file)capturer implementation
 *
 * Copyright (c) 2012 Tensilica Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#define MODULE_TAG                      CAPTURER

/*******************************************************************************
 * Includes
 ******************************************************************************/
#ifdef __XCC__
#ifdef __TOOLS_RF2__
#include <xtensa/xos/xos.h>
#include <xtensa/xos/xos_timer.h>
#else   // #ifdef __TOOLS_RF2__
#include <xtensa/xos.h>
#include <xtensa/xos_timer.h>
#endif  // #ifdef __TOOLS_RF2__
#endif  // #ifdef __XCC__
#include<stdio.h>
#include "audio/xa-capturer-api.h"
#include "xf-debug.h"
#include <string.h>
#ifdef XAF_PROFILE
#include "xaf-clk-test.h"
extern clk_t capturer_cycles;
#endif
#include "xaf-fio-test.h"
/*******************************************************************************
 * Tracing configuration
 ******************************************************************************/

TRACE_TAG(INIT, 1);
TRACE_TAG(WARNING, 1);

TRACE_TAG(OUTPUT, 0);
TRACE_TAG(INPUT, 1);

TRACE_TAG(FIFO, 1);
TRACE_TAG(ISR, 1);
TRACE_TAG(UNDERRUN, 1);



/*******************************************************************************
 * Codec parameters
 ******************************************************************************/

/* ...total length of HW FIFO in bytes */
#define HW_FIFO_LENGTH                  8192

/* ...total length of HW FIFO in samples */
#define HW_FIFO_LENGTH_SAMPLES          (HW_FIFO_LENGTH / (2 * sizeof(WORD16)))

#ifdef RTK_HW
/*...i2s is taking samples from fifo at 48kHz*/
#define HW_SAMPLE_NUM              1024
#define HW_BYTES_PER_SAMPLE        2
#define HW_IN_BUF_SIZE             (2 * HW_BYTES_PER_SAMPLE * HW_SAMPLE_NUM)


#define STRM_1_L_IN     0x18001000
#define STRM_1_R_IN     0x18002000

#endif

#define I2S_SF (44100)


/*******************************************************************************
 * Local data definition
 ******************************************************************************/
#ifdef RTK_HW
/* global variables */
char ghw_inBuffer0[HW_IN_BUF_SIZE] __attribute__((section (" .dram0.bss")));
char ghw_inBuffer1[HW_IN_BUF_SIZE] __attribute__((section (" .dram0.bss")));

unsigned int ghw_Linbuffer_RP __attribute__((section (" .dram0.bss")));
unsigned int ghw_Rinbuffer_RP __attribute__((section (" .dram0.bss")));
#endif
typedef struct XACapturer
{
    /***************************************************************************
     * Internal stuff
     **************************************************************************/

    /* ...component state */
    UWORD32                     state;

    /* ...notification callback pointer */
    xa_capturer_cb_t       *cdata;

    /* ...input buffer pointer */
    void                   *output;
#ifndef RTK_HW
    /* ...estimation of amount of samples that can be written into FIFO */
    UWORD32                     fifo_avail;

    UWORD32                   capturer_eof;
#endif

    /* ...number of samples produced */
    UWORD32                     produced;
    /* ...number of bytes copied in fifo*/
    //UWORD32                     submited;
    /***************************************************************************
     * Run-time data
     **************************************************************************/

    /* ...size of PCM sample (respects channels and PCM width) */
    UWORD32                     sample_size;

    /* ...number of channels */
    UWORD32                     channels;

    /* ...sample width */
    UWORD32                     pcm_width;

    /* ...current sampling rate */
    UWORD32                     rate;

    UWORD32             over_flow_cnt;

    UWORD32            over_flow_flag;

    /* ...flag for detecting underrun..made to non zero over submit */
    //UWORD32              submit_flag;
#ifndef RTK_HW
    FILE * fw;
#endif

    UWORD32        interrupt_cnt;

    /*total bytes produced by the component*/
    UWORD64              tot_bytes_produced;

    /*total bytes to be produced*/
    UWORD64            bytes_end;

}   XACapturer;

/*******************************************************************************
 * Operating flags
 ******************************************************************************/

#define XA_CAPTURER_FLAG_PREINIT_DONE   (1 << 0)
#define XA_CAPTURER_FLAG_POSTINIT_DONE  (1 << 1)
#define XA_CAPTURER_FLAG_IDLE           (1 << 2)
#define XA_CAPTURER_FLAG_RUNNING        (1 << 3)
#define XA_CAPTURER_FLAG_PAUSED         (1 << 4)
/*******************************************************************************
 * global variables
 ******************************************************************************/
#ifndef RTK_HW
XosTimer rend_timer;
const char capturer_in_file[] = "capturer_in.pcm";
#endif
void xa_capturer_callback(xa_capturer_cb_t *cdata, WORD32 i_idx);

#ifdef RTK_HW
/*******************************************************************************
 * RTK_StreamIO_Init
 *
 * Outbount fifo registers are configured here.
 ******************************************************************************/
void RTK_StreamIO_Init()
{
    int ret;
    xthal_set_region_attribute((void *)0x18000000, 65536, XCHAL_CA_BYPASS, 0);
    ret = xthal_set_region_attribute((void *)0x18010000, 65536, XCHAL_CA_BYPASS, 0);

    *((volatile unsigned int*) 0x1801f000) = 0x3;


    // reset STRM_1_L_IN
    *((volatile unsigned int*) (STRM_1_L_IN + 0x28)) = 0x1;
    *((volatile unsigned int*) (STRM_1_L_IN + 0x28)) = 0x0;
    memset(ghw_inBuffer0, 0x0, HW_IN_BUF_SIZE);
    ghw_Linbuffer_RP = (unsigned int)ghw_inBuffer0;

    //STRM_1_L_IN prog---------------------------------------------------

    //STRM_1_L_IN start_addr=ghw_inBuffer0
    *((volatile unsigned int*) STRM_1_L_IN)=(unsigned int)ghw_inBuffer0;

    //STRM_1_L_IN end_addr=(ghw_inBuffer0 + HW_IN_BUF_SIZE - HW_BYTES_PER_SAMPLE)
    *((volatile unsigned int*) (STRM_1_L_IN + 0x4) )=(unsigned int)(ghw_inBuffer0 + HW_IN_BUF_SIZE - HW_BYTES_PER_SAMPLE);

    //STRM_1_L_IN enable, direct mode, tag=1, mono_enable
    *((volatile unsigned int*) (STRM_1_L_IN + 0x10) ) = 0x00010803 /*0x00010801*/;

    //STRM_1_L_IN frame size = HW_SAMPLE_NUM
    *((volatile unsigned int*) (STRM_1_L_IN + 0x30) ) = HW_SAMPLE_NUM;


    // reset STRM_1_R_IN
    *((volatile unsigned int*) (STRM_1_R_IN + 0x28)) = 0x1;
    *((volatile unsigned int*) (STRM_1_R_IN + 0x28)) = 0x0;
    memset(ghw_inBuffer1, 0x0, HW_IN_BUF_SIZE);
    ghw_Rinbuffer_RP = (unsigned int)ghw_inBuffer1;

    //STRM_1_R_IN prog---------------------------------------------------

    //STRM_1_R_IN start_addr=ghw_inBuffer1
    *((volatile unsigned int*) STRM_1_R_IN)=(unsigned int)ghw_inBuffer1;

    //STRM_1_R_IN end_addr=(ghw_inBuffer1 + HW_IN_BUF_SIZE - HW_BYTES_PER_SAMPLE)
    *((volatile unsigned int*) (STRM_1_R_IN + 0x4) )=(unsigned int)(ghw_inBuffer1 + HW_IN_BUF_SIZE - HW_BYTES_PER_SAMPLE);

    //STRM_1_R_IN enable, direct mode, tag=3, mono_enable
    *((volatile unsigned int*) (STRM_1_R_IN + 0x10) ) = 0x00030803/*0x00030801*/;

    //STRM_1_R_IN frame size = HW_SAMPLE_NUM
    *((volatile unsigned int*) (STRM_1_R_IN + 0x30) ) = HW_SAMPLE_NUM;



}

#endif




#ifndef RTK_HW

static void xa_fw_handler(void *arg)
{
    XACapturer *d = arg;

    d->fifo_avail = d->fifo_avail + (HW_FIFO_LENGTH_SAMPLES>>1);

    if((d->fifo_avail)>HW_FIFO_LENGTH_SAMPLES)
    {/*over run case*/
       d->fifo_avail = 0;
    }
    else if(((int)d-> fifo_avail) < 0)
    {/* under run */
       d->fifo_avail=0;
    }
    else
    {
       d->cdata->cb(d->cdata, 0);

    }
}
#else
static void xa_hw_handler(void* ptr)
{
    int i;
    int j =0;
    XACapturer *d = ptr;

    d->interrupt_cnt++;
    i = (int)(ghw_inBuffer0 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    if(d->over_flow_flag == 1)
    {
        d->over_flow_cnt++;
        if(ghw_Linbuffer_RP == i)
        {
            ghw_Linbuffer_RP = (unsigned int)ghw_inBuffer0;  //buffer ring to start addr
        }
        else
        {
            ghw_Linbuffer_RP=ghw_Linbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
        }
        i = (int)(ghw_inBuffer1 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
        if(ghw_Rinbuffer_RP == i)
        {
             ghw_Rinbuffer_RP = (unsigned int)ghw_inBuffer1;  //buffer ring to start addr
        }
        else
        {
            ghw_Rinbuffer_RP=ghw_Rinbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
        }

    }

    for(i=0;i<512;i++)
    {
               j++;
    }
	i = (int)(ghw_inBuffer0 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    if(ghw_Linbuffer_RP == i)
    {
        *((volatile unsigned int*) (STRM_1_L_IN + 0x8) ) = *((volatile unsigned int*) (STRM_1_L_IN + 0x4) );
    }
    else
    {
        *((volatile unsigned int*) (STRM_1_L_IN + 0x8) ) = ghw_Linbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    }

    i = (int)(ghw_inBuffer1 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    if(ghw_Rinbuffer_RP == i)
    {
        *((volatile unsigned int*) (STRM_1_R_IN + 0x8) ) = *((volatile unsigned int*) (STRM_1_R_IN + 0x4) );

    }
    else
    {
        *((volatile unsigned int*) (STRM_1_R_IN + 0x8) ) = ghw_Rinbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    }

    if( d->over_flow_flag == 0 )
    {
        d->over_flow_flag = 1;
        d->cdata->cb(d->cdata, 0);
    }
}
#endif






/*******************************************************************************
 * Codec access functions
 ******************************************************************************/
#ifndef RTK_HW
static inline void xa_fw_capturer_close(XACapturer *d)
{
    fclose(d->fw);
    xos_timer_stop(&rend_timer);
}
#else
static inline void xa_hw_capturer_close(XACapturer *d)
{
    *((volatile unsigned int*) (STRM_1_L_IN + 0x10) ) = 0;
    *((volatile unsigned int*) (STRM_1_R_IN + 0x10) ) = 0;
}
#endif
/* ...submit data (in samples) into internal capturer ring-buffer */

/*******************************************************************************
 * API command hooks
 ******************************************************************************/

/* ...standard codec initialization routine */
static XA_ERRORCODE xa_capturer_get_api_size(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...check parameters are sane */
    XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...retrieve API structure size */
    *(WORD32 *)pv_value = sizeof(*d);

    return XA_NO_ERROR;
}
#ifndef RTK_HW
static XA_ERRORCODE xa_fw_capturer_init (XACapturer *d)
{
   d->produced = 0;
   d->fw = NULL;
   d->tot_bytes_produced = 0;
   /*opening the output file*/
   d->fw = fopen(capturer_in_file,"rb");
   if ( d->fw == NULL )
   {
     /*file open failed*/
     return XA_FATAL_ERROR;
   }

   /*initialy FIFO will be empty so fifo_avail = HW_FIFO_LENGTH_SAMPLES*/
   d->fifo_avail = 0;

   /*initialises the timer ;timer0 is used as system timer*/
   xos_timer_init(&rend_timer);

   return XA_NO_ERROR;
}
#else
static XA_ERRORCODE xa_hw_capturer_init (XACapturer *d)
{
    int status;
    d->produced = 0;
    d->tot_bytes_produced = 0;
    d->interrupt_cnt = 0;
    d->over_flow_flag = 0;
    d->over_flow_cnt = 0;

    return 0;
}
#endif

/* ...standard codec initialization routine */
static XA_ERRORCODE xa_capturer_init(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    int status;
    /* ...sanity check - pointer must be valid */
    XF_CHK_ERR(d, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...process particular initialization type */
    switch (i_idx)
    {
    case XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS:
    {
        /* ...pre-configuration initialization; reset internal data */
        memset(d, 0, sizeof(*d));

        /* ...set default capturer parameters - 16-bit little-endian stereo @ 48KHz */
        d->sample_size = 2;
        d->channels = 2;
        d->pcm_width = 16;
        d->rate = 48000;

        /* ...and mark capturer has been created */
        d->state = XA_CAPTURER_FLAG_PREINIT_DONE;

        return XA_NO_ERROR;
    }

    case XA_CMD_TYPE_INIT_API_POST_CONFIG_PARAMS:
    {
        /* ...post-configuration initialization (all parameters are set) */

        XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PREINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);
#ifndef RTK_HW

        XF_CHK_ERR(xa_fw_capturer_init(d) == 0, XA_CAPTURER_CONFIG_FATAL_HW);
#else
        XF_CHK_ERR(xa_hw_capturer_init(d) == 0, XA_CAPTURER_CONFIG_FATAL_HW);
#endif
        /* ...mark post-initialization is complete */
        d->state |= XA_CAPTURER_FLAG_POSTINIT_DONE;

        return XA_NO_ERROR;
    }

    case XA_CMD_TYPE_INIT_PROCESS:
    {
        /* ...kick run-time initialization process; make sure setup is complete */
        XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);
        /* ...mark capturer is in idle state */
        d->state |= XA_CAPTURER_FLAG_IDLE;
#ifndef RTK_HW
        xos_timer_start(&rend_timer,((long long)(HW_FIFO_LENGTH_SAMPLES>>1)*xos_get_clock_freq()/I2S_SF)/* 2133334*/,1,xa_fw_handler,d);
#else
        RTK_StreamIO_Init();
        status = xos_register_interrupt_handler(XCHAL_EXTINT5_NUM,xa_hw_handler,d);

#ifdef __TOOLS_RF2__
        xos_enable_ints(1<<5);
#else
        xos_interrupt_enable(XCHAL_EXTINT5_NUM);
#endif

#endif

        return XA_NO_ERROR;
    }

    case XA_CMD_TYPE_INIT_DONE_QUERY:
    {
        /* ...check if initialization is done; make sure pointer is sane */
        XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

        /* ...put current status */
        *(WORD32 *)pv_value = (d->state & XA_CAPTURER_FLAG_IDLE ? 1 : 0);

        d->state ^= XA_CAPTURER_FLAG_IDLE | XA_CAPTURER_FLAG_RUNNING;
        return XA_NO_ERROR;
    }

    default:
        /* ...unrecognized command type */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...HW-capturer control function */
static inline XA_ERRORCODE xa_hw_capturer_control(XACapturer *d, UWORD32 state)
{
    switch (state)
    {
    case XA_CAPTURER_STATE_RUN:
        /* ...capturer must be in paused state */
        XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PAUSED, XA_CAPTURER_EXEC_NONFATAL_STATE);

        /* ...resume capturer operation */
        //xa_hw_capturer_resume(d);

        /* ...mark capturer is running */
        d->state ^= XA_CAPTURER_FLAG_RUNNING | XA_CAPTURER_FLAG_PAUSED;

        return XA_NO_ERROR;

    case XA_CAPTURER_STATE_PAUSE:
        /* ...capturer must be in running state */
        XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_RUNNING, XA_CAPTURER_EXEC_NONFATAL_STATE);

        /* ...pause capturer operation */


        /* ...mark capturer is paused */
        d->state ^= XA_CAPTURER_FLAG_RUNNING | XA_CAPTURER_FLAG_PAUSED;

        return XA_NO_ERROR;

    case XA_CAPTURER_STATE_IDLE:
        /* ...command is valid in any active state; stop capturer operation */
#ifndef RTK_HW
        xa_fw_capturer_close(d);
#else
        xa_hw_capturer_close(d);
#endif
               /* ...reset capturer flags */
        d->state &= ~(XA_CAPTURER_FLAG_RUNNING | XA_CAPTURER_FLAG_PAUSED);

        return XA_NO_ERROR;

    default:
        /* ...unrecognized command */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...set capturer configuration parameter */
static XA_ERRORCODE xa_capturer_set_config_param(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    UWORD32     i_value;

    /* ...sanity check - pointers must be sane */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...pre-initialization must be completed */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PREINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    /* ...process individual configuration parameter */
    switch (i_idx)
    {
    case XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_CAPTURER_FLAG_POSTINIT_DONE) == 0, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested PCM width */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...check value is permitted (16 bits only) */
        XF_CHK_ERR(i_value == 16, XA_CAPTURER_CONFIG_NONFATAL_RANGE);

        /* ...apply setting */
        d->pcm_width = i_value;

        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_CHANNELS:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_CAPTURER_FLAG_POSTINIT_DONE) == 0, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested channel number */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...allow stereo only */
        XF_CHK_ERR(i_value == 2, XA_CAPTURER_CONFIG_NONFATAL_RANGE);

        /* ...apply setting */
        d->channels = (UWORD32)i_value;

        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_CAPTURER_FLAG_POSTINIT_DONE) == 0, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested sampling rate */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...allow 44.1 and 48KHz only - tbd */
        XF_CHK_ERR(i_value == 44100 || i_value == 48000, XA_CAPTURER_CONFIG_NONFATAL_RANGE);

        /* ...apply setting */
        d->rate = (UWORD32)i_value;

        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_FRAME_SIZE:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_CAPTURER_FLAG_POSTINIT_DONE) == 0, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested frame size */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...check it is equal to the only frame size we support */
        XF_CHK_ERR(i_value == HW_FIFO_LENGTH_SAMPLES / 2, XA_CAPTURER_CONFIG_NONFATAL_RANGE);

        return XA_NO_ERROR;
    case XA_CAPTURER_CONFIG_PARAM_SAMPLE_END:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_CAPTURER_FLAG_POSTINIT_DONE) == 0, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested frame size */
        d->bytes_end = ((UWORD64) *(UWORD64 *)pv_value)* (d->sample_size) * (d->channels) ;

        /* ...check it is equal to the only frame size we support */
        XF_CHK_ERR(i_value == HW_FIFO_LENGTH_SAMPLES / 2, XA_CAPTURER_CONFIG_NONFATAL_RANGE);

        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_CB:
        /* ...set opaque callback data function */
        d->cdata = (xa_capturer_cb_t *)pv_value;

        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_STATE:
        /* ...runtime state control parameter valid only in execution state */
        XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

        /* ...get requested state */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...pass to state control hook */
        return xa_hw_capturer_control(d, i_value);

    default:
        /* ...unrecognized parameter */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...state retrieval function */
static inline UWORD32 xa_hw_capturer_get_state(XACapturer *d)
{
    if (d->state & XA_CAPTURER_FLAG_RUNNING)
        return XA_CAPTURER_STATE_RUN;
    else if (d->state & XA_CAPTURER_FLAG_PAUSED)
        return XA_CAPTURER_STATE_PAUSE;
    else
        return XA_CAPTURER_STATE_IDLE;
}

/* ...retrieve configuration parameter */
static XA_ERRORCODE xa_capturer_get_config_param(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - capturer must be initialized */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...make sure pre-initialization is completed */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PREINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    /* ...process individual configuration parameter */
    switch (i_idx)
    {
    case XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH:
        /* ...return current PCM width */
        *(WORD32 *)pv_value = d->pcm_width;
        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_CHANNELS:
        /* ...return current channel number */
        *(WORD32 *)pv_value = d->channels;
        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE:
        /* ...return current sampling rate */
        *(WORD32 *)pv_value = d->rate;
        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_FRAME_SIZE:
        /* ...return current audio frame length (in samples) */
        *(WORD32 *)pv_value = HW_FIFO_LENGTH_SAMPLES / 2;
        return XA_NO_ERROR;

    case XA_CAPTURER_CONFIG_PARAM_STATE:
        /* ...return current execution state */
        *(WORD32 *)pv_value = xa_hw_capturer_get_state(d);
        return XA_NO_ERROR;
    case XA_CAPTURER_CONFIG_PARAM_BYTES_PRODUCED:
        /* ...return current execution state */
        *(WORD32 *)pv_value = d->tot_bytes_produced;
        return XA_NO_ERROR;
    default:
        /* ...unrecognized parameter */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...execution command */
static XA_ERRORCODE xa_capturer_execute(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - pointer must be valid */
    XF_CHK_ERR(d, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...capturer must be in running state */
    XF_CHK_ERR(d->state & (XA_CAPTURER_FLAG_RUNNING | XA_CAPTURER_FLAG_IDLE), XA_CAPTURER_EXEC_FATAL_STATE);

    /* ...process individual command type */
    switch (i_idx)
    {
    case XA_CMD_TYPE_DO_EXECUTE:
        /* ...silently ignore; everything is done in "set_input" */
        return XA_NO_ERROR;

    case XA_CMD_TYPE_DONE_QUERY:
        /* ...always report "no" - tbd - is that needed at all? */
        XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

        if((d->produced == 0))
        {
#ifndef RTK_HW
            xos_timer_stop(&rend_timer);
#else
#ifdef __TOOLS_RF2__
		    xos_disable_ints(1<<5);
#else
            xos_interrupt_disable(XCHAL_EXTINT5_NUM);
#endif
#endif
            *(WORD32 *)pv_value = 1;

        }
        else
        {
           *(WORD32 *)pv_value = 0;
        }

        return XA_NO_ERROR;

    case XA_CMD_TYPE_DO_RUNTIME_INIT:
        /* ...silently ignore */
        return XA_NO_ERROR;

    default:
        /* ...unrecognized command */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...get number of produced bytes */
static XA_ERRORCODE xa_capturer_get_output_bytes(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - check parameters */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...input buffer index must be valid */
    XF_CHK_ERR(i_idx == 0, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...capturer must be in post-init state */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_CAPTURER_EXEC_FATAL_STATE);

    /* ...input buffer must exist */
    XF_CHK_ERR(d->output, XA_CAPTURER_EXEC_FATAL_INPUT);

    /* ...return number of bytes produced */
    *(WORD32 *)pv_value = d->produced;

    d->produced = 0;



    return XA_NO_ERROR;
}

/*******************************************************************************
 * Memory information API
 ******************************************************************************/

/* ..get total amount of data for memory tables */
static XA_ERRORCODE xa_capturer_get_memtabs_size(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...check capturer is pre-initialized */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PREINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    /* ...we have all our tables inside API structure */
    *(WORD32 *)pv_value = 0;

    return XA_NO_ERROR;
}

/* ..set memory tables pointer */
static XA_ERRORCODE xa_capturer_set_memtabs_ptr(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...check capturer is pre-initialized */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_PREINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    /* ...do not do anything; just return success - tbd */
    return XA_NO_ERROR;
}

/* ...return total amount of memory buffers */
static XA_ERRORCODE xa_capturer_get_n_memtabs(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...we have 1 input buffer only */
    *(WORD32 *)pv_value = 1;

    return XA_NO_ERROR;
}

/* ...return memory buffer data */
static XA_ERRORCODE xa_capturer_get_mem_info_size(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    UWORD32     i_value;

    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    switch (i_idx)
    {
    case 0:
        /* ...input buffer specification; accept exact audio frame */
        i_value = HW_FIFO_LENGTH / 2;
        break;

    default:
        /* ...invalid index */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }

    /* ...return buffer size to caller */
    *(WORD32 *)pv_value = (WORD32) i_value;

    return XA_NO_ERROR;
}

/* ...return memory alignment data */
static XA_ERRORCODE xa_capturer_get_mem_info_alignment(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    /* ...all buffers are at least 4-bytes aligned */
    *(WORD32 *)pv_value = 4;

    return XA_NO_ERROR;
}

/* ...return memory type data */
static XA_ERRORCODE xa_capturer_get_mem_info_type(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_CAPTURER_CONFIG_FATAL_STATE);

    switch (i_idx)
    {
    case 0:
        /* ...input buffers */
        *(WORD32 *)pv_value = XA_MEMTYPE_OUTPUT;
        return XA_NO_ERROR;

    default:
        /* ...invalid index */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}
#ifdef RTK_HW
static inline UWORD32 xa_hw_capturer_read_FIFO(XACapturer *d)
{
    WORD16* out_buffer_ptr = NULL;
    UWORD32 i;

    d->over_flow_flag = 0;
    out_buffer_ptr = d->output;
    for(i=0 ; i< HW_SAMPLE_NUM ;i++)
    {

        *((unsigned short*) out_buffer_ptr)=  *((unsigned short*) ghw_Linbuffer_RP+i);
        out_buffer_ptr++;
        *((unsigned short*) out_buffer_ptr) = *((unsigned short*) ghw_Rinbuffer_RP+i);
        out_buffer_ptr++;
    }
    i = (int)(ghw_inBuffer0 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    if(ghw_Linbuffer_RP == i)
    {
         ghw_Linbuffer_RP = (unsigned int)ghw_inBuffer0;  //buffer ring to start addr

    }
    else
    {
        ghw_Linbuffer_RP=ghw_Linbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);

    }

    i = (int)(ghw_inBuffer1 + HW_IN_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    if(ghw_Rinbuffer_RP == i)
    {

        ghw_Rinbuffer_RP = (unsigned int)ghw_inBuffer1;  //buffer ring to start addr

    }
    else
    {
        ghw_Rinbuffer_RP=ghw_Rinbuffer_RP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);

    }

    (*((volatile unsigned int*) (STRM_1_L_IN + 0x34))) = 0;
    (*((volatile unsigned int*) (STRM_1_R_IN + 0x34))) = 0;
    return (HW_SAMPLE_NUM << 2);
}
#endif
/* ...set memory pointer */
static XA_ERRORCODE xa_capturer_set_mem_ptr(XACapturer *d, WORD32 i_idx, pVOID pv_value)
{
    WORD32 bytes_read = 0;
    static UWORD32 frame_cnt = 0;
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...codec must be initialized */
    XF_CHK_ERR(d->state & XA_CAPTURER_FLAG_POSTINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);

    TRACE(INIT, _b("xa_capturer_set_mem_ptr[%u]: %p"), i_idx, pv_value);

    /* ...select memory buffer */
    switch (i_idx)
    {
    case 0:
        /* ...input buffer */
        d->output = pv_value;

#ifndef RTK_HW
        FIO_PRINTF(stdout,"%d\n",++frame_cnt);
        if(d->fifo_avail >= (HW_FIFO_LENGTH_SAMPLES>>1))
        {
            d->fifo_avail = d->fifo_avail - (HW_FIFO_LENGTH_SAMPLES>>1);
            bytes_read = fread(d->output,1,(HW_FIFO_LENGTH_SAMPLES<<1), d->fw );
            d->produced = bytes_read;
            d->tot_bytes_produced = d->tot_bytes_produced + d->produced;
            if(d->bytes_end!=0)
            {
                if( d->tot_bytes_produced > d->bytes_end )
                {
                    if(d->produced > ( d->tot_bytes_produced - d->bytes_end ))
                    {
                        d->produced = d->produced - ( d->tot_bytes_produced - d->bytes_end );
                    }
                    else
                    {
                        d->produced = 0;
                    }
                }
            }

        }
        else
        {
            /*overrun happened*/
            d->produced = 0;

        }
#else
	    FIO_PRINTF(stdout,"%d\n",++frame_cnt);
        bytes_read = xa_hw_capturer_read_FIFO(d);
        d->produced = bytes_read;
        d->tot_bytes_produced = d->tot_bytes_produced + d->produced;
        if(d->bytes_end!=0)
        {
            if( d->tot_bytes_produced > d->bytes_end )
                {
                    if(d->produced > ( d->tot_bytes_produced - d->bytes_end ))
                    {
                        d->produced = d->produced - ( d->tot_bytes_produced - d->bytes_end );
                    }
                    else
                    {
                        d->produced = 0;
                    }
                }
        }
#endif
        return XA_NO_ERROR;

    default:
        /* ...invalid index */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}



/*******************************************************************************
 * API command hooks
 ******************************************************************************/
static XA_ERRORCODE (* const xa_capturer_api[])(XACapturer *, WORD32, pVOID) =
{
    [XA_API_CMD_GET_API_SIZE]           = xa_capturer_get_api_size,
    [XA_API_CMD_INIT]                   = xa_capturer_init,
    [XA_API_CMD_SET_CONFIG_PARAM]       = xa_capturer_set_config_param,
    [XA_API_CMD_GET_CONFIG_PARAM]       = xa_capturer_get_config_param,
    [XA_API_CMD_EXECUTE]                = xa_capturer_execute,
    [XA_API_CMD_GET_OUTPUT_BYTES]       = xa_capturer_get_output_bytes,
    [XA_API_CMD_GET_MEMTABS_SIZE]       = xa_capturer_get_memtabs_size,
    [XA_API_CMD_SET_MEMTABS_PTR]        = xa_capturer_set_memtabs_ptr,
    [XA_API_CMD_GET_N_MEMTABS]          = xa_capturer_get_n_memtabs,
    [XA_API_CMD_GET_MEM_INFO_SIZE]      = xa_capturer_get_mem_info_size,
    [XA_API_CMD_GET_MEM_INFO_ALIGNMENT] = xa_capturer_get_mem_info_alignment,
    [XA_API_CMD_GET_MEM_INFO_TYPE]      = xa_capturer_get_mem_info_type,
    [XA_API_CMD_SET_MEM_PTR]            = xa_capturer_set_mem_ptr,

};

/* ...total numer of commands supported */
#define XA_CAPTURER_API_COMMANDS_NUM   (sizeof(xa_capturer_api) / sizeof(xa_capturer_api[0]))

/*******************************************************************************
 * API entry point
 ******************************************************************************/

XA_ERRORCODE xa_capturer(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value)
{
    XA_ERRORCODE sh_test = 0;
    XACapturer *capturer = (XACapturer *) p_xa_module_obj;
#ifdef XAF_PROFILE
    clk_t comp_start, comp_stop;
#endif
    /* ...check if command index is sane */
    XF_CHK_ERR(i_cmd < XA_CAPTURER_API_COMMANDS_NUM, XA_API_FATAL_INVALID_CMD);

    /* ...see if command is defined */
    XF_CHK_ERR(xa_capturer_api[i_cmd], XA_API_FATAL_INVALID_CMD);

    /* ...execute requested command */
#ifdef XAF_PROFILE
    comp_start = clk_read_start(CLK_SELN_THREAD);
#endif
    sh_test = xa_capturer_api[i_cmd](capturer, i_idx, pv_value);
#ifdef XAF_PROFILE
    comp_stop = clk_read_stop(CLK_SELN_THREAD);
    capturer_cycles += clk_diff(comp_stop, comp_start);
#endif
    return sh_test;
}
