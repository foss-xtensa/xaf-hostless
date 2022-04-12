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

#include "audio/xa-person-detect-inference-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:infilename.raw\n", argv[0]);\
    FIO_PRINTF(stdout, "\nNote: Output is person detection score\n\n");

#define AUDIO_FRMWK_BUF_SIZE   (256 << 10)
#define AUDIO_COMP_BUF_SIZE    (1024 << 9)

#define NUM_COMP_IN_GRAPH       1

#define PERSON_DETECT_SAMPLE_WIDTH    (16)      //We need to do atleast one set_config. Thus this dummy set_config value.
#define XA_PERSON_DETECT_FRAME_SIZE   (96*96*1) // Full image

#define THREAD_SCRATCH_SIZE         (1024)

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern int tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern int dsp_comps_cycles;
    extern double dsp_mcps;
    extern int pd_inference_cycles;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec22(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_aec23(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_pcm_split(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_mimo_mix(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_wwd(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_opus_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_wwd_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_fe(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_keyword_detection_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_opus_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

static int person_detect_setup(void *p_comp,xaf_format_t comp_format)
{
    int param[][2] = {
        {
            XA_PERSON_DETECT_INFERENCE_CONFIG_PARAM_PCM_WIDTH,
            comp_format.pcm_width,
        },
    };

    return(xaf_comp_set_config(p_comp, ARRAY_SIZE(param), param[0]));
}

void fio_quit()
{
    return;
}

extern FILE *mcps_p_input;

int main_task(int argc, char **argv)
{

    void *p_adev = NULL;
    void *p_input;
    void *p_output;
    int read_length;
    int buf_length = XAF_INBUF_SIZE;
    xf_thread_t person_detect_thread;
    unsigned char person_detect_stack[STACK_SIZE];
    void * p_person_detect  = NULL;
    xaf_comp_status person_detect_status;
    char *filename_ptr;
    void *person_detect_thread_args[NUM_THREAD_ARGS];
    int person_detect_info[4];
    FILE *fp = NULL;
    FILE *ofp = NULL;
    int i;
    void *inp_buff;
    xaf_format_t person_detect_format;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    int num_comp;
    int ret = 0;
    mem_obj_t* mem_handle;
    xaf_comp_type comp_type;
    xf_id_t comp_id;
    int dsp_comp_scratch_size;
    double MCPS_per_image = 0.0;

#ifdef XAF_PROFILE
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dsp_comps_cycles = 0;
    pd_inference_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
#endif

    memset(&person_detect_format, 0, sizeof(xaf_format_t));
    
    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;
    dsp_comp_scratch_size = THREAD_SCRATCH_SIZE;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...start xos */
    board_id = start_rtos();

    /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Person detect \'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Person detect  \' Sample App");

    /* ...check input arguments */
    if (argc != 2)
    {
        PRINT_USAGE;
        return 0;
    }

    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][8]);

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
    if ((ofp = fio_fopen("dummy_out.bin", "wb")) == NULL)
    {
        FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
        exit(-1);
    }
#if 0
    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][9]);
        ext = strrchr(argv[1], '.');
        if(ext!=NULL)
        {
            ext++;
            if (strcmp(ext, "raw"))
            {
                FIO_PRINTF(stderr, "Please provide raw input image of size 96x96x1 without any header and '.raw' extention'\n");
                exit(-1);
            }
        }
        else
        {
            FIO_PRINTF(stderr, "Failed to open outfile\n");
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
#endif

    mcps_p_input = fp;
    p_input = fp;
    p_output = ofp;
    mem_handle = mem_init();
    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.pmem_malloc =  mem_malloc;
    adev_config.pmem_free =  mem_free;
    adev_config.audio_framework_buffer_size =  audio_frmwk_buf_size;
    adev_config.audio_component_buffer_size =  audio_comp_buf_size;
    //setting scratch size for worker thread
    adev_config.worker_thread_scratch_size[0] = dsp_comp_scratch_size;
    TST_CHK_API(xaf_adev_open(&p_adev, &adev_config),  "xaf_adev_open");
    FIO_PRINTF(stdout,"Audio Device Ready\n");

    person_detect_format.pcm_width = PERSON_DETECT_SAMPLE_WIDTH;
    TST_CHK_API_COMP_CREATE(p_adev, &p_person_detect, "post-proc/person_detect_inference", 1, 1, &inp_buff, XAF_POST_PROC, "xaf_comp_create");
    TST_CHK_API(person_detect_setup(p_person_detect,person_detect_format), "person_detect_setup");

    //TST_CHK_API(xaf_connect(p_microspeech_fe, 1, p_inference, 0, 4), "xaf_connect");

    
    TST_CHK_API(xaf_comp_process(p_adev, p_person_detect, NULL, 0, XAF_START_FLAG), "xaf_comp_process");

    TST_CHK_API(xaf_comp_get_status(p_adev, p_person_detect, &person_detect_status, &person_detect_info[0]), "xaf_comp_get_status");
    if (person_detect_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stdout,"Inference Failed to init \n");
        exit(-1);
    }
#ifdef XAF_PROFILE
    clk_start();

#endif
    {
        TST_CHK_API(read_input(inp_buff, buf_length, &read_length, p_input, XAF_POST_PROC), "read_input");

        if (read_length)
            TST_CHK_API(xaf_comp_process(p_adev, p_person_detect, inp_buff, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
        else
        {    
            TST_CHK_API(xaf_comp_process(p_adev, p_person_detect, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
            //break;
        }
    }

    comp_id="post-proc/person_detect_inference";
    comp_type = XAF_POST_PROC;
    person_detect_thread_args[0] = p_adev;
    person_detect_thread_args[1] = p_person_detect;
    person_detect_thread_args[2] = p_input;
    person_detect_thread_args[3] = p_output;
    person_detect_thread_args[4] = &comp_type;
    person_detect_thread_args[5] = (void *)comp_id;
    person_detect_thread_args[6] = (void *)&i;
    ret = __xf_thread_create(&person_detect_thread, comp_process_entry, &person_detect_thread_args[0], "Inference Thread", person_detect_stack, STACK_SIZE, 7);
    if(ret != XOS_OK)
    {
        FIO_PRINTF(stdout,"Failed to create person_detect thread  : %d\n", ret);
        exit(-1);
    }
    ret = __xf_thread_join(&person_detect_thread, NULL);

    if(ret != XOS_OK)
    {
        FIO_PRINTF(stdout,"Decoder thread exit Failed : %d \n", ret);
        exit(-1);
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
    TST_CHK_API(xaf_comp_delete(p_person_detect), "xaf_comp_delete");
    TST_CHK_API(xaf_adev_close(p_adev, XAF_ADEV_NORMAL_CLOSE), "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");
    mem_exit();

    MCPS_per_image = (double)pd_inference_cycles / ((num_bytes_read/XA_PERSON_DETECT_FRAME_SIZE) * 1000000.0);

    printf("Million cycles per image                               :  %lf\n", MCPS_per_image);

    if (fp) fio_fclose(fp);
    if (ofp) fio_fclose(ofp);

    fio_quit();

    /* ...deinitialize tracing facility */
    TRACE_DEINIT();

    return 0;
}


