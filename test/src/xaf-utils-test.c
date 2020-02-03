/*******************************************************************************
* Copyright (c) 2015-2020 Cadence Design Systems, Inc.
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

#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define TICK_CYCLES       (10000)
#define CLK_FREQ      (100000000)

/* global data */
int g_continue_wait = 0;
int g_force_input_over[MAX_NUM_COMP_IN_GRAPH];
int (*gpcomp_connect)(int comp_id_src, int port_src, int comp_id_dest, int port_dest, int create_delete_flag);
int (*gpcomp_disconnect)(int comp_id_src, int port_src, int comp_id_dest, int port_dest, int create_delete_flag);

#if 1 //TENA-2376, TENA-2434
int g_num_comps_in_graph = 0;
xf_thread_t *g_comp_thread;
#endif

/*... Global File pointer required for correct MCPS calculation */
FILE *mcps_p_input = NULL;
FILE *mcps_p_output = NULL; 

extern unsigned int num_bytes_read, num_bytes_write;
int audio_frmwk_buf_size;
int audio_comp_buf_size;
extern double strm_duration;

#if 1 //TENA-2376, TENA-2434
int abort_blocked_threads()
{
    int i;

    /* Ignore if not enabled in the testbench */
    if ( g_num_comps_in_graph == 0 )
        return -1;

    for( i=0; i<g_num_comps_in_graph; i++ )
    {
        if ( __xf_thread_get_state(&g_comp_thread[i]) == XF_THREAD_STATE_BLOCKED )
        {
            fprintf(stderr, "Aborting thread: %d\n", i);
            __xf_thread_cancel( (xf_thread_t *) &g_comp_thread[i] );
        } 
    }
    return 0;
}
#endif

#ifdef XAF_PROFILE
    long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    long long dsp_comps_cycles, enc_cycles, dec_cycles, mix_cycles, pcm_gain_cycles, src_cycles,capturer_cycles, renderer_cycles,aac_dec_cycles;
    long long aec22_cycles, aec23_cycles, pcm_split_cycles, pcm_mix_cycles;
    long long wwd_cycles, hbuf_cycles;
    double dsp_mcps;
#endif

_XA_API_ERR_MAP error_map_table_api[XA_NUM_API_ERRS]=
{
    {(int)XAF_RTOS_ERROR,       "rtos error"},
    {(int)XAF_INVALID_VALUE,    "invalid value"},
    {(int)XAF_ROUTING_ERROR,    "routing error"},
    {(int)XAF_PTR_ERROR,        "invalid pointer"},
    {(int)XAF_API_ERR,          "API error"},
    {(int)XAF_STATUS_TIMEOUT,   "Message queue Timeout"},
};

// Set cache attribute to Write Back No Allocate when the last argument is -wbna
void set_wbna(int *argc, char **argv)
{
    if ( *argc > 1 && !strcmp(argv[*argc-1], "-wbna") ) {
#ifdef __XCC__
        extern char _memmap_cacheattr_wbna_trapnull;

        xthal_set_cacheattr((unsigned)&_memmap_cacheattr_wbna_trapnull);
#endif
        (*argc)--;
    }
}

int print_verinfo(pUWORD8 ver_info[],pUWORD8 app_name)
{
    TST_CHK_PTR(ver_info[0], "print_verinfo");
    TST_CHK_PTR(ver_info[1], "print_verinfo");
    TST_CHK_PTR(ver_info[2], "print_verinfo");

    FIO_PRINTF(stdout, "****************************************************************\n");
    FIO_PRINTF(stdout, "Cadence Audio Framework (Hostless) : %s \n",app_name);
    FIO_PRINTF(stdout, "Build: %s, RTOS: %s, On: %s %s\n", BUILD_STRING, BUILD_RTOS, __DATE__, __TIME__);
    FIO_PRINTF(stdout, "Copyright (c) 2015-2020 Cadence Design Systems, Inc.\n");
    FIO_PRINTF(stdout, "Lib Name        : %s\n", ver_info[0]);
    FIO_PRINTF(stdout, "Lib Version     : %s\n", ver_info[1]);
    FIO_PRINTF(stdout, "API Version     : %s\n", ver_info[2]);
    FIO_PRINTF(stdout, "****************************************************************\n");

    return 0;
}

