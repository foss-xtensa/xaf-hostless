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
 * xa-renderer.c
 *
 * dummy (dumping data to file)renderer implementation
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

#define MODULE_TAG                      RENDERER

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
#include "audio/xa-renderer-api.h"
#include "xf-debug.h"
#include <string.h>

#ifdef XAF_PROFILE
#include "xaf-clk-test.h"
extern clk_t renderer_cycles;
#endif

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
#define HW_SAMPLE_NUM				1024
#define HW_BYTES_PER_SAMPLE		2
#define HW_OUT_BUF_SIZE			(2 * HW_BYTES_PER_SAMPLE * HW_SAMPLE_NUM)	


#define HW_STRM_1_L_OUT	0x18010000
#define HW_STRM_1_R_OUT	0x18011000

#endif 

#define HW_I2S_SF (44100)

/*******************************************************************************
 * Local data definition
 ******************************************************************************/
#ifdef RTK_HW
/* global variables */
char ghw_outBuffer0[HW_OUT_BUF_SIZE] __attribute__((section (" .dram0.bss")));
char ghw_outBuffer1[HW_OUT_BUF_SIZE] __attribute__((section (" .dram0.bss")));
unsigned int ghw_Loutbuffer_WP __attribute__((section (" .dram0.bss")));
unsigned int ghw_Routbuffer_WP __attribute__((section (" .dram0.bss")));
#endif
typedef struct XARenderer
{
    /***************************************************************************
     * Internal stuff
     **************************************************************************/

    /* ...component state */
    UWORD32                     state;

    /* ...notification callback pointer */
    xa_renderer_cb_t       *cdata;

    /* ...input buffer pointer */
    void                   *input;
#ifndef RTK_HW
    /* ...estimation of amount of samples that can be written into FIFO */
    UWORD32                     fifo_avail;
#endif

    /* ...number of samples consumed */
    UWORD32                     consumed;
    /* ...number of bytes copied in fifo*/
    UWORD32                     submited;
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
	
	/* ...flag for detecting underrun..made to non zero over submit */
	UWORD32              submit_flag;
#ifndef RTK_HW
    FILE * fw;
#endif
    /*bytes produced*/
    UWORD64             bytes_produced;
}   XARenderer;

/*******************************************************************************
 * Operating flags
 ******************************************************************************/

#define XA_RENDERER_FLAG_PREINIT_DONE   (1 << 0)
#define XA_RENDERER_FLAG_POSTINIT_DONE  (1 << 1)
#define XA_RENDERER_FLAG_IDLE           (1 << 2)
#define XA_RENDERER_FLAG_RUNNING        (1 << 3)
#define XA_RENDERER_FLAG_PAUSED         (1 << 4)
/*******************************************************************************
 * global variables
 ******************************************************************************/
#ifndef RTK_HW
XosTimer grend_timer;
const char grenderer_out_file[] = "renderer_out.pcm";
#else
/*******************************************************************************
 * RTK_StreamIO_Init
 *
 * Outbount fifo registers are configured here.
 ******************************************************************************/
