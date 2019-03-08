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

#ifndef __XA_AMR_WB_CODEC_API_H__
#define __XA_AMR_WB_CODEC_API_H__

#include "xa_type_def.h"
#include "xa_error_standards.h"


#define XA_AMR_WB_MAX_SAMPLES_PER_FRAME 320
#define XA_AMR_WB_MAX_SPEECH_BITS_PER_FRAME 480
#define XA_AMR_WB_MAX_SPEECH_WORD8_PER_FRAME ((XA_AMR_WB_MAX_SPEECH_BITS_PER_FRAME + 7) / 8)

#define XA_AMR_WB_FRAME_TYPE_TX 0x6b21
#define XA_AMR_WB_FRAME_TYPE_RX 0x6b20

#define XA_AMR_WB_CONFORMANCE_BIT_0 (-127)
#define XA_AMR_WB_CONFORMANCE_BIT_1 (+127)
#define XA_AMR_WB_SCRATCH_FILL 1


typedef enum
  {
    XA_AMR_WB_DTX_OFF = 0,
    XA_AMR_WB_DTX_VAD
  } xa_amr_wb_dtx_t;


typedef enum
  {
    XA_AMR_WB_MR660 = 0,
    XA_AMR_WB_MR885,
    XA_AMR_WB_MR1265,
    XA_AMR_WB_MR1425,
    XA_AMR_WB_MR1585,
    XA_AMR_WB_MR1825,
    XA_AMR_WB_MR1985,
    XA_AMR_WB_MR2305,
    XA_AMR_WB_MR2385,
    XA_AMR_WB_MRDTX
  } xa_amr_wb_mode_t;


typedef enum
  {
    XA_AMR_WB_TX_SPEECH = 0,
    XA_AMR_WB_TX_SID_FIRST,
    XA_AMR_WB_TX_SID_UPDATE,
    XA_AMR_WB_TX_NO_DATA,
  } xa_amr_wb_tx_frame_type_t;


typedef enum
  {
    XA_AMR_WB_RX_SPEECH_GOOD = 0,
    XA_AMR_WB_RX_SPEECH_PROBABLY_DEGRADED,
    XA_AMR_WB_RX_SPEECH_LOST,
    XA_AMR_WB_RX_SPEECH_BAD,
    XA_AMR_WB_RX_SID_FIRST,
    XA_AMR_WB_RX_SID_UPDATE,
    XA_AMR_WB_RX_SID_BAD,
    XA_AMR_WB_RX_NO_DATA,
  } xa_amr_wb_rx_frame_type_t;


typedef enum
  {
    XA_AMR_WB_FMT_CONFORMANCE,
    XA_AMR_WB_FMT_CORE_FRAME
  } xa_amr_wb_format_t;


#if defined(__cplusplus)
extern "C" {

#endif	/* __cplusplus */


WORD32
xa_amr_wb_enc_get_handle_byte_size ();


WORD32
xa_amr_wb_enc_get_scratch_byte_size ();


WORD32
xa_amr_wb_dec_get_handle_byte_size ();


WORD32
xa_amr_wb_dec_get_scratch_byte_size ();


const char *
xa_amr_wb_get_lib_name_string ();


const char *
xa_amr_wb_get_lib_version_string ();


const char *
xa_amr_wb_get_lib_api_version_string ();


XA_ERRORCODE
xa_amr_wb_enc_init (xa_codec_handle_t handle,
		    pWORD32 scratch,
		    xa_amr_wb_format_t format);


XA_ERRORCODE
xa_amr_wb_enc (xa_codec_handle_t handle,
	       pWORD16 inp_speech,
	       pUWORD8 enc_speech /* output */,
	       xa_amr_wb_dtx_t dtx,
	       xa_amr_wb_mode_t *enc_mode /* input/output */,
	       xa_amr_wb_tx_frame_type_t *frame_type /* output */,
	       pWORD32 num_bits /* output */);


XA_ERRORCODE
xa_amr_wb_dec_init (xa_codec_handle_t handle,
		    pWORD32 scratch,
		    xa_amr_wb_format_t format);


XA_ERRORCODE
xa_amr_wb_dec (xa_codec_handle_t handle,
	       xa_amr_wb_mode_t mode,
	       xa_amr_wb_rx_frame_type_t frame_type,
	       pUWORD8 enc_speech,
	       pWORD16 synth_speech /* output */);


#if defined(__cplusplus)
}
#endif	/* __cplusplus */


#endif /* __XA_AMR_WB_CODEC_API_H__ */
