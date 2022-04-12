/*
* Copyright (c) 2015-2022 Cadence Design Systems Inc.
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

#include "audio/xa-opus-encoder-api.h"
#include "audio/xa-opus-decoder-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:infilename.out(or .pcm) -infile:filename.bit(or .ogg) -outfile:filename.bit -outfile:filename.out\n\n", argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 10)  //Extra
#define AUDIO_COMP_BUF_SIZE    (1024 << 7) + (1024 << 10)

#define NUM_COMP_IN_GRAPH       2

#define XA_EXT_CFG_ID_OFFSET       0
#define XA_EXT_CFG_BUF_PTR_OFFSET  1

//#define ENABLE_RAW_OPUS_SET_CONFIG

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, enc_cycles, dec_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec22(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_aec23(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_pcm_split(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_mimo_mix(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_wwd(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_wwd_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
//XA_ERRORCODE xa_opus_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
//XA_ERRORCODE xa_opus_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}


//encoder parameters
#define OPUS_ENC_PCM_WIDTH              16
#define OPUS_ENC_SAMPLE_RATE            16000
#define OPUS_ENC_CHANNELS               1

#define OPUS_ENC_APPLICATION		    XA_OPUS_APPLICATION_VOIP
#define OPUS_ENC_BITRATE			    20000
#define OPUS_ENC_MAX_PAYLOAD		    1500
#define OPUS_ENC_COMPLEXITY			    10
#define OPUS_ENC_RESET_STATE		    0
#define OPUS_ENC_FRAME_SIZE_IN_SAMPLES	320  //20 ms of 16KHz samples input
#define MAX_EXEC_COUNT                  1

//decoder parameters
#define OPUS_DEC_PCM_WIDTH              16
#define OPUS_DEC_SAMPLE_RATE            48000
#define OPUS_DEC_CHANNELS               6

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
#ifndef PACK_WS_DUMMY
    {
#define OPUS_DEC_NUM_SET_PARAMS_EXT	1
        int param_ext[OPUS_DEC_NUM_SET_PARAMS_EXT * 2];
        xaf_ext_buffer_t ext_buf[OPUS_DEC_NUM_SET_PARAMS_EXT];
        memset(ext_buf, 0, sizeof(xaf_ext_buffer_t) * OPUS_DEC_NUM_SET_PARAMS_EXT);

        WORD8 stream_map[XA_OPUS_MAX_NUM_CHANNELS]= {0,4,1,2,3,5,0,0};

        ext_buf[0].max_data_size = sizeof(stream_map);
        ext_buf[0].valid_data_size = sizeof(stream_map);
        ext_buf[0].ext_config_flags |= XAF_EXT_PARAM_SET_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
        //ext_buf[0].ext_config_flags &= XAF_EXT_PARAM_CLEAR_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
        ext_buf[0].data = (UWORD8 *) stream_map;

        param_ext[0*2+XA_EXT_CFG_ID_OFFSET] = XA_OPUS_DEC_CONFIG_PARAM_STREAM_MAP;
        param_ext[0*2+XA_EXT_CFG_BUF_PTR_OFFSET] = (int) &ext_buf[0];

        ret = xaf_comp_set_config_ext(p_decoder, OPUS_DEC_NUM_SET_PARAMS_EXT, param_ext);
    }
#endif //PACK_WS_DUMMY
    return ret;
}
#else //ENABLE_RAW_OPUS_SET_CONFIG
static int opus_dec_setup(void *p_decoder)
{
    int param[20];
    int ret;

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

    ret = xaf_comp_set_config(p_decoder, 9, &param[0]);
    return ret;
}
#endif //ENABLE_RAW_OPUS_SET_CONFIG

static int opus_enc_setup(void *p_encoder)
{
#ifndef PACK_WS_DUMMY
    int param[20];

    param[0] = XA_OPUS_ENC_CONFIG_PARAM_PCM_WIDTH;
    param[1] = OPUS_ENC_PCM_WIDTH;

    param[2] = XA_OPUS_ENC_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = OPUS_ENC_SAMPLE_RATE;

    param[4] = XA_OPUS_ENC_CONFIG_PARAM_CHANNELS;
    param[5] = OPUS_ENC_CHANNELS;

    param[6] = XA_OPUS_ENC_CONFIG_PARAM_APPLICATION;
    param[7] = OPUS_ENC_APPLICATION;

    param[8] = XA_OPUS_ENC_CONFIG_PARAM_BITRATE;
    param[9] = OPUS_ENC_BITRATE;

    param[10] = XA_OPUS_ENC_CONFIG_PARAM_FRAME_SIZE;
    param[11] = OPUS_ENC_FRAME_SIZE_IN_SAMPLES;

    param[12] = XA_OPUS_ENC_CONFIG_PARAM_MAX_PAYLOAD;
    param[13] = OPUS_ENC_MAX_PAYLOAD;

    param[14] = XA_OPUS_ENC_CONFIG_PARAM_COMPLEXITY;
    param[15] = OPUS_ENC_COMPLEXITY;

    param[16] = XA_OPUS_ENC_CONFIG_PARAM_SIGNAL_TYPE;
    param[17] = XA_OPUS_SIGNAL_VOICE;

    param[18] = XA_OPUS_ENC_CONFIG_PARAM_RESET_STATE;
    param[19] = OPUS_ENC_RESET_STATE;

    return(xaf_comp_set_config(p_encoder, 10, &param[0]));
#else //PACK_WS_DUMMY
    return 0;
#endif //PACK_WS_DUMMY
}

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

#ifndef PACK_WS_DUMMY
    {
#define OPUS_DEC_NUM_GET_PARAMS_EXT	1
        int param_ext[OPUS_DEC_NUM_GET_PARAMS_EXT * 2];

        xaf_ext_buffer_t ext_buf[OPUS_DEC_NUM_GET_PARAMS_EXT];
        memset(ext_buf, 0, sizeof(xaf_ext_buffer_t) * OPUS_DEC_NUM_GET_PARAMS_EXT);

        WORD8 stream_map[XA_OPUS_MAX_NUM_CHANNELS]= {0,0,0,0,0,0,0,0};
        
        ext_buf[0].max_data_size = sizeof(stream_map);
        ext_buf[0].valid_data_size = sizeof(stream_map);
        //ext_buf[0].ext_config_flags |= XAF_EXT_PARAM_SET_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
        ext_buf[0].ext_config_flags &= XAF_EXT_PARAM_CLEAR_FLAG(XAF_EXT_PARAM_FLAG_OFFSET_ZERO_COPY);
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
#endif //PACK_WS_DUMMY

    return 0;
}

static int get_opus_enc_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;

    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");

    param[0] = XA_OPUS_ENC_CONFIG_PARAM_PCM_WIDTH;
    param[2] = XA_OPUS_ENC_CONFIG_PARAM_SAMPLE_RATE;
    param[4] = XA_OPUS_ENC_CONFIG_PARAM_CHANNELS;

    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->pcm_width = param[1];
    comp_format->sample_rate = param[3];
    comp_format->channels = param[5];

    return 0;
}

void fio_quit()
{
    return;
}

int main_task(int argc, char **argv)
{
    void *p_adev = NULL;
    void *p_comp[NUM_COMP_IN_GRAPH] = {0};
    void *p_input[NUM_COMP_IN_GRAPH];
    void *p_output[NUM_COMP_IN_GRAPH];
    xf_thread_t comp_thread[NUM_COMP_IN_GRAPH];
    unsigned char comp_stack[NUM_COMP_IN_GRAPH][STACK_SIZE];
    xaf_comp_status comp_status;
    int comp_info[4];
    char *filename_ptr;
    FILE *fp, *ofp;    
    void *thread_args[NUM_COMP_IN_GRAPH][NUM_THREAD_ARGS];
    void *comp_inbuf[NUM_COMP_IN_GRAPH][2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int i, k;      
    xf_id_t comp_id[NUM_COMP_IN_GRAPH];
    int (*comp_setup[NUM_COMP_IN_GRAPH])(void *p_comp);
    int (*get_comp_config[NUM_COMP_IN_GRAPH])(void *p_comp, xaf_format_t *comp_format);
    const char *ext;
    xaf_format_t comp_format[NUM_COMP_IN_GRAPH];
    int num_comp;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    mem_obj_t* mem_handle;
    xaf_comp_type comp_type[2];
    double enc_strm_duration;
    double dec_strm_duration;

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


    memset(&comp_format[0], 0, sizeof(xaf_format_t)*NUM_COMP_IN_GRAPH);

    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...start xos */
    board_id = start_rtos();
 
    /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Full duplex Opus\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Full duplex\' Sample App");

    /* ...check input arguments */
    if (argc != (NUM_COMP_IN_GRAPH*2+1))
    {
        PRINT_USAGE;
        return 0;
    }
    
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        if(NULL != strstr(argv[k+1], "-infile:"))
        {
            filename_ptr = (char *)&(argv[k+1][8]);
            ext = strrchr(argv[k+1], '.');
			if(ext!=NULL)
			{
				ext++;
				if ((!strcmp(ext, "pcm")) || (!strcmp(ext, "out")))
				{
					comp_id[k]    = "audio-encoder/opus";
					comp_setup[k] = opus_enc_setup;
					comp_type[k]  = XAF_ENCODER;
					get_comp_config[k] = get_opus_enc_config;

				}
				else if ((!strcmp(ext, "ogg")) || (!strcmp(ext, "bit")))
				{
					comp_id[k]    = "audio-decoder/opus";
					comp_setup[k] = opus_dec_setup;
					comp_type[k]  = XAF_DECODER;
					get_comp_config[k] = get_opus_dec_config;
				}
				else 
				{
					FIO_PRINTF(stderr, "Unknown Extension '%s'\n", ext);
					exit(-1);
				}
			}
			else
			{
				FIO_PRINTF(stderr, "Failed to open infile %d\n",k+1);
				exit(-1);
			}
            /* ...open file */
            if ((fp = fio_fopen(filename_ptr, "rb")) == NULL)
            {
                FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
                exit(-1);
            }
            FIO_PRINTF(stderr, "opened '%s'\n", filename_ptr);

            p_input[k]  = fp;      
        }
        else
        {

            PRINT_USAGE;
            return 0;
        }    
        
        /* ...out file opening */
        int idx = (NUM_COMP_IN_GRAPH==1)?2:3;
        if(NULL != strstr(argv[k+idx], "-outfile:"))
        {
            filename_ptr = (char *)&(argv[k+idx][9]);
            if ((ofp = fio_fopen(filename_ptr, "wb")) == NULL)
            {
                FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
                exit(-1);
            }
            FIO_PRINTF(stderr, "opened '%s'\n", filename_ptr);

            p_output[k] = ofp;
        }
        else
        {
            PRINT_USAGE;
            return 0;
        }
    }

    mem_handle = mem_init();

    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.pmem_malloc =  mem_malloc;
    adev_config.pmem_free =  mem_free;
    adev_config.audio_framework_buffer_size =  audio_frmwk_buf_size;
    adev_config.audio_component_buffer_size =  audio_comp_buf_size;
    TST_CHK_API(xaf_adev_open(&p_adev, &adev_config),  "xaf_adev_open");
    
    FIO_PRINTF(stdout, "Audio Device Ready\n");
    
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        /* ...create encoder component */
        TST_CHK_API_COMP_CREATE(p_adev, &p_comp[k], comp_id[k], 2, 1, &comp_inbuf[k][0], comp_type[k], "xaf_comp_create");
        TST_CHK_API(comp_setup[k](p_comp[k]), "comp_setup");
    
    	/* ...start encoder component */
        TST_CHK_API(xaf_comp_process(p_adev, p_comp[k], NULL, 0, XAF_START_FLAG), "xaf_comp_process");
    
        /* ...feed input to encoder component */
        for (i=0; i<2; i++)
        {
            TST_CHK_API(read_input(comp_inbuf[k][i], buf_length, &read_length, p_input[k], comp_type[k]), "read_input");
    
            if (read_length) 
                TST_CHK_API(xaf_comp_process(p_adev, p_comp[k], comp_inbuf[k][i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else 
            {    
                TST_CHK_API(xaf_comp_process(p_adev, p_comp[k], NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
                break;
            }
        }
    
        /* ...initialization loop */    
        while (1)
        {
            TST_CHK_API(xaf_comp_get_status(p_adev, p_comp[k], &comp_status, &comp_info[0]), "xaf_comp_get_status");
            
            if (comp_status == XAF_INIT_DONE || comp_status == XAF_EXEC_DONE) break;
    
            if (comp_status == XAF_NEED_INPUT)
            {
                void *p_buf = (void *) comp_info[0]; 
                int size    = comp_info[1];
                
                TST_CHK_API(read_input(p_buf, size, &read_length, p_input[k], comp_type[k]), "read_input");
    
                if (read_length) 
                    TST_CHK_API(xaf_comp_process(p_adev, p_comp[k], p_buf, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
                else
                { 
                    TST_CHK_API(xaf_comp_process(p_adev, p_comp[k], NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
                    break;
                }
            }
        }
    
        if (comp_status != XAF_INIT_DONE)
        {
            FIO_PRINTF(stderr, "Failed to init");
            exit(-1);
        }    
        
        TST_CHK_API(get_comp_config[k](p_comp[k], &comp_format[k]), "get_comp_config");
    }
    
#ifdef XAF_PROFILE
    clk_start();
    
#endif
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        thread_args[k][0]= p_adev;
        thread_args[k][1]= p_comp[k];
        thread_args[k][2]= p_input[k];
        thread_args[k][3]= p_output[k];
        thread_args[k][4]= &comp_type[k];
        thread_args[k][5]= (void *)comp_id[k];
        thread_args[k][6]= (void *)&k;

        /* ...init done, begin execution thread */
        __xf_thread_create(&comp_thread[k], comp_process_entry, thread_args[k], "Comp Thread", comp_stack[k], STACK_SIZE, XAF_APP_THREADS_PRIORITY);
    }

    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        __xf_thread_join(&comp_thread[k], NULL);
    }
    
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
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {    
        __xf_thread_destroy(&comp_thread[k]);
        TST_CHK_API(xaf_comp_delete(p_comp[k]), "xaf_comp_delete");
        if(p_input[k]) 
            fio_fclose(p_input[k]);
        if(p_output[k])
            fio_fclose(p_output[k]);        
    }
    
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");
    
    mem_exit();
    
    dsp_comps_cycles = enc_cycles + dec_cycles;

    dsp_mcps = 0;
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        if(comp_type[k] == XAF_ENCODER)
        {
            dsp_mcps += compute_comp_mcps(num_bytes_read, enc_cycles, comp_format[k], &enc_strm_duration);
            //printf("\n\nOpus Encoder took %f\n", compute_comp_mcps(num_bytes_read, enc_cycles, comp_format[k], &enc_strm_duration));
            strm_duration = enc_strm_duration;
        }
        if(comp_type[k] == XAF_DECODER)
        {
            dsp_mcps += compute_comp_mcps(num_bytes_write, dec_cycles, comp_format[k], &dec_strm_duration);
            //printf("\nOpus Decoder took %f \n", compute_comp_mcps(num_bytes_write, dec_cycles, comp_format[k], &dec_strm_duration));
            strm_duration = dec_strm_duration;
        }
    }

    if( NUM_COMP_IN_GRAPH>1)
    {
    if(enc_strm_duration > dec_strm_duration)
       strm_duration = enc_strm_duration;
    else
        strm_duration = dec_strm_duration;
    }

    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");
    
    fio_quit();
    
    /* ...deinitialize tracing facility */
    TRACE_DEINIT();
        
    return 0;
}

