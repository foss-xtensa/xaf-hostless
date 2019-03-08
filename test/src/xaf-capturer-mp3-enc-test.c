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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "audio/xa_mp3_enc_api.h"
#include "audio/xa-capturer-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s  -outfile:out_filename.mp3  -samples:<samples to be captured(zero for endless capturing)>\n\n", argv[0]);
#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)

#if defined(CORE_HIFI3) || defined(CORE_HIFI4)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7) + (1024 << 5)  + (1024 << 3)
#else
#define AUDIO_COMP_BUF_SIZE    (1024 << 7)
#endif

#define NUM_COMP_IN_GRAPH       (2)

//encoder parameters
#define MP3_ENC_PCM_WIDTH       (16)
#define MP3_ENC_SAMPLE_RATE     (44100)
#define MP3_ENC_NUM_CH          (2)
#define CAPTURER_PCM_WIDTH       (16)
#define CAPTURER_SAMPLE_RATE     (44100)
#define CAPTURER_NUM_CH          (2)

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, enc_cycles,capturer_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}

static int capturer_setup(void *p_capturer,xaf_format_t capturer_format,UWORD64 sample_end)
{
    int param[8];

    param[0] = XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH;
    param[1] = capturer_format.pcm_width;
    param[2] = XA_CAPTURER_CONFIG_PARAM_CHANNELS;
    param[3] = capturer_format.channels;
    param[4] = XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE;
    param[5] = capturer_format.sample_rate;
    param[6] = XA_CAPTURER_CONFIG_PARAM_SAMPLE_END;
    param[7] = sample_end;
    return(xaf_comp_set_config(p_capturer, 4, &param[0]));
}

static int mp3_enc_setup(void *p_encoder,xaf_format_t encoder_format)
{
    int param[6];

    param[0] = XA_MP3ENC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = encoder_format.pcm_width;
    param[2] = XA_MP3ENC_CONFIG_PARAM_SAMP_FREQ;
    param[3] = encoder_format.sample_rate;
    param[4] = XA_MP3ENC_CONFIG_PARAM_NUM_CHANNELS;
    param[5] = encoder_format.channels;

    return(xaf_comp_set_config(p_encoder, 3, &param[0]));
}


static int get_mp3_enc_get_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;


    TST_CHK_PTR(p_comp, "get_mp3_enc_config");
    TST_CHK_PTR(comp_format, "get_mp3_enc_config");
    param[0] = XA_MP3ENC_CONFIG_PARAM_NUM_CHANNELS;
    param[2] = XA_MP3ENC_CONFIG_PARAM_PCM_WDSZ;
    param[4] = XA_MP3ENC_CONFIG_PARAM_SAMP_FREQ;
    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];

    return 0;
}

static int capturer_get_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[8];
    int ret;


    TST_CHK_PTR(p_comp, "get_mp3_enc_config");
    TST_CHK_PTR(comp_format, "get_mp3_enc_config");
    param[0] = XA_CAPTURER_CONFIG_PARAM_CHANNELS;
    param[2] = XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE;
    param[6] = XA_CAPTURER_CONFIG_PARAM_BYTES_PRODUCED;
    ret = xaf_comp_get_config(p_comp, 4, &param[0]);
    if(ret < 0)
        return ret;
    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];
    comp_format->output_produced = param[7];
    return 0;
}

void fio_quit()
{
    return;
}

#ifdef RTK_HW
#define XA_MAX_ARGS    3
#define XA_MAX_ARG_LEN 128

char pGargv[XA_MAX_ARGS][XA_MAX_ARG_LEN] =
{
    "xa_af_mp3_enc_capturer_test",
    "-outfile:..\\..\\test_out\\capturer.mp3",
    "-samples:960000",
};