void RTK_StreamIO_Init()
{
	
    int ret =0;
    int ret1 =0;
    /*Bypassing the cashe for the FIFO registers*/
    xthal_set_region_attribute((void *)0x18000000, 65536, XCHAL_CA_BYPASS, 0);	
    ret = xthal_set_region_attribute((void *)0x18010000, 65536, XCHAL_CA_BYPASS, 0);	
	// reset HW_STRM_1_L_OUT
	*((volatile unsigned int*) (0x1801F094 + 0x0) ) = 0x3;
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x28)) = 0x1;
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x28)) = 0x0;
	ret1 = *((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x28));
	memset(ghw_outBuffer0, 0x0, HW_OUT_BUF_SIZE);
	ghw_Loutbuffer_WP = (unsigned int)ghw_outBuffer0;

	//HW_STRM_1_L_OUT prog---------------------------------------------------

	//HW_STRM_1_L_OUT start_addr=ghw_outBuffer0
	*((volatile unsigned int*) HW_STRM_1_L_OUT)=(unsigned int)ghw_outBuffer0;

	//HW_STRM_1_L_OUT end_addr=(ghw_outBuffer0 + HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE)
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x4) )=(unsigned int)(ghw_outBuffer0 + HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE);

	//HW_STRM_1_L_OUT enable, direct mode, tag=5, mono_enable 
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x10) ) = /*0x00050A01*/0x00050803/*0x00050001*/;

	//HW_STRM_1_L_OUT threshold 
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x14) ) = (HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE) >> 1;
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x24) ) = 0x1;
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x1c) ) = 0x1;

	// reset HW_STRM_1_R_OUT
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x28)) = 0x1;
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x28)) = 0x0;
	memset(ghw_outBuffer1, 0x0, HW_OUT_BUF_SIZE);
	ghw_Routbuffer_WP = (unsigned int)ghw_outBuffer1;

	//HW_STRM_1_R_OUT prog---------------------------------------------------

	//HW_STRM_1_R_OUT start_addr=ghw_outBuffer1
	*((volatile unsigned int*) HW_STRM_1_R_OUT)=(unsigned int)ghw_outBuffer1;

	//HW_STRM_1_R_OUT end_addr=(ghw_outBuffer1 + HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE)
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x4) )=(unsigned int)(ghw_outBuffer1 + HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE);

	//HW_STRM_1_R_OUT enable, direct mode, tag=7, mono_enable 
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x10) ) = 0x00070803/*0x00070A01*/;

	//HW_STRM_1_R_OUT threshold 
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x14) ) = (HW_OUT_BUF_SIZE - HW_BYTES_PER_SAMPLE) >> 1;

	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x24) ) = 0x1;
	*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0x1c) ) = 0x1;
    
} 

#endif
/*******************************************************************************
 * xa_fw_fifo_handler
 *
 * Process FIFO exchange / underrun interrupt. Do a copy from ring buffer into
 * I2S high-buffer (normal operation). In case of low-buffer emptiness
 * (underrun) reset the FIFO and perform a recovery
 ******************************************************************************/

#ifndef RTK_HW
static void xa_fw_handler(void *arg)
{
    XARenderer *d = arg;

    d->consumed = d->submited * d->sample_size;
    d->fifo_avail = d->fifo_avail + (HW_FIFO_LENGTH_SAMPLES>>1);
    if((d->fifo_avail)>HW_FIFO_LENGTH_SAMPLES)
       {/*under run case*/
           d->state ^= XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_IDLE;
           d->fifo_avail = HW_FIFO_LENGTH_SAMPLES;
           xos_timer_stop(&grend_timer);
           d->cdata->cb(d->cdata, 0);
       }
      else if(((int)d-> fifo_avail) <= 0)
       {/* over run */
           d->state ^= XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_IDLE;
           d->fifo_avail=HW_FIFO_LENGTH_SAMPLES;
           xos_timer_stop(&grend_timer);
           d->cdata->cb(d->cdata, 0);
	   }
      else
      {
           d->cdata->cb(d->cdata, 0);
      }
}
#else
void xa_hw_handler(void* ptr)
{
	int i;
	int j = 0;
	XARenderer *d = ptr;
	
	d->consumed = d->submited * d->sample_size;
	if( d->submit_flag == 0 )
	{
		/*underrun case */
		/*move the renderer to idle and reset the fifo*/
		
#ifdef __TOOLS_RF2__
		xos_disable_ints(1<<6);
#else
		xos_interrupt_disable(XCHAL_EXTINT6_NUM);
#endif
		d->cdata->cb(d->cdata, 0);
		RTK_StreamIO_Init();
		d->state ^= XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_IDLE;
	}
    else
	{
        /*delay added for clearing the threshold interrupt status bit*/
	    for(i=0;i<512;i++)
	    {
		   j++;
	    }
	    i = (int)(ghw_outBuffer0 + HW_OUT_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
	    if(ghw_Loutbuffer_WP == i)
		{
		    *((volatile unsigned int*) (HW_STRM_1_L_OUT + 0xc) ) =  (unsigned int)ghw_outBuffer0+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE*2)-HW_BYTES_PER_SAMPLE;
		}
		else
		{
			*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0xc) ) = (unsigned int)(ghw_outBuffer0 + HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
		}
		i = (int)(ghw_outBuffer1 + HW_OUT_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
		if(ghw_Routbuffer_WP == i)
		{
        
    		*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0xc) ) = (unsigned int)ghw_outBuffer1+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE*2)-HW_BYTES_PER_SAMPLE;
		}
		else
		{
			*((volatile unsigned int*) (HW_STRM_1_R_OUT + 0xc) ) =  (unsigned int)ghw_outBuffer1 + HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE;
		}
		(*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x34))) = 0;
		d->cdata->cb(d->cdata, 0);
	}
    /*submit flag is cleared; it it remains cleared in the next interrupt handler execution it is underrun*/
	d->submit_flag = 0;
}
#endif

