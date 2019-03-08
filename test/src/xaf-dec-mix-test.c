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

#include "audio/xa_mp3_dec_api.h"
#include "audio/xa-mixer-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:filename.mp3 ... -outfile:filename.pcm\n\n", argv[0]); \
                    FIO_PRINTF(stdout, "    Up to 4 input files are supported in the commandline.\n\n");


#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)
#if defined(CORE_HIFI3) || defined(CORE_HIFI4)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7) + (1024 << 5) + (1024 << 3)
#else
#define AUDIO_COMP_BUF_SIZE    (1024 << 7) + (1024 << 5)
#endif

#define NUM_COMP_IN_GRAPH       3
#define MAX_INP_STRMS           4

//component parameters
#define MP3_DEC_PCM_WIDTH       16

#define MIXER_SAMPLE_RATE       44100
#define MIXER_NUM_CH            2
#define MIXER_PCM_WIDTH         16

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, dec_cycles, mix_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}

static int mp3_setup(void *p_decoder)
{
    int param[2];

    param[0] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_DEC_PCM_WIDTH;

    return(xaf_comp_set_config(p_decoder, 1, &param[0]));
}

static int mixer_setup(void *p_mixer, xaf_format_t *p_format)
{
    int param[6];

    param[0] = XA_MIXER_CONFIG_PARAM_SAMPLE_RATE;
    param[1] = p_format->sample_rate;
    param[2] = XA_MIXER_CONFIG_PARAM_CHANNELS;
    param[3] = p_format->channels;
    param[4] = XA_MIXER_CONFIG_PARAM_PCM_WIDTH;
    param[5] = p_format->pcm_width;

    return(xaf_comp_set_config(p_mixer, 3, &param[0]));
}

static int get_comp_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;
      
    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");
    
    param[0] = XA_MP3DEC_CONFIG_PARAM_NUM_CHANNELS;
    param[2] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[4] = XA_MP3DEC_CONFIG_PARAM_SAMP_FREQ;
    
    
    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;
    
    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];
    
    return 0; 
}

static int check_format(void *p_comp, xaf_format_t *p_format, int *format_match)
{
    xaf_format_t dec_format;
    int ret;
        
    TST_CHK_PTR(p_comp, "check_format");
    TST_CHK_PTR(p_format, "check_format");
    TST_CHK_PTR(format_match, "check_format");
    
    *format_match = 0;
    
    ret = get_comp_config(p_comp, &dec_format);
    if(ret < 0)
        return ret;
    
    if ((dec_format.channels == p_format->channels) &&
        (dec_format.pcm_width == p_format->pcm_width) &&
        (dec_format.sample_rate == p_format->sample_rate))
        {
            *format_match = 1;
        }
        else
        {
            return -1;
        }
    return 0;
}

void fio_quit()
{
    return;
}

#ifdef RTK_HW
#define XA_MAX_ARGS    4
#define XA_MAX_ARG_LEN 128

char pGargv[XA_MAX_ARGS][XA_MAX_ARG_LEN] =
{
    "xa_af_dec_mix_test",
    "-infile:..\\..\\test_inp\\hihat.mp3",
    "-infile:..\\..\\test_inp\\hihat.mp3",
    "-outfile:..\\..\\test_out\\hihat_decmix_out.pcm"
};

