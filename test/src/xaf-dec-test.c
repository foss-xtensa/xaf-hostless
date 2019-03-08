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
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:filename.mp3 -outfile:filename.pcm\n\n", argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7)

#define NUM_COMP_IN_GRAPH       1

//component parameters
#define MP3_DEC_PCM_WIDTH       16

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, dec_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
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

void fio_quit()
{
    return;
}

#ifdef RTK_HW
#define XA_MAX_ARGS    3
#define XA_MAX_ARG_LEN 128
char pGargv[XA_MAX_ARGS][XA_MAX_ARG_LEN] =
{
    "xaf-dec-test",
    "-infile:..\\..\\test_inp\\hihat.mp3",
    "-outfile:..\\..\\test_out\\hihat_dec_out.pcm"
};

int main()
#else // RTK_SIM
int main(int argc, const char **argv)
#endif
{
    void *p_adev = NULL;
    void *p_decoder = NULL;    
    void *p_input, *p_output;  
    XosThread dec_thread;
    unsigned char dec_stack[STACK_SIZE];
    xaf_comp_status dec_status;
    int dec_info[4];
    char *filename_ptr;
    void *dec_thread_args[5];
    FILE *fp, *ofp;
    void *dec_inbuf[2];
    int buf_length = XAF_INBUF_SIZE;
    int read_length;
    int input_over = 0;
    int i;    
    xaf_comp_type comp_type;
    xf_id_t dec_id;
    int (*dec_setup)(void *p_comp);    
    const char *ext;

    xaf_format_t dec_format;
    int num_comp;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    mem_obj_t* mem_handle;


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
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Mp3 Decoder\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Mp3 Decoder\' Sample App");

    /* ...check input arguments */
    if (argc != 3)
    {
        PRINT_USAGE;
        return 0;
    }
    
    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][8]);
        ext = strrchr(argv[1], '.');
        ext++;
        if (!strcmp(ext, "mp3")) {
            dec_id    = "audio-decoder/mp3";
            dec_setup = mp3_setup;
        }
        else {
            FIO_PRINTF(stderr, "Unknown Decoder Extension '%s'\n", ext);
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

    TST_CHK_API(xaf_adev_open(&p_adev, audio_frmwk_buf_size, audio_comp_buf_size, mem_malloc, mem_free), "xaf_adev_open");
    
    FIO_PRINTF(stdout, "Audio Device Ready\n");

    /* ...create decoder component */
    comp_type = XAF_DECODER;
    TST_CHK_API(xaf_comp_create(p_adev, &p_decoder, dec_id, 2, 1, &dec_inbuf[0], comp_type),"xaf_comp_create");
    TST_CHK_API(dec_setup(p_decoder), "dec_setup");

    /* ...start decoder component */            
    TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_START_FLAG), "xaf_comp_process");

    /* ...feed input to decoder component */
    for (i=0; i<2; i++)
    {
        TST_CHK_API(read_input(dec_inbuf[i], buf_length, &read_length, p_input, comp_type), "read_input");

        if (read_length) 
            TST_CHK_API(xaf_comp_process(p_adev, p_decoder, dec_inbuf[i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
        else 
            break;
    }

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
                break;
        }
    }

    if (dec_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        exit(-1);
    }

    TST_CHK_API(get_comp_config(p_decoder, &dec_format), "get_comp_config");
    
#ifdef XAF_PROFILE
    clk_start();
    
#endif

    dec_thread_args[0]= p_adev;
    dec_thread_args[1]= p_decoder;
    dec_thread_args[2]= p_input;
    dec_thread_args[3]= p_output;
    dec_thread_args[4]= &comp_type;
    
    /* ...init done, begin execution thread */
    xos_thread_create(&dec_thread, 0, comp_process_entry, &dec_thread_args[0], "Decoder Thread", dec_stack, STACK_SIZE, 7, 0, 0);

    xos_thread_join(&dec_thread, &i); 
    
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
    xos_thread_delete(&dec_thread); 
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

    return 0;
}