int consume_output(void *p_buf, int buf_length, void *p_output, xaf_comp_type comp_type)
{
    TST_CHK_PTR(p_buf, "consume_output");
    TST_CHK_PTR(p_output, "consume_output");

    FILE *fp = p_output;
    fio_fwrite(p_buf, 1, buf_length, fp);

    if((comp_type != XAF_ENCODER) && ((mcps_p_output == fp) || (mcps_p_output == NULL)))
        num_bytes_write += buf_length;

    return 0;
}

/* ... align pointer to 8 bytes */
#define XA_ALIGN_8BYTES(ptr)                (((unsigned int)(ptr) + 7) & ~7)
#define COMP_MAX_IO_PORTS                   8

int consume_probe_output(void *p_buf, int buf_length, int cid)
{
    TST_CHK_PTR(p_buf, "consume_probe_output");

    unsigned int  probe_length;
    unsigned int bytes_consumed = 0;
    void *probe_buf = p_buf;
    FILE *fp; 

    char probefname[64];
    unsigned int port;
    do
    {
        probe_buf    = (void *) XA_ALIGN_8BYTES(probe_buf);
        port         = *(unsigned int *) probe_buf;
        probe_buf   += sizeof(unsigned int);
        probe_length = *(unsigned int *) probe_buf;
        probe_buf   += sizeof(unsigned int);

        sprintf(probefname,"comp%d_port%d.bin", cid, port);

        if(port >= COMP_MAX_IO_PORTS) 
        {
            fprintf(stderr,"consume_probe: invalid port %x\n", port);
            return -1;
        }
        fp = fopen(probefname, "ab");
        if (probe_length)
        {
            fio_fwrite(probe_buf, 1, probe_length, fp);
            probe_buf += probe_length; 
        }
        bytes_consumed = probe_buf - p_buf;
        fclose(fp);

    } while(bytes_consumed < buf_length);

    return (!(bytes_consumed == buf_length));
}

int read_input(void *p_buf, int buf_length, int *read_length, void *p_input, xaf_comp_type comp_type)
{
    TST_CHK_PTR(p_buf, "read_input");
    TST_CHK_PTR(read_length, "read_input");
    TST_CHK_PTR(p_input, "read_input");
    FILE *fp = p_input;
    *read_length = fio_fread(p_buf, 1, buf_length, fp);

    if((comp_type == XAF_ENCODER) || (mcps_p_input == fp))
        num_bytes_read += *read_length;

    return 0;
}

#if 0 /* unused function */
static int get_per_comp_size(int size, int num)
{
    int comp_size;

    comp_size = size/num;

    if((comp_size*num) < size)
        comp_size += 1;

    return(comp_size);
}
#endif

#if defined(HAVE_XOS)
int init_rtos(int argc, char **argv, int (*main_task)(int argc, char **argv))
{
    return main_task(argc, argv);
}

unsigned short start_rtos(void)
{
	volatile unsigned short  board_id = 0;

    xos_set_clock_freq(CLK_FREQ);

#ifdef __TOOLS_RF2__
    xos_start("main", 7, 0);
#else   // #ifdef __TOOLS_RF2__
    xos_start_main("main", 7, 0);
#endif  // #ifdef __TOOLS_RF2__

#if XCHAL_NUM_TIMERS > 0
    xos_start_system_timer(0, TICK_CYCLES);
#endif

    return board_id;
}
#endif
#ifdef HAVE_FREERTOS

#define MAIN_STACK_SIZE 65536

struct args {
    int argc;
    char **argv;
    int (*main_task)(int argc, char **argv);
};

static void *main_task_wrapper(void *arg)
{
    struct args *args = arg;
    exit(args->main_task(args->argc, args->argv));
}