int main()
#else // RTK_SIM
int main(int argc, const char **argv)
#endif
{
    void *p_adev = NULL;
    void* p_decoder[MAX_INP_STRMS] = {0};
    void * p_mixer = NULL;

    xaf_comp_status dec_status;
    xaf_format_t mixer_format;
    int dec_info[4];

    void *p_input[MAX_INP_STRMS], *p_output;

    XosThread dec_thread[MAX_INP_STRMS];

    unsigned char dec_stack[MAX_INP_STRMS][STACK_SIZE];
    xf_id_t dec_id[MAX_INP_STRMS];
    int (*dec_setup[MAX_INP_STRMS])(void *p_comp);

    XosThread mixer_thread;
    unsigned char mixer_stack[STACK_SIZE];

    void *dec_thread_args[MAX_INP_STRMS][5];
    void *mixer_thread_args[5];

    const char *ext;
    FILE *fp, *ofp;
    void *dec_inbuf[MAX_INP_STRMS][2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int input_over = 0;
    int i, j;
    int num_strms = 0;
    int format_match = 0;

    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    xaf_comp_type comp_type;
    mem_obj_t* mem_handle;
    int num_comp = NUM_COMP_IN_GRAPH;

#ifdef XAF_PROFILE
    clk_t frmwk_start, frmwk_stop;
    frmwk_cycles = 0;    
    fread_cycles = 0;
    fwrite_cycles = 0; 
    dsp_comps_cycles = 0;
    dec_cycles = 0;
    mix_cycles = 0;        
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

    memset(&mixer_format, 0, sizeof(xaf_format_t));
    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;

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
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'2 Mp3 Decoder + Mixer\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - /'Mixer/' Sample App");

    mixer_format.sample_rate = MIXER_SAMPLE_RATE;
    mixer_format.channels    = MIXER_NUM_CH;
    mixer_format.pcm_width   = MIXER_PCM_WIDTH;

    /* ...check input arguments */
    if (argc < 3 || argc > (MAX_INP_STRMS+2))
    {
        PRINT_USAGE;
        return 0;
    }

    argc--;
    for (i=0; i<argc; i++)
    {
        char *filename_ptr;
        // Open input files
        if(NULL != strstr(argv[i+1], "-infile:"))
        {
            filename_ptr = (char *)&(argv[i+1][8]);
            ext = strrchr(argv[i+1], '.');
            ext++;
            if (!strcmp(ext, "mp3")) {
                dec_id[i]    = "audio-decoder/mp3";
                dec_setup[i] = mp3_setup;
            }
            else {
               FIO_PRINTF(stderr, "Unknown Decoder Extension '%s'\n", ext);
               exit(-1);
            }
            /* ...open file */
            if ((fp = fio_fopen(filename_ptr, "rb")) == NULL)
            {
               FIO_PRINTF(stderr, "Failed to open '%s': %d\n", argv[i+1], errno);
               exit(-1);
            }
            p_input[i] = fp;
            num_strms++;
        }
        // Open output file
        else if(NULL != strstr(argv[i+1], "-outfile:"))
        {
            filename_ptr = (char *)&(argv[i+1][9]);

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
    }

    p_output = ofp;

    if(num_strms == 0 || ofp == NULL)
    {
        PRINT_USAGE;
        exit(-1);
    }
    
    mem_handle = mem_init();


    TST_CHK_API(xaf_adev_open(&p_adev, audio_frmwk_buf_size, audio_comp_buf_size, mem_malloc, mem_free), "xaf_adev_open");
    
    FIO_PRINTF(stdout,"Audio Device Ready\n");

    TST_CHK_API(xaf_comp_create(p_adev, &p_mixer, "mixer", 0, 1, NULL, XAF_MIXER), "xaf_comp_create");
    TST_CHK_API(mixer_setup(p_mixer, &mixer_format), "mixer_setup");

    for (i=0; i<num_strms; i++)
    {
        /* ...create decoder component */
        TST_CHK_API(xaf_comp_create(p_adev, &p_decoder[i], dec_id[i], 2, 0, &dec_inbuf[i][0], XAF_DECODER), "xaf_comp_create");
        TST_CHK_API((dec_setup[i])(p_decoder[i]), "dec_setup");


        TST_CHK_API(xaf_comp_process(p_adev, p_decoder[i], NULL, 0, XAF_START_FLAG), "xaf_comp_process");
        for (j=0; j<2; j++)
        {
            TST_CHK_API(read_input(dec_inbuf[i][j], buf_length, &read_length, p_input[i], XAF_DECODER), "read_input");

            if (read_length)
                TST_CHK_API(xaf_comp_process(p_adev, p_decoder[i], dec_inbuf[i][j], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else
                break;
        }

        while (1)
        {
            TST_CHK_API(xaf_comp_get_status(p_adev, p_decoder[i], &dec_status, &dec_info[0]), "xaf_comp_get_status");

            if (dec_status == XAF_INIT_DONE || dec_status == XAF_EXEC_DONE) break;

            if (dec_status == XAF_NEED_INPUT && !input_over)
            {
                void *p_buf = (void *) dec_info[0];
                int size    = dec_info[1];

                TST_CHK_API(read_input(p_buf, size, &read_length, p_input[i], XAF_DECODER), "read_input");

                if (read_length)
                    TST_CHK_API(xaf_comp_process(p_adev, p_decoder[i], p_buf, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
                else
                    break;
            }
        }

        if (dec_status != XAF_INIT_DONE)
        {
            FIO_PRINTF(stderr, "Failed to init");
            exit(-1);
        }

        TST_CHK_API(check_format(p_decoder[i], &mixer_format, &format_match), "check_format");

        if (format_match)
        {
            TST_CHK_API(xaf_connect(p_decoder[i], p_mixer, 4), "xaf_connect");
        }
        else
        {
            FIO_PRINTF(stderr, "Failed to connect");
            exit(-1);
        }
    }

    TST_CHK_API(xaf_comp_process(p_adev, p_mixer, NULL, 0, XAF_START_FLAG), "xaf_comp_process");
    TST_CHK_API(xaf_comp_get_status(p_adev, p_mixer, &dec_status, &dec_info[0]), "xaf_comp_get_status");

    if (dec_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        exit(-1);
    }

#ifdef XAF_PROFILE
    clk_start();
    
#endif

    comp_type = XAF_DECODER;
    for (i=0; i<num_strms; i++)
    {
        dec_thread_args[i][0] = p_adev;
        dec_thread_args[i][1] = p_decoder[i];
        dec_thread_args[i][2] = p_input[i];
        dec_thread_args[i][3] = p_output;
        dec_thread_args[i][4] = &comp_type; 
        xos_thread_create(&dec_thread[i], 0, comp_process_entry, dec_thread_args[i], "Decoder Thread", dec_stack[i], STACK_SIZE, 7, 0, 0);
    }

    comp_type = XAF_MIXER;
    mixer_thread_args[0] = p_adev;
    mixer_thread_args[1] = p_mixer;
    mixer_thread_args[2] = NULL;
    mixer_thread_args[3] = p_output;
    mixer_thread_args[4] = &comp_type;
    xos_thread_create(&mixer_thread, 0, comp_process_entry, &mixer_thread_args[0], "Mixer Thread", mixer_stack, STACK_SIZE, 7, 0, 0);

    for (i=0; i<num_strms; i++)
    {
        xos_thread_join(&dec_thread[i], &j);
    }
    xos_thread_join(&mixer_thread, &j);
    
#ifdef XAF_PROFILE
    compute_total_frmwrk_cycles();
    clk_stop();
   
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
    for (i=0; i<num_strms; i++)
    {
        xos_thread_delete(&dec_thread[i]);
        TST_CHK_API(xaf_comp_delete(p_decoder[i]), "xaf_comp_delete");
        if (p_input[i]) fio_fclose(p_input[i]);
    }

    xos_thread_delete(&mixer_thread);
    TST_CHK_API(xaf_comp_delete(p_mixer), "xaf_comp_delete");
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");

    mem_exit();

    dsp_comps_cycles = dec_cycles + mix_cycles;

    dsp_mcps = compute_comp_mcps(num_bytes_write, dsp_comps_cycles, mixer_format, &strm_duration);

    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");    

    if (p_output) fio_fclose(p_output);
    
    fio_quit();
    
    return 0;
}

