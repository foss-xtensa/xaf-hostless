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

#include "audio/xa-pcm-gain-api.h"
#include "audio/xa-renderer-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:in_filename.pcm\n\n", argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7)

#define NUM_COMP_IN_GRAPH       2

//component parameters
#define PCM_GAIN_SAMPLE_WIDTH   16
// supports only 16-bit PCM

#define PCM_GAIN_NUM_CH         2
// supports 1 and 2 channels only

#define PCM_GAIN_IDX_FOR_GAIN   1
//gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}

#define PCM_GAIN_SAMPLE_RATE    44100

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, pcm_gain_cycles, renderer_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}

static int pcm_gain_setup(void *p_comp)
{
    int param[10];
    int pcm_width = PCM_GAIN_SAMPLE_WIDTH;        // supports only 16-bit PCM
    int num_ch = PCM_GAIN_NUM_CH;                 // supports 1 and 2 channels only
    int sample_rate = PCM_GAIN_SAMPLE_RATE;
    int gain_idx = PCM_GAIN_IDX_FOR_GAIN;         //gain index range is 0 to 6 -> {0db, -6db, -12db, -18db, 6db, 12db, 18db}
    int frame_size = XAF_INBUF_SIZE;

    param[0] = XA_PCM_GAIN_CONFIG_PARAM_CHANNELS;
    param[1] = num_ch;
    param[2] = XA_PCM_GAIN_CONFIG_PARAM_SAMPLE_RATE;
    param[3] = sample_rate;
    param[4] = XA_PCM_GAIN_CONFIG_PARAM_PCM_WIDTH;
    param[5] = pcm_width;
    param[6] = XA_PCM_GAIN_CONFIG_PARAM_FRAME_SIZE;
    param[7] = frame_size;
    param[8] = XA_PCM_GAIN_CONFIG_PARAM_GAIN_FACTOR;
    param[9] = gain_idx;

    return(xaf_comp_set_config(p_comp, 5, &param[0]));
}

static int renderer_setup(void *p_renderer,xaf_format_t renderer_format)
{
    int param[6];

    param[0] = XA_RENDERER_CONFIG_PARAM_PCM_WIDTH;
    param[1] = renderer_format.pcm_width;
    param[2] = XA_RENDERER_CONFIG_PARAM_CHANNELS;
    param[3] = renderer_format.channels;
    param[4] = XA_RENDERER_CONFIG_PARAM_SAMPLE_RATE;
    param[5] = renderer_format.sample_rate;
    return(xaf_comp_set_config(p_renderer, 3, &param[0]));
}
static int get_comp_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;


    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");

    param[0] = XA_PCM_GAIN_CONFIG_PARAM_CHANNELS;
    param[2] = XA_PCM_GAIN_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_PCM_GAIN_CONFIG_PARAM_SAMPLE_RATE;


    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];

    return 0;
}

static int renderer_get_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[8];
    int ret;
    
    
    TST_CHK_PTR(p_comp, "get_renderer_config");
    TST_CHK_PTR(comp_format, "get_renderer_config");
    param[0] = XA_RENDERER_CONFIG_PARAM_CHANNELS;
    param[2] = XA_RENDERER_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_RENDERER_CONFIG_PARAM_SAMPLE_RATE;    
    param[6] = XA_RENDERER_CONFIG_PARAM_BYTES_PRODUCED;
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
#define XA_MAX_ARGS    2
#define XA_MAX_ARG_LEN 128
char pGargv[XA_MAX_ARGS][XA_MAX_ARG_LEN] =
{
    "xa_af_gain_rend_test",
    "-infile:..\\..\\test_inp\\sine.pcm"

};