/*******************************************************************************
 * Codec access functions
 ******************************************************************************/
#ifndef RTK_HW
static inline void xa_fw_renderer_close(XARenderer *d)
{
    fclose(d->fw);
    xos_timer_stop(&grend_timer);
}
#else
static inline void xa_hw_renderer_close(XARenderer *d)
{
	*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x10) ) = 0;
}
#endif
/* ...submit data (in samples) into internal renderer ring-buffer */
#ifndef RTK_HW
static inline UWORD32 xa_fw_renderer_submit(XARenderer *d, void *b, UWORD32 n)
{
   FILE *fp = NULL;
   UWORD32 samples_write = 0;
   UWORD32 avail;
   UWORD32 k,bytes_write;

   fp = d ->fw;
   if((int)(avail = d-> fifo_avail)>=0)
   {
        k = (avail > n ? n : avail);
        d->fifo_avail = (avail -= k);
        /* ...process buffer start-up */
        if (d->state & XA_RENDERER_FLAG_IDLE)
        {
            /* ...start-up transmission if FIFO gets full */
            if (avail == 0)
            {
            /*FIFO is full starting the timer*/
                xos_timer_start(&grend_timer,((long long)(HW_FIFO_LENGTH_SAMPLES>>1)*xos_get_clock_freq()/HW_I2S_SF)/* 2133334*/,1,xa_fw_handler,d);
                d->state ^= XA_RENDERER_FLAG_IDLE | XA_RENDERER_FLAG_RUNNING;
            }
         }
     }
   bytes_write = n * d->sample_size;
   samples_write = fwrite(b,1,bytes_write,fp);
   return(samples_write/d->sample_size);
}
#else
static inline UWORD32 xa_hw_renderer_submit(XARenderer *d, void *b, UWORD32 n)
{
	WORD16* in_buffer_ptr = NULL;
	UWORD32 i;
	int test_ch_idx,test_idx;

	d->submit_flag = 1;
	in_buffer_ptr = (short*) b;
	
    /*deinterleaving data from the input buffer*/
	for( i=0 ; i<n ; i++ )
	{
		 *((UWORD16*) ghw_Loutbuffer_WP+i)= *(in_buffer_ptr);
		 in_buffer_ptr++;
		 *((UWORD16*) ghw_Routbuffer_WP+i) = *(in_buffer_ptr);
		 in_buffer_ptr++;
	}
	i = (UWORD32)(ghw_outBuffer0 + HW_OUT_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
	if(ghw_Loutbuffer_WP == i)
	{
	    ghw_Loutbuffer_WP = (unsigned int)ghw_outBuffer0;  //buffer ring to start addr
	}
	else
	{
		ghw_Loutbuffer_WP=ghw_Loutbuffer_WP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
	}
    
	i = (UWORD32)(ghw_outBuffer1 + HW_OUT_BUF_SIZE) - (HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
	if(ghw_Routbuffer_WP == i)
	{
		ghw_Routbuffer_WP = (unsigned int)ghw_outBuffer1;  //buffer ring to start addr
  	}
	else
	{
		ghw_Routbuffer_WP=ghw_Routbuffer_WP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE);
    }
	if (d->state & XA_RENDERER_FLAG_IDLE)
    {	
		if( ghw_Loutbuffer_WP == (unsigned int)ghw_outBuffer0)
		{		
		   *((volatile unsigned int*) (HW_STRM_1_L_OUT + 0xc) ) = ghw_Loutbuffer_WP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE*2)-HW_BYTES_PER_SAMPLE;
	       *((volatile unsigned int*) (HW_STRM_1_R_OUT + 0xc) ) = ghw_Routbuffer_WP+(HW_SAMPLE_NUM*HW_BYTES_PER_SAMPLE*2)-HW_BYTES_PER_SAMPLE;
	       (*((volatile unsigned int*) (HW_STRM_1_L_OUT + 0x30))) = HW_SAMPLE_NUM;
		   d->state ^= XA_RENDERER_FLAG_IDLE | XA_RENDERER_FLAG_RUNNING;
#ifdef __TOOLS_RF2__
            xos_enable_ints(1<<6);
#else
			xos_interrupt_enable(XCHAL_EXTINT6_NUM);
#endif
		}
	
     }
	return n;
}

