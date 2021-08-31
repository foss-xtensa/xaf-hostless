/*
* Copyright (c) 2015-2021 Cadence Design Systems Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
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
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "audio/xa-opus-decoder-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#ifdef __XTENSA_EB__
#define _SYSTEM_IS_BIG_ENDIAN
#endif

#define XA_EXT_CFG_ID_OFFSET       0
#define XA_EXT_CFG_BUF_PTR_OFFSET  1
// #define ENABLE_RAW_OPUS_SET_CONFIG

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:../test/test_inp/opus51_trim.webm.ogg -outfile:opus51_trim_out.pcm\nNOTE: To decode RAW OPUS input define ENABLE_RAW_OPUS_SET_CONFIG flag and re-compile.\nUsage for RAW input: %s -infile:../test/test_inp/testvector01.bit -outfile:testvector01_out.pcm\n\n", argv[0], argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 9)
#define AUDIO_COMP_BUF_SIZE    (1024 << 10)

#define NUM_COMP_IN_GRAPH       1

#define MAX_EXEC_COUNT           1

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, dec_cycles;
    extern double dsp_mcps;
#endif

XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec22(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec23(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_split(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mimo_mix(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_wwd(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_opus_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_wwd_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_fe(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_person_detect_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_keyword_detection_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}


//component parameters
#define OPUS_DEC_PCM_WIDTH       16
#define OPUS_DEC_SAMPLE_RATE     48000
#define OPUS_DEC_CHANNELS        6

#ifndef ENABLE_RAW_OPUS_SET_CONFIG
static int opus_dec_setup(void *p_decoder)
{
    int param[20];
    int ret;

    param[0] = XA_OPUS_DEC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_DEC_PCM_WIDTH;

    param[2] = XA_OPUS_DEC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_DEC_SAMPLE_RATE;

    param[4] = XA_OPUS_DEC_CONFIG_PARAM_CHANNELS;
    param[5] = OPUS_DEC_CHANNELS;

    param[6] = XA_OPUS_DEC_CONFIG_PARAM_GAIN;
    param[7] = 0;

    param[8] = XA_OPUS_DEC_CONFIG_PARAM_SILK_INBANDFEC_ENABLE;
    param[9] = 0;

    param[10] = XA_OPUS_DEC_CONFIG_PARAM_NUM_STREAMS;
    param[11] = 4;

    param[12] = XA_OPUS_DEC_CONFIG_PARAM_NUM_COUPLED_STREAMS;
    param[13] = 2;

    param[14] = XA_OPUS_DEC_CONFIG_PARAM_CHAN_MAPPING;
    param[15] = 1;

    param[16] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_TYPE;
    param[17] = 1;

    ret = xaf_comp_set_config(p_decoder, 9, &param[0]);
    if(ret != 0)
        return ret;
    {
#define OPUS_DEC_NUM_SET_PARAMS_EXT	1
        int param_ext[OPUS_DEC_NUM_SET_PARAMS_EXT * 2];
        xaf_ext_buffer_t ext_buf[OPUS_DEC_NUM_SET_PARAMS_EXT];
        memset(ext_buf, 0, sizeof(xaf_ext_buffer_t) * OPUS_DEC_NUM_SET_PARAMS_EXT);

        WORD8 stream_map[XA_OPUS_MAX_NUM_CHANNELS]= {0,4,1,2,3,5,0,0};

        ext_buf[0].max_data_size = sizeof(stream_map);
        ext_buf[0].valid_data_size = sizeof(stream_map);
        ext_buf[0].ext_config_flags |= XAF_EXT_PARAM_SET_FLAG(0);
        //ext_buf[0].ext_config_flags &= XAF_EXT_PARAM_CLEAR_FLAG(0);
        ext_buf[0].data = (UWORD8 *) stream_map;

        param_ext[0*2+XA_EXT_CFG_ID_OFFSET] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_MAP;
        param_ext[0*2+XA_EXT_CFG_BUF_PTR_OFFSET] = (int) &ext_buf[0];

        ret = xaf_comp_set_config_ext(p_decoder, OPUS_DEC_NUM_SET_PARAMS_EXT, param_ext);
    }
    return ret;
}
#else
static int opus_dec_setup(void *p_decoder)
{
    int param[20];

    param[0] = XA_OPUS_DEC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_DEC_PCM_WIDTH;

    param[2] = XA_OPUS_DEC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_DEC_SAMPLE_RATE;

    param[4] = XA_OPUS_DEC_CONFIG_PARAM_CHANNELS;
    param[5] = 2;

    param[6] = XA_OPUS_DEC_CONFIG_PARAM_GAIN;
    param[7] = 0;

    param[8] = XA_OPUS_DEC_CONFIG_PARAM_SILK_INBANDFEC_ENABLE;
    param[9] = 0;

    param[10] = XA_OPUS_DEC_CONFIG_PARAM_NUM_STREAMS;
    param[11] = 1;

    param[12] = XA_OPUS_DEC_CONFIG_PARAM_NUM_COUPLED_STREAMS;
    param[13] = 1;

    param[14] = XA_OPUS_DEC_CONFIG_PARAM_CHAN_MAPPING;
    param[15] = 0;

    param[16] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_TYPE;
    param[17] = 0;

    return(xaf_comp_set_config(p_decoder, 9, &param[0]));
}
#endif

static int get_opus_dec_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;

    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");

    param[0] = XA_OPUS_DEC_CONFIG_PARAM_PCM_WIDTH;
    param[2] = XA_OPUS_DEC_CONFIG_PARAM_SAMPLE_RATE;
    param[4] = XA_OPUS_DEC_CONFIG_PARAM_CHANNELS;

    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->pcm_width = param[1];
    comp_format->sample_rate = param[3];
    comp_format->channels = param[5];

    printf("pcm_width: %d\n", comp_format->pcm_width);
    printf("sample_rate: %d\n", comp_format->sample_rate);
    printf("channels: %d\n", comp_format->channels);

    {
#define OPUS_DEC_NUM_GET_PARAMS_EXT	1
        int param_ext[OPUS_DEC_NUM_GET_PARAMS_EXT * 2];

        xaf_ext_buffer_t ext_buf[OPUS_DEC_NUM_GET_PARAMS_EXT];
        memset(ext_buf, 0, sizeof(xaf_ext_buffer_t) * OPUS_DEC_NUM_GET_PARAMS_EXT);

        WORD8 stream_map[XA_OPUS_MAX_NUM_CHANNELS]= {0,0,0,0,0,0,0,0};
        
        ext_buf[0].max_data_size = sizeof(stream_map);
        ext_buf[0].valid_data_size = sizeof(stream_map);
        //ext_buf[0].ext_config_flags |= XAF_EXT_PARAM_SET_FLAG(0);
        ext_buf[0].ext_config_flags &= XAF_EXT_PARAM_CLEAR_FLAG(0);
        ext_buf[0].data = (UWORD8 *) stream_map;

        param_ext[0*2+XA_EXT_CFG_ID_OFFSET] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_MAP;
        param_ext[0*2+XA_EXT_CFG_BUF_PTR_OFFSET] = (int) &ext_buf[0];

        ret = xaf_comp_get_config_ext(p_comp, OPUS_DEC_NUM_GET_PARAMS_EXT, param_ext);
        if(ret < 0)
            return ret;

        printf("stream map: ");
        int i;
        for(i = 0; i < XA_OPUS_MAX_NUM_CHANNELS; i++)
        {
            printf("%d ", stream_map[i]);
        }
        printf("\n");
    }

    return 0;
}


void fio_quit()
{
    return;
}

int main_task(int argc, char **argv)
{
	void *p_adev = NULL;
    void *p_decoder = NULL;
    void *p_input, *p_output;
    xf_thread_t dec_thread;
    unsigned char dec_stack[STACK_SIZE];
    xaf_comp_status dec_status;
    int dec_info[4];
    char *filename_ptr;
    void *dec_thread_args[NUM_THREAD_ARGS];
    FILE *fp, *ofp;
    void *dec_inbuf[2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int i;
    xaf_comp_type comp_type;
    xf_id_t dec_id;
    int (*dec_setup)(void *p_comp);

    xaf_format_t dec_format;
    int num_comp;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    mem_obj_t* mem_handle;
    int exec_count = 0; /* counter for multiple execs without comp_delete */