int main()
#else // RTK_SIM
int main(int argc, const char **argv)
#endif
{

    void *p_adev = NULL;
    void *p_output;
    XosThread mp3_encoder_thread;
    void *p_renderer = NULL;
    unsigned char mp3_encoder_stack[STACK_SIZE];
    void * p_mp3_encoder  = NULL;
    void * p_capturer  = NULL;
    xaf_comp_status mp3_encoder_status;
    xaf_comp_status capturer_status;
    int mp3_encoder_info[4];
    int capturer_info[4];
    char *filename_ptr;
    char *sample_cnt_ptr;
    void *mp3_encoder_thread_args[5];
    FILE *ofp = NULL;
    int buf_length = XAF_INBUF_SIZE;
    xaf_format_t encoder_format;
    xaf_format_t capturer_format;
    xf_id_t comp_id;
    int (*comp_setup)(void * p_comp);
    const char *ext;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    int num_comp;
    int ret = 0;
    int i = 0;
    mem_obj_t* mem_handle;
    xaf_comp_type comp_type;
    long long int sammple_end_capturer = 0;
    double enc_mcps = 0,wait_mcps=0;

#ifdef XAF_PROFILE
    clk_t frmwk_start, frmwk_stop;
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dsp_comps_cycles = 0;
    enc_cycles = 0;
    capturer_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
#endif
#ifdef RTK_HW
    int argc;
    const char *argv[XA_MAX_ARGS];
    for (argc = 0; argc < XA_MAX_ARGS; argc++)
        argv[argc] = &pGargv[argc][0];
#endif

    memset(&encoder_format, 0, sizeof(xaf_format_t));
    memset(&capturer_format, 0, sizeof(xaf_format_t));

    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...start xos */
    board_id = start_xos();

#ifdef RTK_HW
     if(board_id != VENDER_ID){
         fprintf(stderr," RealTek hardware not detected \n");
         return 0;
     }
#endif


    /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Capturer + Mp3 Encoder\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Capturer + Mp3 Encoder\' Sample App");

    /* ...check input arguments */
    if (argc != 3)
    {
        PRINT_USAGE;
        return 0;
    }

    if(NULL != strstr(argv[1], "-outfile:"))
    {
        filename_ptr = (char *)&(argv[1][9]);
        ext = strrchr(argv[1], '.');
        ext++;
        if (strcmp(ext, "mp3"))
        {
            FIO_PRINTF(stderr, "Unknown input file format '%s'\n", ext);
            exit(-1);
        }

        /* ...open file */
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
    if(NULL != strstr(argv[2], "-samples:"))
    {
        sample_cnt_ptr = (char *)&(argv[2][9]);

        if ((*sample_cnt_ptr) == '\0' )
        {
            FIO_PRINTF(stderr, "Samples to be produced is not entererd\n");
            exit(-1);
        }
        sammple_end_capturer = atoi(sample_cnt_ptr);


    }
    else
    {
        PRINT_USAGE;
        return 0;
    }
    p_output = ofp;
    mem_handle = mem_init();
    TST_CHK_API(xaf_adev_open(&p_adev, audio_frmwk_buf_size, audio_comp_buf_size, mem_malloc, mem_free), "xaf_adev_open");
    FIO_PRINTF(stdout,"Audio Device Ready\n");

    /* ...create capturer component */
    comp_type = XAF_CAPTURER;
    capturer_format.sample_rate = CAPTURER_SAMPLE_RATE;
    capturer_format.channels = CAPTURER_NUM_CH;
    capturer_format.pcm_width = CAPTURER_PCM_WIDTH;
    TST_CHK_API(xaf_comp_create(p_adev, &p_capturer, "capturer", 0, 0, NULL, comp_type),"xaf_comp_create");
    TST_CHK_API(capturer_setup(p_capturer, capturer_format,sammple_end_capturer), "capturer_setup");
    /* ...start capturer component */
    TST_CHK_API(xaf_comp_process(p_adev, p_capturer, NULL, 0, XAF_START_FLAG),"xaf_comp_process");
    TST_CHK_API(xaf_comp_get_status(p_adev, p_capturer, &capturer_status, &capturer_info[0]), "xaf_comp_get_status");
    if (capturer_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        exit(-1);
    }

     /* ...create mp3 encoder component */
    encoder_format.sample_rate = MP3_ENC_SAMPLE_RATE;
    encoder_format.channels = MP3_ENC_NUM_CH;
    encoder_format.pcm_width = MP3_ENC_PCM_WIDTH;
    TST_CHK_API(xaf_comp_create(p_adev, &p_mp3_encoder, "audio-encoder/mp3", 0, 1, NULL, XAF_ENCODER), "xaf_comp_create");
    TST_CHK_API(mp3_enc_setup(p_mp3_encoder,encoder_format), "mp3_encoder_setup");
    TST_CHK_API(xaf_connect(p_capturer, p_mp3_encoder, 4), "xaf_connect");
    TST_CHK_API(xaf_comp_process(p_adev, p_mp3_encoder, NULL, 0, XAF_START_FLAG), "xaf_comp_process");
    TST_CHK_API(xaf_comp_get_status(p_adev, p_mp3_encoder, &mp3_encoder_status, &mp3_encoder_info[0]), "xaf_comp_get_status");
    if (mp3_encoder_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stdout,"Failed to init \n");
        exit(-1);
    }
#ifdef XAF_PROFILE
    clk_start();

#endif
    comp_type = XAF_ENCODER;
    mp3_encoder_thread_args[0] = p_adev;
    mp3_encoder_thread_args[1] = p_mp3_encoder;
    mp3_encoder_thread_args[2] = NULL;
    mp3_encoder_thread_args[3] = p_output;
    mp3_encoder_thread_args[4] = &comp_type;
    ret = xos_thread_create(&mp3_encoder_thread, 0, comp_process_entry, &mp3_encoder_thread_args[0], "Mp3 encoder Thread", mp3_encoder_stack, STACK_SIZE, 7, 0, 0);
    if(ret != XOS_OK)
    {
        FIO_PRINTF(stdout,"Failed to create mp3 encoder thread  : %d\n", ret);
        exit(-1);
    }
    ret = xos_thread_join(&mp3_encoder_thread, &i);
    if(ret != XOS_OK)
    {
        FIO_PRINTF(stdout,"Decoder thread exit Failed : %d \n", ret);
        exit(-1);
    }


#ifdef XAF_PROFILE
   compute_total_frmwrk_cycles();
   clk_stop();
    //tot_cycles = clk_diff(frmwk_stop, frmwk_start);
#endif

    {
        /* collect memory stats before closing the device */
        WORD32 meminfo[3];
        if(xaf_get_mem_stats(p_adev, &meminfo[0]))
        {
            FIO_PRINTF(stdout,"Init is incomplete, reliable memory stats are unavailable.\n");
        }
        else
        {
            FIO_PRINTF(stderr,"Local Memory used by DSP Components, in bytes            : %8d of %8d\n", meminfo[0], AUDIO_COMP_BUF_SIZE);
            FIO_PRINTF(stderr,"Shared Memory used by Components and Framework, in bytes : %8d of %8d\n", meminfo[1], AUDIO_FRMWK_BUF_SIZE);
            FIO_PRINTF(stderr,"Local Memory used by Framework, in bytes                 : %8d\n", meminfo[2]);
        }
    }
    /* ...exec done, clean-up */
    xos_thread_delete(&mp3_encoder_thread);
    TST_CHK_API(capturer_get_config(p_capturer, &capturer_format), "capturer get config");
    TST_CHK_API(get_mp3_enc_get_config(p_mp3_encoder, &encoder_format), "capturer get config");
    TST_CHK_API(xaf_comp_delete(p_mp3_encoder), "xaf_comp_delete");
    TST_CHK_API(xaf_comp_delete(p_capturer), "xaf_comp_delete");
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");
    mem_exit();

    dsp_comps_cycles = enc_cycles + capturer_cycles;
    enc_mcps = compute_comp_mcps(capturer_format.output_produced, enc_cycles, encoder_format, &strm_duration);
    dsp_mcps = compute_comp_mcps(capturer_format.output_produced, capturer_cycles, capturer_format, &strm_duration);
    dsp_mcps += enc_mcps;

    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");


    if (ofp) fio_fclose(ofp);

    fio_quit();

    return 0;
}