int main()
#else // RTK_SIM
int main(int argc, const char **argv)
#endif
{

    void *p_adev = NULL;
    void *p_input;
    XosThread pcm_gain_thread;
    void *p_renderer = NULL;
    unsigned char pcm_gain_stack[STACK_SIZE];
    void * p_pcm_gain  = NULL;
    xaf_comp_status pcm_gain_status;
    int pcm_gain_info[4];
    char *filename_ptr;
    void *pcm_gain_thread_args[5];
    FILE *fp = NULL;
    void *pcm_gain_inbuf[2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int i;
    xaf_format_t pcm_gain_format;
    xaf_format_t renderer_format;
    xf_id_t comp_id;
    int (*comp_setup)(void * p_comp);
    const char *ext;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    int num_comp;
    mem_obj_t* mem_handle;
    xaf_comp_type comp_type;

#ifdef XAF_PROFILE
    clk_t frmwk_start, frmwk_stop;
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dsp_comps_cycles = 0;
    pcm_gain_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
    renderer_cycles =0;
#endif
#ifdef RTK_HW
    int argc;
    const char *argv[XA_MAX_ARGS];
    for (argc = 0; argc < XA_MAX_ARGS; argc++)
        argv[argc] = &pGargv[argc][0];
#endif
    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;
    memset(&pcm_gain_format, 0, sizeof(xaf_format_t));
    memset(&renderer_format, 0, sizeof(xaf_format_t));

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
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Gain + Renderer\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Gain + Renderer\' Sample App");

    /* ...check input arguments */
    if (argc != 2)
    {
        PRINT_USAGE;
        return 0;
    }

    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][8]);
        ext = strrchr(argv[1], '.');
        ext++;
        if (!strcmp(ext, "pcm"))
        {
            comp_id    = "post-proc/pcm_gain";
            comp_setup = pcm_gain_setup;
        }
        else
        {
            FIO_PRINTF(stderr, "Unknown input file format '%s'\n", ext);
            exit(-1);
        }

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

    p_input  = fp;
    

    mem_handle = mem_init();

    TST_CHK_API(xaf_adev_open(&p_adev, audio_frmwk_buf_size, audio_comp_buf_size, mem_malloc, mem_free), "xaf_adev_open");

    FIO_PRINTF(stdout,"Audio Device Ready\n");

    /* ...create pcm gain component */
    comp_type = XAF_POST_PROC;
    TST_CHK_API(xaf_comp_create(p_adev, &p_pcm_gain, "post-proc/pcm_gain", 2, 0, &pcm_gain_inbuf[0], comp_type),"xaf_comp_create");
    TST_CHK_API(comp_setup(p_pcm_gain), "comp_setup");

    /* ...start pcm gain component */
    TST_CHK_API(xaf_comp_process(p_adev, p_pcm_gain, NULL, 0, XAF_START_FLAG),"xaf_comp_process");

    /* ...feed input to pcm gain component */
    for (i=0; i<2; i++)
    {
        TST_CHK_API(read_input(pcm_gain_inbuf[i], buf_length, &read_length, p_input, comp_type), "read_input");

        if (read_length)
            TST_CHK_API(xaf_comp_process(p_adev, p_pcm_gain, pcm_gain_inbuf[i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
        else
            break;
    }

    /* ...initialization loop */
    while (1)
    {
        TST_CHK_API(xaf_comp_get_status(p_adev, p_pcm_gain, &pcm_gain_status, &pcm_gain_info[0]), "xaf_comp_get_status");

        if (pcm_gain_status == XAF_INIT_DONE || pcm_gain_status == XAF_EXEC_DONE) break;

        if (pcm_gain_status == XAF_NEED_INPUT)
        {
            void *p_buf = (void *) pcm_gain_info[0];
            int size    = pcm_gain_info[1];

            TST_CHK_API(read_input(p_buf, size, &read_length, p_input,comp_type), "read_input");

            if (read_length)
                TST_CHK_API(xaf_comp_process(p_adev, p_pcm_gain, p_buf, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else
                break;
        }
    }

    if (pcm_gain_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        exit(-1);
    }

    TST_CHK_API(get_comp_config(p_pcm_gain, &pcm_gain_format), "get_comp_config");
    renderer_format.sample_rate = pcm_gain_format.sample_rate;
    renderer_format.channels = pcm_gain_format.channels;
    renderer_format.pcm_width = pcm_gain_format.pcm_width;
    TST_CHK_API(xaf_comp_create(p_adev, &p_renderer, "renderer", 0, 0, NULL, XAF_RENDERER), "xaf_comp_create");
    TST_CHK_API(renderer_setup(p_renderer, renderer_format), "renderer_setup");
    TST_CHK_API(xaf_connect(p_pcm_gain, p_renderer, 4), "renderer_connect");
#ifdef XAF_PROFILE
    clk_start();
    
#endif

    pcm_gain_thread_args[0] = p_adev;
    pcm_gain_thread_args[1] = p_pcm_gain;
    pcm_gain_thread_args[2] = p_input;
    pcm_gain_thread_args[3] = NULL;
    pcm_gain_thread_args[4] = &comp_type;

    /* ...init done, begin execution thread */
    xos_thread_create(&pcm_gain_thread, 0, comp_process_entry, &pcm_gain_thread_args[0], "Pcm Gain Thread", pcm_gain_stack, STACK_SIZE, 7, 0, 0);

    xos_thread_join(&pcm_gain_thread, &i);
    

#ifdef XAF_PROFILE
    compute_total_frmwrk_cycles();
    clk_stop();
    
#endif

    TST_CHK_API(renderer_get_config(p_renderer, &renderer_format), "renderer get config");
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
    xos_thread_delete(&pcm_gain_thread);
    TST_CHK_API(xaf_comp_delete(p_renderer), "xaf_comp_delete");
    TST_CHK_API(xaf_comp_delete(p_pcm_gain), "xaf_comp_delete");
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");

    mem_exit();

    dsp_comps_cycles = pcm_gain_cycles + renderer_cycles;

    dsp_mcps = compute_comp_mcps(renderer_format.output_produced, pcm_gain_cycles, pcm_gain_format, &strm_duration);
    
    dsp_mcps += compute_comp_mcps(renderer_format.output_produced, renderer_cycles, renderer_format, &strm_duration);
     
    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");

    if (fp)  fio_fclose(fp);
    
    fio_quit();
    return 0;
}

