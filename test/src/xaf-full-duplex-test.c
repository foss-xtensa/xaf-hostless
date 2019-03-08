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
#include "audio/xa_mp3_dec_api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:filename.pcm -infile:filename.mp3 -outfile:filename.mp3 -outfile:filename.pcm\n\n", argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)
#if defined(CORE_HIFI3) || defined(CORE_HIFI4)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7) + (1024 << 5)
#else
#define AUDIO_COMP_BUF_SIZE    (1024 << 7)
#endif
#define NUM_COMP_IN_GRAPH       2

//encoder parameters
#define MP3_ENC_PCM_WIDTH       16
#define MP3_ENC_SAMPLE_RATE     44100
#define MP3_ENC_NUM_CH          2


//decoder parameters
#define MP3_DEC_PCM_WIDTH       16

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
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}

static int mp3_dec_setup(void *p_decoder)
{
    int param[2];

    param[0] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_DEC_PCM_WIDTH;

    return(xaf_comp_set_config(p_decoder, 1, &param[0]));
}

static int mp3_enc_setup(void *p_encoder)
{
    int param[6];

    param[0] = XA_MP3ENC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_ENC_PCM_WIDTH;
    param[2] = XA_MP3ENC_CONFIG_PARAM_SAMP_FREQ;
    param[3] = MP3_ENC_SAMPLE_RATE;
    param[4] = XA_MP3ENC_CONFIG_PARAM_NUM_CHANNELS;
    param[5] = MP3_ENC_NUM_CH;    

    return(xaf_comp_set_config(p_encoder, 3, &param[0]));
}

static int get_mp3_dec_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;
    
    
    TST_CHK_PTR(p_comp, "get_mp3_dec_config");
    TST_CHK_PTR(comp_format, "get_mp3_dec_config");
    
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

static int get_mp3_enc_config(void *p_comp, xaf_format_t *comp_format)
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

void fio_quit()
{
    return;
}

#ifdef RTK_HW
#define XA_MAX_ARGS    5
#define XA_MAX_ARG_LEN 128

char pGargv[XA_MAX_ARGS][XA_MAX_ARG_LEN] =
{
    "xa_af_full_duplex_test",
    "-infile:..\\..\\test_inp\\hihat.pcm",
    "-infile:..\\..\\test_inp\\hihat.mp3",
    "-outfile:..\\..\\test_out\\hihat_full_duplex_enc_out.mp3",
    "-outfile:..\\..\\test_out\\hihat_full_duplex_dec_out.pcm"
};

int main()
#else // RTK_SIM
int main(int argc, const char **argv)
#endif
{
    void *p_adev = NULL;
    void *p_comp[NUM_COMP_IN_GRAPH] = {0,0};
    void *p_input[NUM_COMP_IN_GRAPH];
    void *p_output[NUM_COMP_IN_GRAPH];
    XosThread comp_thread[NUM_COMP_IN_GRAPH];
    unsigned char comp_stack[NUM_COMP_IN_GRAPH][STACK_SIZE];
    xaf_comp_status comp_status;
    int comp_info[4];
    char *filename_ptr;
    FILE *fp, *ofp;    
    void *thread_args[NUM_COMP_IN_GRAPH][5];
    void *comp_inbuf[NUM_COMP_IN_GRAPH][2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int input_over = 0;
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
    xaf_format_t dec_format;
    double enc_strm_duration;
    double dec_strm_duration;

#ifdef XAF_PROFILE
    clk_t frmwk_start, frmwk_stop;
    frmwk_cycles = 0;    
    fread_cycles = 0;
    fwrite_cycles = 0;    
    dec_cycles = 0;
    dsp_comps_cycles = 0;
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

    memset(&dec_format, 0, sizeof(xaf_format_t));
    memset(&comp_format[0], 0, sizeof(xaf_format_t)*NUM_COMP_IN_GRAPH);

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
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Full duplex\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Full duplex\' Sample App");

    /* ...check input arguments */
    if (argc != 5)
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
            ext++;
            if (!strcmp(ext, "pcm")) 
            {
                comp_id[k]    = "audio-encoder/mp3";
                comp_setup[k] = mp3_enc_setup;
                comp_type[k]  = XAF_ENCODER;
                get_comp_config[k] = get_mp3_enc_config;

            }
            else if(!strcmp(ext, "mp3"))
            {
                comp_id[k]    = "audio-decoder/mp3";
                comp_setup[k] = mp3_dec_setup;
                comp_type[k]  = XAF_DECODER;
                get_comp_config[k] = get_mp3_dec_config;
            }
            else 
            {
                FIO_PRINTF(stderr, "Unknown Decoder Extension '%s'\n", ext);
                exit(-1);
            }
    
            /* ...open file */
            if ((fp = fio_fopen(filename_ptr, "rb")) == NULL)
            {
            FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
            exit(-1);
            }
            
            p_input[k]  = fp;      
        }
        else
        {
            PRINT_USAGE;
            return 0;
        }    
        
        /* ...out file opening */

        if(NULL != strstr(argv[k+3], "-outfile:"))
        {
            filename_ptr = (char *)&(argv[k+3][9]);
            if ((ofp = fio_fopen(filename_ptr, "wb")) == NULL)
            {
            FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
            exit(-1);
            }
            p_output[k] = ofp;
        }
        else
        {
            PRINT_USAGE;
            return 0;
        }
    }

    mem_handle = mem_init();

    TST_CHK_API(xaf_adev_open(&p_adev, audio_frmwk_buf_size, audio_comp_buf_size, mem_malloc, mem_free), "xaf_adev_open");
    
    FIO_PRINTF(stdout, "Audio Device Ready\n");
    
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        /* ...create encoder component */
        TST_CHK_API(xaf_comp_create(p_adev, &p_comp[k], comp_id[k], 2, 1, &comp_inbuf[k][0], comp_type[k]),"xaf_comp_create");
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
                break;
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
                    break;
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

        /* ...init done, begin execution thread */
        xos_thread_create(&comp_thread[k], 0, comp_process_entry, thread_args[k], "Comp Thread", comp_stack[k], STACK_SIZE, 7, 0, 0);
    }

    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {
        xos_thread_join(&comp_thread[k], &i);
    }
    
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
    for(k = 0; k < NUM_COMP_IN_GRAPH; k++)
    {    
        xos_thread_delete(&comp_thread[k]);
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

    dsp_mcps = compute_comp_mcps(num_bytes_read, enc_cycles, comp_format[0], &enc_strm_duration);
    dsp_mcps += compute_comp_mcps(num_bytes_write, dec_cycles, comp_format[1], &dec_strm_duration);

    if(enc_strm_duration > dec_strm_duration)
       strm_duration = enc_strm_duration;
    else
        strm_duration = dec_strm_duration;

    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");
    
    fio_quit();
        
    return 0;
}