#ifdef XAF_PROFILE
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dec_cycles = 0;
    dsp_comps_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
#endif

    memset(&dec_format, 0, sizeof(xaf_format_t));
    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Opus Dec\'"), "print_verinfo");

    /* ...start xos */
    board_id = start_rtos();

   /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Opus Dec\' Sample App");

    /* ...check input arguments */
    if (argc != 3)
    {
        PRINT_USAGE;
        return 0;
    }

    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][8]);

        dec_id    = "audio-decoder/opus";
        dec_setup = opus_dec_setup;

        /* ...open file */
        if ((fp = fio_fopen(filename_ptr, "rb")) == NULL)
        {
           FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
           exit(-1);
        }
    }
    else
    {
        PRINT_USAGE;
        return 0;
    }

    if(NULL != strstr(argv[2], "-outfile:"))
    {
        filename_ptr = (char *)&(argv[2][9]);
        if ((ofp = fio_fopen(filename_ptr, "wb")) == NULL)
        {
           FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
           exit(-1);
        }
    }
    else
    {
        PRINT_USAGE;
        return 0;
    }

    p_input  = fp;
    p_output = ofp;

    mem_handle = mem_init();

    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.pmem_malloc =  mem_malloc;
    adev_config.pmem_free =  mem_free;
    adev_config.audio_framework_buffer_size =  audio_frmwk_buf_size;
    adev_config.audio_component_buffer_size =  audio_comp_buf_size;
    TST_CHK_API(xaf_adev_open(&p_adev, &adev_config),  "xaf_adev_open");

    FIO_PRINTF(stdout, "Audio Device Ready\n");

    /* ...create decoder component */
    comp_type = XAF_DECODER;
    //TST_CHK_API_COMP_CREATE(p_adev, &p_decoder, dec_id, 2, 1, &dec_inbuf[0], comp_type, "xaf_comp_create");
    {
        xaf_comp_config_t comp_config;
        TST_CHK_API(xaf_comp_config_default_init(&comp_config), "xaf_comp_config_default_init");
#ifndef XA_DISABLE_EVENT
        comp_config.error_channel_ctl = g_enable_error_channel_flag;
#endif        
        comp_config.comp_id = dec_id;
        comp_config.comp_type = comp_type;
        comp_config.num_input_buffers = 2;
        comp_config.num_output_buffers = 1;
        comp_config.pp_inbuf = (pVOID (*)[XAF_MAX_INBUFS])&dec_inbuf[0];
        comp_config.cfg_param_ext_buf_size_max = 20;
        TST_CHK_API(xaf_comp_create(p_adev, &p_decoder, &comp_config), "xaf_comp_create");
    }
    TST_CHK_API(dec_setup(p_decoder), "dec_setup");

    /* ...start decoder component */
    TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_START_FLAG), "xaf_comp_process");