int init_rtos(int argc, char **argv, int (*main_task)(int argc, char **argv))
{
    struct args args = {
        .argc = argc,
        .argv = argv,
        .main_task = main_task,
    };
    xf_thread_t thread;

    __xf_thread_create(&thread, main_task_wrapper, &args, "main task",
                       NULL, MAIN_STACK_SIZE , 7);
    vTaskStartScheduler();
    return 1;
}
unsigned short start_rtos(void)
{
    return 0;
}
void vApplicationTickHook( void )
{
}
void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
}
#endif

int main(int argc, char **argv)
{
    return init_rtos(argc, argv, main_task);
}

static int _comp_process_entry(void *arg)
{
    void *p_adev, *p_comp;
    void *p_input, *p_output;
    xaf_comp_status comp_status;
    int comp_info[4];
    int input_over, output_over, read_length;
    void * (*arg_arr)[10];
    xaf_comp_type comp_type;
    int error;
#ifdef XAF_PROFILE
    clk_t fread_start, fread_stop, fwrite_start, fwrite_stop;
#endif
    TST_CHK_PTR(arg, "comp_process_entry");

    arg_arr = arg;
    p_adev = (*arg_arr)[0];
    p_comp = (*arg_arr)[1];
    p_input = (*arg_arr)[2];
    p_output = (*arg_arr)[3];
    comp_type = *(xaf_comp_type *)(*arg_arr)[4];
    //char *pcid = (char *)(*arg_arr)[5];
    int cid = *(int *)(*arg_arr)[6];

    /* workaround to run some test usecases which are not passing cid properly */
    if ( (cid >= MAX_NUM_COMP_IN_GRAPH) || (cid < 0) )
    {
        cid = 0;
    }

    g_force_input_over[cid] = 0;

    TST_CHK_PTR(p_adev, "comp_process_entry");
    TST_CHK_PTR(p_comp, "comp_process_entry");

    input_over = output_over = 0;

    TST_CHK_API(xaf_comp_process(NULL, p_comp, NULL, 0, XAF_EXEC_FLAG), "xaf_comp_process");

    while (1)
    {
        if ((p_input == NULL) && (output_over))
        {
            break;
        }

        error = xaf_comp_get_status(NULL, p_comp, &comp_status, &comp_info[0]);
        if ((error == XAF_STATUS_TIMEOUT) && (g_continue_wait))
        {
            FIO_PRINTF(stderr, ".");
            continue;    // If resume is expected, then wait. Else timeout.
        }

        TST_CHK_API(error, "xaf_comp_get_status");

        if (comp_status == XAF_EXEC_DONE) break;

        if (comp_status == XAF_NEED_INPUT && !input_over)
        {
            read_length = 0;
            if (g_force_input_over[cid] != 1)
            {

                void *p_buf = (void *)comp_info[0];
                int size = comp_info[1];
                static int frame_count = 0;

#ifdef XAF_PROFILE
                fread_start = clk_read_start(CLK_SELN_THREAD);
#endif

                TST_CHK_API(read_input(p_buf, size, &read_length, p_input, comp_type), "read_input");
                FIO_PRINTF(stdout, " frame_count:%d\n", ++frame_count);

#ifdef XAF_PROFILE
                fread_stop = clk_read_stop(CLK_SELN_THREAD);
                fread_cycles += clk_diff(fread_stop, fread_start);
#endif
                
                /* Set g_force_input_over[cid] to 2 for natural input over */
                if ( read_length == 0 )
                {
                    g_force_input_over[cid] = 2;
                }

            }
            if (read_length)
                TST_CHK_API(xaf_comp_process(NULL, p_comp, (void *)comp_info[0], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else
            {
                TST_CHK_API(xaf_comp_process(NULL, p_comp, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
                input_over = 1;
            }
        }

        if (comp_status == XAF_OUTPUT_READY)
        {
            void *p_buf = (void *)comp_info[0];
            int size = comp_info[1];

            if (size == 0)
                output_over = 1;
            else
            {
#ifdef XAF_PROFILE
                fwrite_start = clk_read_start(CLK_SELN_THREAD);
#endif

                TST_CHK_API(consume_output(p_buf, size, p_output, comp_type), "consume_output");

#ifdef XAF_PROFILE
                fwrite_stop = clk_read_stop(CLK_SELN_THREAD);
                fwrite_cycles += clk_diff(fwrite_stop, fwrite_start);
#endif

                TST_CHK_API(xaf_comp_process(NULL, p_comp, (void *)comp_info[0], comp_info[1], XAF_NEED_OUTPUT_FLAG), "xaf_comp_process");
            }
        }

        if (comp_status == XAF_PROBE_READY)
        {
            void *p_buf = (void *)comp_info[0];
            int size = comp_info[1];

            if (size)
            {
                TST_CHK_API(consume_probe_output(p_buf, size, cid), "consume_probe_output");

                TST_CHK_API(xaf_comp_process(NULL, p_comp, (void *)comp_info[0], comp_info[1], XAF_NEED_PROBE_FLAG), "xaf_comp_process");
            }
            else
            {
                /* ...tbd - improve break condition */
                if ((p_input == NULL) && (p_output == NULL)) break;
            }
        }
    }

    return 0;
}

void *comp_process_entry(void *arg)
{
    return (void *)_comp_process_entry(arg);
}

double compute_comp_mcps(unsigned int num_bytes, int comp_cycles, xaf_format_t comp_format, double *strm_duration)
{
    double mcps;
    unsigned int num_samples;
    int pcm_width_in_bytes;

    *strm_duration = 0.0;

    switch(comp_format.pcm_width)
    {
        case 8:
        case 16:
        case 24:
        case 32:
            break;

        default:
            FIO_PRINTF(stdout,"Insufficient data to compute MCPS...\n");
            return 0;
    }

    switch(comp_format.sample_rate)
    {
        case 4000:
        case 8000:
        case 11025:
        case 12000:
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
        case 64000:
        case 88200:
        case 96000:
        case 128000:
        case 176400:
        case 192000:
            break;

        default:
            FIO_PRINTF(stdout,"Insufficient data to compute MCPS...\n");
            return 0;
    }

    if(comp_format.channels > 32)
    {
        FIO_PRINTF(stdout,"Insufficient data to compute MCPS...\n");
        return 0;
    }

    if( comp_cycles < 0 )
    {
        FIO_PRINTF(stdout,"Insufficient data to compute MCPS...\n");
        return 0;
    }

    pcm_width_in_bytes = (comp_format.pcm_width)/8;
    num_samples = num_bytes/pcm_width_in_bytes;
    *strm_duration = (double)num_samples/((comp_format.sample_rate)*(comp_format.channels));

    mcps = ((double)comp_cycles/((*strm_duration)*1000000.0));

    FIO_PRINTF(stdout, "PCM Width                                    :  %8d\n", comp_format.pcm_width);
    FIO_PRINTF(stdout, "Sample Rate                                  :  %8d\n", comp_format.sample_rate);
    FIO_PRINTF(stdout, "No of channels                               :  %8d\n", comp_format.channels);
    FIO_PRINTF(stdout, "Stream duration (seconds)                    :  %8f\n\n", *strm_duration);

    return mcps;
}


int print_mem_mcps_info(mem_obj_t* mem_handle, int num_comp)
{
    int tot_dev_mem_size, tot_comp_mem_size, mem_for_comp, tot_size;
    double mcps,read_write_mcps;

    /* ...printing memory info*/

    tot_dev_mem_size = mem_get_alloc_size(mem_handle, XAF_MEM_ID_DEV);
    tot_comp_mem_size = mem_get_alloc_size(mem_handle, XAF_MEM_ID_COMP);
    tot_size = tot_dev_mem_size + tot_comp_mem_size;
    mem_for_comp = (audio_frmwk_buf_size + audio_comp_buf_size - XAF_SHMEM_STRUCT_SIZE);
    /* XAF_SHMEM_STRUCT_SIZE is used internally by the framework. Computed as sizeof(xf_shmem_data_t)-XF_CFG_REMOTE_IPC_POOL_SIZE*/

    /* ...printing mcps info*/

#ifdef XAF_PROFILE
    if(strm_duration)
    {
        frmwk_cycles =  frmwk_cycles - (dsp_comps_cycles) - (fread_cycles + fwrite_cycles);
        read_write_mcps = ((double)(fread_cycles + fwrite_cycles)/(strm_duration*1000000.0));
        mcps = ((double)tot_cycles/(strm_duration*1000000.0));
        FIO_PRINTF(stdout,"Total MCPS                                   :  %f\n",mcps);

        FIO_PRINTF(stdout,"DSP component MCPS                           :  %f\n",dsp_mcps);

        FIO_PRINTF(stdout,"File Read/Write MCPS                         :  %f\n",read_write_mcps);
        mcps = ((double)frmwk_cycles/(strm_duration*1000000.0));
        //mcps = mcps - dsp_mcps;
        FIO_PRINTF(stdout,"Framework MCPS                               :  %f\n\n",mcps);
    }
#endif

    return 0;
}

void sort_runtime_action_commands(cmd_array_t *command_array)  //Implementation of Insertion Sort Algorithm(incremental sort)
{
    int size = command_array->size;
    int i, j, key;
    cmd_info_t **cmdlist = command_array->commands; 

    cmd_info_t *key_command = NULL;

    for (i = 1; i < size; i++)
    {
        key = cmdlist[i]->time;
        key_command = cmdlist[i];
        j = i-1;
 
        while (j >= 0 && cmdlist[j]->time > key)
        {
            cmdlist[j+1] = cmdlist[j];
            j = j-1;
        }
        cmdlist[j+1] = key_command;
    }
    command_array->commands = cmdlist;
}

int parse_runtime_params(void **runtime_params, int argc, char **argv, int num_comp ) 
{
    cmd_array_t *cmd_array;

    cmd_array = (cmd_array_t *)malloc(sizeof(cmd_array_t));
    cmd_array->commands = (cmd_info_t **)malloc(RUNTIME_MAX_COMMANDS * 2 * sizeof(cmd_info_t *));
    cmd_array->size = 0;

    int i, x;
    char *ptr = NULL;
    char *token;
    char *pr_string;
    int cmd_params[6];
    cmd_info_t *cmd1, *cmd2;
    int probe_flag;
    int reconnect_flag;

    for (i = 0; i < (argc); i++)
    {
        cmd_params[0] = cmd_params[1] = cmd_params[2] = cmd_params[3] = -1;
        probe_flag = 0;
        if ((NULL != strstr(argv[i + 1], "-pr:")) || (NULL != strstr(argv[i + 1], "-probe:")))
        {
            if (NULL != strstr(argv[i + 1], "-probe:")) probe_flag = 1;

            if (probe_flag)
                pr_string = (char *)&(argv[i + 1][7]);
            else
                pr_string = (char *)&(argv[i + 1][4]);

            x = 0;
            for (token = strtok_r(pr_string, ",", &ptr); token != NULL; token = strtok_r(NULL, ",", &ptr))
            {
                cmd_params[x] = atoi(token);
                x++;

                if (x == 4) break;
            }

            if (probe_flag && (x < 3))
            {
                fprintf(stderr, "Runtime-Params-Parse: Invalid probe command. Minimum 3 values required.\n");
                return -1;
            }
            else if (!probe_flag && (x < 4))
            {
                fprintf(stderr, "Runtime-Params-Parse: Invalid pause/resume command. Minimum 4 values required.\n");
                return -1;
            }
            if (cmd_params[0] >= num_comp)
            {
                fprintf(stderr, "Runtime-Params-Parse: Invalid component ID. Allowed range: 0-%d\n", num_comp - 1);
                return -1;
            }

            cmd1 = (cmd_info_t *)malloc(sizeof(cmd_info_t));
            cmd2 = (cmd_info_t *)malloc(sizeof(cmd_info_t));

            cmd1->component_id = cmd_params[0];
            cmd2->component_id = cmd_params[0];

            if (probe_flag)
            {
                cmd1->action = PROBE_START;
                cmd2->action = PROBE_STOP;
                cmd1->time = cmd_params[1];
                cmd2->time = cmd_params[2];
            }
            else
            {
                cmd1->action = PAUSE;
                cmd2->action = RESUME;
                cmd1->port = cmd_params[1];
                cmd2->port = cmd_params[1];
                cmd1->time = cmd_params[2];
                cmd2->time = cmd_params[3];
                if (cmd2->time <= cmd1->time)
                {
                    fprintf(stderr, "Runtime-Params-Parse: Resume time must be > pause time.\n");
                    return -1;
                }
                g_continue_wait = 1;
            }

            cmd_array->commands[cmd_array->size] = cmd1;
            cmd_array->commands[cmd_array->size + 1] = cmd2;
            cmd_array->size = cmd_array->size + 2;

            if (cmd_array->size >= (RUNTIME_MAX_COMMANDS * 2))
            {
                fprintf(stderr, "Runtime-Params-Parse: Max number of runtime commands exceeded.\n");
                return -1;
            }
        }
        if ((NULL != strstr(argv[i + 1], "-D:")) || (NULL != strstr(argv[i + 1], "-C:")))
        {
            reconnect_flag = 0;
            if (NULL != strstr(argv[i + 1], "-C:"))
            {
                reconnect_flag = 1;
            }

            pr_string = (char *)&(argv[i + 1][3]);

            x = 0;
            for (token = strtok_r(pr_string, ",", &ptr); token != NULL; token = strtok_r(NULL, ",", &ptr))
            {
                cmd_params[x] = atoi(token);
                x++;

                if (x == 6) break;
            }

            if (x < 6)
            {
                fprintf(stderr, "Runtime-Params-Parse: Invalid disconnect/connect command.\n");
                return -1;
            }

            cmd1 = (cmd_info_t *)malloc(sizeof(cmd_info_t));

            cmd1->component_id = cmd_params[0];
            cmd1->port = cmd_params[1];
            cmd1->component_dest_id = cmd_params[2];
            cmd1->port_dest = cmd_params[3];
            cmd1->time = cmd_params[4];
            cmd1->comp_create_delete_flag = cmd_params[5];
            if (reconnect_flag)
            {
                cmd1->action = CONNECT;
            }
            else
            {
                cmd1->action = DISCONNECT;
            }
            g_continue_wait = 1;

            cmd_array->commands[cmd_array->size] = cmd1;
            cmd_array->size = cmd_array->size + 1;

            if (cmd_array->size >= (RUNTIME_MAX_COMMANDS * 2))
            {
                fprintf(stderr, "Runtime-Params-Parse: Max number of runtime commands exceeded.\n");
                return -1;
            }
        }
    }
    *runtime_params = (void *)cmd_array;

    return 0;
}


int execute_runtime_actions(void *command_array, 
                                void *p_adev, 
                                void **comp_ptr,
                                int *comp_nbufs,
                                void **comp_threads,
                                int num_threads, 
                                void *comp_thread_args[],
                                int num_thread_args,
                                unsigned char *comp_stack)
{
    int x, k;
    int curr_time = 0;
    int thread_exit_count;
    cmd_info_t *cmd;
    cmd_array_t *cmd_array;
    int thread_state;

    cmd_array = (cmd_array_t *)command_array;
    sort_runtime_action_commands(cmd_array);

    for (x = 0; x < cmd_array->size; x++)
    {
        cmd = cmd_array->commands[x];

        if (cmd->time < 0)    // Command with -ve time value is ignored.
            continue;

        __xf_thread_sleep_msec(cmd->time - curr_time);        
        
        curr_time += (cmd->time - curr_time);

        /* Don't issue runtime commands if all threads are exited */
        thread_exit_count = 0;
        for (k = 0; k < num_threads; k++)
        {
            thread_state = __xf_thread_get_state(comp_threads[k]);
            if ((thread_state == XF_THREAD_STATE_EXITED) || (thread_state == XF_THREAD_STATE_INVALID))
            {
                thread_exit_count++;
            }
        }
        if (thread_exit_count == num_threads)
        {
            fprintf(stdout, "\nRuntime Action: All threads exited. Further commands will be skipped\n\n");
            break;
        }

        switch (cmd->action)
        {
        case PAUSE:
            TST_CHK_API(xaf_pause(comp_ptr[cmd->component_id], cmd->port), "xaf_pause");
            fprintf(stdout, "Runtime Action: Component %d, Port %d : PAUSE command issued\n", cmd->component_id, cmd->port);
            break;

        case RESUME:
            TST_CHK_API(xaf_resume(comp_ptr[cmd->component_id], cmd->port), "xaf_resume");
            fprintf(stdout, "Runtime Action: Component %d, Port %d : RESUME command issued\n", cmd->component_id, cmd->port);
            break;

        case PROBE_START:
            TST_CHK_API(xaf_probe_start(comp_ptr[cmd->component_id]), "xaf_probe_start");
            fprintf(stdout, "Runtime Action: Component %d : PROBE-START command issued\n", cmd->component_id);
            if (comp_nbufs[cmd->component_id] == 0)
            {
                __xf_thread_create(comp_threads[cmd->component_id], comp_process_entry, &comp_thread_args[num_thread_args * cmd->component_id], "Probe Thread", &comp_stack[STACK_SIZE * cmd->component_id], STACK_SIZE, 7);
            }
            break;

        case PROBE_STOP:
            TST_CHK_API(xaf_probe_stop(comp_ptr[cmd->component_id]), "xaf_probe_stop");
            fprintf(stdout, "Runtime Action: Component %d : PROBE-STOP command issued\n", cmd->component_id);
            if (comp_nbufs[cmd->component_id] == 0)
            {
                __xf_thread_join(comp_threads[cmd->component_id], NULL);
                fprintf(stdout, "Probe thread for component %d joined\n", cmd->component_id);
                __xf_thread_destroy(comp_threads[cmd->component_id]);
            }
            break;

        case DISCONNECT:
            if ( cmd->comp_create_delete_flag == 0 )
            {
                cmd_info_t *cmd_next;
                int re_connect_flag = 0;
                
                for (k = x; k < cmd_array->size; k++)
                {
                    cmd_next = cmd_array->commands[k];
                    
                    if ( ( cmd_next->action == CONNECT ) && ( cmd->component_id == cmd_next->component_id) && ( cmd_next->comp_create_delete_flag == 0) )
                    {
                        re_connect_flag = 1;
                    }
                }
                if ( re_connect_flag == 0 )                
                {
                    fprintf(stderr, "INFO: DISCONNECT is initiated with component and thread deletion since reconnect is not issued for this component \n");
                    cmd->comp_create_delete_flag = 1;                    
                }
            }
            
            if (gpcomp_disconnect(cmd->component_id, cmd->port, cmd->component_dest_id, cmd->port_dest, cmd->comp_create_delete_flag))
            {
                fprintf(stderr, "ERROR: DISCONNECT cid:%d port:%d\n", cmd->component_id, cmd->port);
            }
            break;

        case CONNECT:
            if (gpcomp_connect(cmd->component_id, cmd->port, cmd->component_dest_id, cmd->port_dest, cmd->comp_create_delete_flag))
            {
                fprintf(stderr, "ERROR: CONNECT cid:%d port:%d\n", cmd->component_id, cmd->port);
            }
            break;

        default:
            fprintf(stdout, "Runtime Action: Unknown command. Ignored.\n");
        }
    }

    for (x = 0; x < cmd_array->size; x++)
    {
        free(cmd_array->commands[x]);
    }
    free(cmd_array->commands);
    free(cmd_array);

    return 0;
}