#endif
/*******************************************************************************
 * API command hooks
 ******************************************************************************/

/* ...standard codec initialization routine */
static XA_ERRORCODE xa_renderer_get_api_size(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...check parameters are sane */
    XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);
    /* ...retrieve API structure size */
    *(WORD32 *)pv_value = sizeof(*d);
    return XA_NO_ERROR;
}
#ifndef RTK_HW
static XA_ERRORCODE xa_fw_renderer_init (XARenderer *d)
{
   d->consumed = 0;
   d->fw = NULL;

   /*opening the output file*/
   d->fw = fopen(grenderer_out_file,"wb");
   if ( d->fw == NULL )
   {
     /*file open failed*/
     return XA_FATAL_ERROR;
   }
   /*initialy FIFO will be empty so fifo_avail = HW_FIFO_LENGTH_SAMPLES*/
   d->fifo_avail = HW_FIFO_LENGTH_SAMPLES;
   
   /*initialises the timer ;timer0 is used as system timer*/
   xos_timer_init(&grend_timer);
   return XA_NO_ERROR;
}
#else
static XA_ERRORCODE xa_hw_renderer_init (XARenderer *d)
{
	int status;
	d->consumed = 0;
	RTK_StreamIO_Init();
	status = xos_register_interrupt_handler(XCHAL_EXTINT6_NUM,xa_hw_handler,d);
	return 0;
}
#endif

/* ...standard codec initialization routine */
static XA_ERRORCODE xa_renderer_init(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - pointer must be valid */
    XF_CHK_ERR(d, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...process particular initialization type */
    switch (i_idx)
    {
    case XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS:
    {
        /* ...pre-configuration initialization; reset internal data */
        memset(d, 0, sizeof(*d));
        /* ...set default renderer parameters - 16-bit little-endian stereo @ 48KHz */
        d->sample_size = 4;
        d->channels = 2;
        d->pcm_width = 16;
        d->rate = 48000;
        /* ...and mark renderer has been created */
        d->state = XA_RENDERER_FLAG_PREINIT_DONE;
        return XA_NO_ERROR;
    }
    case XA_CMD_TYPE_INIT_API_POST_CONFIG_PARAMS:
    {
        /* ...post-configuration initialization (all parameters are set) */
        XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PREINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);
#ifndef RTK_HW

        XF_CHK_ERR(xa_fw_renderer_init(d) == 0, XA_RENDERER_CONFIG_FATAL_HW);
#else
        XF_CHK_ERR(xa_hw_renderer_init(d) == 0, XA_RENDERER_CONFIG_FATAL_HW);
#endif
        /* ...mark post-initialization is complete */
        d->state |= XA_RENDERER_FLAG_POSTINIT_DONE;
        return XA_NO_ERROR;
    }

    case XA_CMD_TYPE_INIT_PROCESS:
    {
        /* ...kick run-time initialization process; make sure setup is complete */
        XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);
        /* ...mark renderer is in idle state */
        d->state |= XA_RENDERER_FLAG_IDLE;
        return XA_NO_ERROR;
    }

    case XA_CMD_TYPE_INIT_DONE_QUERY:
    {
        /* ...check if initialization is done; make sure pointer is sane */
        XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);
        /* ...put current status */
        *(WORD32 *)pv_value = (d->state & XA_RENDERER_FLAG_IDLE ? 1 : 0);
        return XA_NO_ERROR;
    }

    default:
        /* ...unrecognized command type */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...HW-renderer control function */