#if 1
    /* ...feed input to decoder component */
    for (i=0; i<2; i++)
    {
        TST_CHK_API(read_input(dec_inbuf[i], buf_length, &read_length, p_input, comp_type), "read_input");

        if (read_length)
            TST_CHK_API(xaf_comp_process(p_adev, p_decoder, dec_inbuf[i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
        else
        {    
            TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
            //break;
        }
    }
#endif
    /* ...initialization loop */
    while (1)
    {
        TST_CHK_API(xaf_comp_get_status(p_adev, p_decoder, &dec_status, &dec_info[0]), "xaf_comp_get_status");

        if (dec_status == XAF_INIT_DONE || dec_status == XAF_EXEC_DONE) break;
        if (dec_status == XAF_NEED_INPUT)
        {
            void *p_buf = (void *) dec_info[0];
            int size    = dec_info[1];

            TST_CHK_API(read_input(p_buf, size, &read_length, p_input, comp_type), "read_input");

            if (read_length)
                TST_CHK_API(xaf_comp_process(p_adev, p_decoder, p_buf, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else
            {    
                TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
                break;
            }
        }
    }

    if (dec_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        exit(-1);
    }

    TST_CHK_API(get_opus_dec_config(p_decoder, &dec_format), "get_comp_config");

    while(1)
    {
        dec_cycles = 0;
#ifdef XAF_PROFILE
    	clk_start();
#endif
		dec_thread_args[0]= p_adev;
		dec_thread_args[1]= p_decoder;
		dec_thread_args[2]= p_input;
		dec_thread_args[3]= p_output;
		dec_thread_args[4]= &comp_type;
		dec_thread_args[5]= (void *)dec_id;
		dec_thread_args[6]= (void *)&i; //dummy

		/* ...init done, begin execution thread */
		__xf_thread_create(&dec_thread, comp_process_entry, &dec_thread_args[0], "Decoder Thread", dec_stack, STACK_SIZE, XAF_APP_THREADS_PRIORITY);

		__xf_thread_join(&dec_thread, NULL);

#ifdef XAF_PROFILE
		compute_total_frmwrk_cycles();
		clk_stop();
#endif

		{
			/* collect memory stats before closing the device */
			WORD32 meminfo[5];
			if(xaf_get_mem_stats(p_adev, &meminfo[0]))
			{
				FIO_PRINTF(stdout,"Init is incomplete, reliable memory stats are unavailable.\n");
			}
			else
			{
				FIO_PRINTF(stderr,"Local Memory used by DSP Components, in bytes            : %8d of %8d\n", meminfo[0], adev_config.audio_component_buffer_size);
				FIO_PRINTF(stderr,"Shared Memory used by Components and Framework, in bytes : %8d of %8d\n", meminfo[1], adev_config.audio_framework_buffer_size);
				FIO_PRINTF(stderr,"Local Memory used by Framework, in bytes                 : %8d\n", meminfo[2]);
			}
		}
		/* ...exec done, clean-up */
		__xf_thread_destroy(&dec_thread);

		exec_count++;
		fprintf(stderr,"completed exec_count:%d\n", exec_count);
		if(exec_count >= MAX_EXEC_COUNT)
		{
			break;
		}

        /* ...feed input again, to test reinit support */
		fseek(p_input, 0, SEEK_SET);
		fseek(p_output, 0, SEEK_SET);
		/* ...feed input to decoder component */
		for (i=0; i<2; i++)
		{
			TST_CHK_API(read_input(dec_inbuf[i], buf_length, &read_length, p_input, comp_type), "read_input");

			if (read_length)
				TST_CHK_API(xaf_comp_process(p_adev, p_decoder, dec_inbuf[i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
			else
			{
				TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
				break;
			}
		}//for
    }//while(1)

    TST_CHK_API(xaf_comp_delete(p_decoder), "xaf_comp_delete");
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");

    mem_exit();

    dsp_comps_cycles = dec_cycles;

    dsp_mcps = compute_comp_mcps(num_bytes_write, dec_cycles, dec_format, &strm_duration);

    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");

    if (fp)  fio_fclose(fp);
    if (ofp) fio_fclose(ofp);
    
    fio_quit();
    
    /* ...deinitialize tracing facility */
    TRACE_DEINIT();
    
    return 0;
}