static inline XA_ERRORCODE xa_hw_renderer_control(XARenderer *d, UWORD32 state)
{
    switch (state)
    {
    case XA_RENDERER_STATE_RUN:
        /* ...renderer must be in paused state */
        XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PAUSED, XA_RENDERER_EXEC_NONFATAL_STATE);
        /* ...mark renderer is running */
        d->state ^= XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_PAUSED;
        return XA_NO_ERROR;

    case XA_RENDERER_STATE_PAUSE:
        /* ...renderer must be in running state */
        XF_CHK_ERR(d->state & XA_RENDERER_FLAG_RUNNING, XA_RENDERER_EXEC_NONFATAL_STATE);
        /* ...pause renderer operation */
        //xa_hw_renderer_pause(d);
        /* ...mark renderer is paused */
        d->state ^= XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_PAUSED;
        return XA_NO_ERROR;

    case XA_RENDERER_STATE_IDLE:
        /* ...command is valid in any active state; stop renderer operation */
#ifndef RTK_HW
        xa_fw_renderer_close(d);
#else
        xa_hw_renderer_close(d);
#endif
               /* ...reset renderer flags */
        d->state &= ~(XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_PAUSED);
        return XA_NO_ERROR;

    default:
        /* ...unrecognized command */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...set renderer configuration parameter */
static XA_ERRORCODE xa_renderer_set_config_param(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    UWORD32     i_value;

    /* ...sanity check - pointers must be sane */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);
    /* ...pre-initialization must be completed */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PREINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);
    /* ...process individual configuration parameter */
    switch (i_idx)
    {
    case XA_RENDERER_CONFIG_PARAM_PCM_WIDTH:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_RENDERER_FLAG_POSTINIT_DONE) == 0, XA_RENDERER_CONFIG_FATAL_STATE);
        /* ...get requested PCM width */
        i_value = (UWORD32) *(WORD32 *)pv_value;
        /* ...check value is permitted (16 bits only) */
        XF_CHK_ERR(i_value == 16, XA_RENDERER_CONFIG_NONFATAL_RANGE);
        /* ...apply setting */
        d->pcm_width = i_value;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_CHANNELS:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_RENDERER_FLAG_POSTINIT_DONE) == 0, XA_RENDERER_CONFIG_FATAL_STATE);
        /* ...get requested channel number */
        i_value = (UWORD32) *(WORD32 *)pv_value;
        /* ...allow stereo only */
        XF_CHK_ERR(i_value == 2, XA_RENDERER_CONFIG_NONFATAL_RANGE);
        /* ...apply setting */
        d->channels = (UWORD32)i_value;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_SAMPLE_RATE:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_RENDERER_FLAG_POSTINIT_DONE) == 0, XA_RENDERER_CONFIG_FATAL_STATE);
        /* ...get requested sampling rate */
        i_value = (UWORD32) *(WORD32 *)pv_value;
        /* ...allow 44.1 and 48KHz only - tbd */
        XF_CHK_ERR(i_value == 44100 || i_value == 48000, XA_RENDERER_CONFIG_NONFATAL_RANGE);
        /* ...apply setting */
        d->rate = (UWORD32)i_value;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_FRAME_SIZE:
        /* ...command is valid only in configuration state */
        XF_CHK_ERR((d->state & XA_RENDERER_FLAG_POSTINIT_DONE) == 0, XA_RENDERER_CONFIG_FATAL_STATE);

        /* ...get requested frame size */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...check it is equal to the only frame size we support */
        XF_CHK_ERR(i_value == HW_FIFO_LENGTH_SAMPLES / 2, XA_RENDERER_CONFIG_NONFATAL_RANGE);

        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_CB:
        /* ...set opaque callback data function */
        d->cdata = (xa_renderer_cb_t *)pv_value;

        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_STATE:
        /* ...runtime state control parameter valid only in execution state */
        XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

        /* ...get requested state */
        i_value = (UWORD32) *(WORD32 *)pv_value;

        /* ...pass to state control hook */
        return xa_hw_renderer_control(d, i_value);

    default:
        /* ...unrecognized parameter */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...state retrieval function */
static inline UWORD32 xa_hw_renderer_get_state(XARenderer *d)
{
    if (d->state & XA_RENDERER_FLAG_RUNNING)
        return XA_RENDERER_STATE_RUN;
    else if (d->state & XA_RENDERER_FLAG_PAUSED)
        return XA_RENDERER_STATE_PAUSE;
    else
        return XA_RENDERER_STATE_IDLE;
}

/* ...retrieve configuration parameter */
static XA_ERRORCODE xa_renderer_get_config_param(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - renderer must be initialized */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...make sure pre-initialization is completed */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PREINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

    /* ...process individual configuration parameter */
    switch (i_idx)
    {
    case XA_RENDERER_CONFIG_PARAM_PCM_WIDTH:
        /* ...return current PCM width */
        *(WORD32 *)pv_value = d->pcm_width;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_CHANNELS:
        /* ...return current channel number */
        *(WORD32 *)pv_value = d->channels;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_SAMPLE_RATE:
        /* ...return current sampling rate */
        *(WORD32 *)pv_value = d->rate;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_FRAME_SIZE:
        /* ...return current audio frame length (in samples) */
        *(WORD32 *)pv_value = HW_FIFO_LENGTH_SAMPLES / 2;
        return XA_NO_ERROR;

    case XA_RENDERER_CONFIG_PARAM_STATE:
        /* ...return current execution state */
        *(WORD32 *)pv_value = xa_hw_renderer_get_state(d);
        return XA_NO_ERROR;
	case XA_RENDERER_CONFIG_PARAM_BYTES_PRODUCED:
        /* ...return current execution state */
        *(UWORD64 *)pv_value = d->bytes_produced;
        return XA_NO_ERROR;

    default:
        /* ...unrecognized parameter */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...execution command */
static XA_ERRORCODE xa_renderer_execute(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - pointer must be valid */
    XF_CHK_ERR(d, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...renderer must be in running state */
    XF_CHK_ERR(d->state & (XA_RENDERER_FLAG_RUNNING | XA_RENDERER_FLAG_IDLE), XA_RENDERER_EXEC_FATAL_STATE);

    /* ...process individual command type */
    switch (i_idx)
    {
    case XA_CMD_TYPE_DO_EXECUTE:
        /* ...silently ignore; everything is done in "set_input" */
        return XA_NO_ERROR;

    case XA_CMD_TYPE_DONE_QUERY:
        /* ...always report "no" - tbd - is that needed at all? */
        XF_CHK_ERR(pv_value, XA_API_FATAL_INVALID_CMD_TYPE);
        *(WORD32 *)pv_value = 0;
        return XA_NO_ERROR;

    case XA_CMD_TYPE_DO_RUNTIME_INIT:
        /* ...silently ignore */
        return XA_NO_ERROR;

    default:
        /* ...unrecognized command */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...set number of input bytes */
static XA_ERRORCODE xa_renderer_set_input_bytes(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    UWORD32     size=0;
   
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...make sure it is an input port  */
    XF_CHK_ERR(i_idx == 0, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...renderer must be initialized */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_EXEC_FATAL_STATE);

    /* ...input buffer pointer must be valid */
    XF_CHK_ERR(d->input, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...check buffer size is sane */
    XF_CHK_ERR((size = *(UWORD32 *)pv_value / d->sample_size) >= 0, XA_RENDERER_EXEC_FATAL_INPUT);

    /* ...make sure we have integral amount of samples */
    XF_CHK_ERR(size * d->sample_size == *(UWORD32 *)pv_value, XA_RENDERER_EXEC_FATAL_INPUT);

	d->bytes_produced += (*(UWORD32 *)pv_value);
    d->submited = size;

#ifndef RTK_HW

    xa_fw_renderer_submit(d, d->input, size);
#else
    xa_hw_renderer_submit(d, d->input, size);
#endif
	/*update the consumed bytes soon untill the i2s is started*/
    if (d->state & XA_RENDERER_FLAG_IDLE)
         d->consumed = HW_FIFO_LENGTH_SAMPLES << 1;
    /* ...all is correct */
    return XA_NO_ERROR;
}

/* ...get number of consumed bytes */
static XA_ERRORCODE xa_renderer_get_curidx_input_buf(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...sanity check - check parameters */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...input buffer index must be valid */
    XF_CHK_ERR(i_idx == 0, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...renderer must be in post-init state */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_EXEC_FATAL_STATE);

    /* ...input buffer must exist */
    XF_CHK_ERR(d->input, XA_RENDERER_EXEC_FATAL_INPUT);

    /* ...return number of bytes consumed */
    *(WORD32 *)pv_value = d->consumed;
    d->consumed = 0;
    return XA_NO_ERROR;
}

/*******************************************************************************
 * Memory information API
 ******************************************************************************/

/* ..get total amount of data for memory tables */
static XA_ERRORCODE xa_renderer_get_memtabs_size(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...check renderer is pre-initialized */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PREINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

    /* ...we have all our tables inside API structure */
    *(WORD32 *)pv_value = 0;

    return XA_NO_ERROR;
}

/* ..set memory tables pointer */
static XA_ERRORCODE xa_renderer_set_memtabs_ptr(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...check renderer is pre-initialized */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_PREINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

    /* ...do not do anything; just return success - tbd */
    return XA_NO_ERROR;
}

/* ...return total amount of memory buffers */
static XA_ERRORCODE xa_renderer_get_n_memtabs(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity checks */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...we have 1 input buffer only */
    *(WORD32 *)pv_value = 1;

    return XA_NO_ERROR;
}

/* ...return memory buffer data */
static XA_ERRORCODE xa_renderer_get_mem_info_size(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    UWORD32     i_value;

    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

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
static XA_ERRORCODE xa_renderer_get_mem_info_alignment(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

    /* ...all buffers are at least 4-bytes aligned */
    *(WORD32 *)pv_value = 4;

    return XA_NO_ERROR;
}

/* ...return memory type data */
static XA_ERRORCODE xa_renderer_get_mem_info_type(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...command valid only after post-initialization step */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_RENDERER_CONFIG_FATAL_STATE);

    switch (i_idx)
    {
    case 0:
        /* ...input buffers */
        *(WORD32 *)pv_value = XA_MEMTYPE_INPUT;
        return XA_NO_ERROR;

    default:
        /* ...invalid index */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...set memory pointer */
static XA_ERRORCODE xa_renderer_set_mem_ptr(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    /* ...basic sanity check */
    XF_CHK_ERR(d && pv_value, XA_API_FATAL_INVALID_CMD_TYPE);

    /* ...codec must be initialized */
    XF_CHK_ERR(d->state & XA_RENDERER_FLAG_POSTINIT_DONE, XA_API_FATAL_INVALID_CMD_TYPE);

    TRACE(INIT, _b("xa_renderer_set_mem_ptr[%u]: %p"), i_idx, pv_value);

    /* ...select memory buffer */
    switch (i_idx)
    {
    case 0:
        /* ...input buffer */
        d->input = pv_value;
        return XA_NO_ERROR;

    default:
        /* ...invalid index */
        return XF_CHK_ERR(0, XA_API_FATAL_INVALID_CMD_TYPE);
    }
}

/* ...set input over */
static XA_ERRORCODE xa_renderer_input_over(XARenderer *d, WORD32 i_idx, pVOID pv_value)
{
    return XA_NO_ERROR;
}

/*******************************************************************************
 * API command hooks
 ******************************************************************************/

static XA_ERRORCODE (* const xa_renderer_api[])(XARenderer *, WORD32, pVOID) =
{
    [XA_API_CMD_GET_API_SIZE]           = xa_renderer_get_api_size,
    [XA_API_CMD_INIT]                   = xa_renderer_init,
    [XA_API_CMD_SET_CONFIG_PARAM]       = xa_renderer_set_config_param,
    [XA_API_CMD_GET_CONFIG_PARAM]       = xa_renderer_get_config_param,
    [XA_API_CMD_EXECUTE]                = xa_renderer_execute,
    [XA_API_CMD_SET_INPUT_BYTES]        = xa_renderer_set_input_bytes,
    [XA_API_CMD_GET_CURIDX_INPUT_BUF]   = xa_renderer_get_curidx_input_buf,
    [XA_API_CMD_GET_MEMTABS_SIZE]       = xa_renderer_get_memtabs_size,
    [XA_API_CMD_SET_MEMTABS_PTR]        = xa_renderer_set_memtabs_ptr,
    [XA_API_CMD_GET_N_MEMTABS]          = xa_renderer_get_n_memtabs,
    [XA_API_CMD_GET_MEM_INFO_SIZE]      = xa_renderer_get_mem_info_size,
    [XA_API_CMD_GET_MEM_INFO_ALIGNMENT] = xa_renderer_get_mem_info_alignment,
    [XA_API_CMD_GET_MEM_INFO_TYPE]      = xa_renderer_get_mem_info_type,
    [XA_API_CMD_SET_MEM_PTR]            = xa_renderer_set_mem_ptr,
    [XA_API_CMD_INPUT_OVER]             = xa_renderer_input_over,
};

/* ...total numer of commands supported */
#define XA_RENDERER_API_COMMANDS_NUM   (sizeof(xa_renderer_api) / sizeof(xa_renderer_api[0]))

/*******************************************************************************
 * API entry point
 ******************************************************************************/

XA_ERRORCODE xa_renderer(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value)
{
    XA_ERRORCODE Rend_ret = 0;
    XARenderer *renderer = (XARenderer *) p_xa_module_obj;
#ifdef XAF_PROFILE
    clk_t comp_start, comp_stop;
#endif
    /* ...check if command index is sane */
    XF_CHK_ERR(i_cmd < XA_RENDERER_API_COMMANDS_NUM, XA_API_FATAL_INVALID_CMD);

    /* ...see if command is defined */
    XF_CHK_ERR(xa_renderer_api[i_cmd], XA_API_FATAL_INVALID_CMD);

    /* ...execute requested command */
#ifdef XAF_PROFILE
     if(XA_API_CMD_INIT != i_cmd)
	 {
         comp_start = clk_read_start(CLK_SELN_THREAD);
	 }
#endif
  
    Rend_ret = xa_renderer_api[i_cmd](renderer, i_idx, pv_value);
#ifdef XAF_PROFILE
    if(XA_API_CMD_INIT != i_cmd)
	 {
        comp_stop = clk_read_stop(CLK_SELN_THREAD);
        renderer_cycles += clk_diff(comp_stop, comp_start);
	 }
#endif
    return Rend_ret;
}
