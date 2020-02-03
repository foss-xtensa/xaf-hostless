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
#ifndef __XAF_CLK_TEST_H__
#define __XAF_CLK_TEST_H__

#ifdef XAF_PROFILE
/* Common clock structures and functions */
typedef enum 
{
    CLK_SELN_WALL,   // Total time elapsed
    CLK_SELN_THREAD  // Time elapsed in this thread
} clk_seln_t;
typedef long long clk_t;
void clk_start(void);
void clk_stop(void);
clk_t clk_read(clk_seln_t seln);
clk_t clk_read_start(clk_seln_t seln);
clk_t clk_read_stop(clk_seln_t seln);
clk_t clk_diff(clk_t stop, clk_t start);
clk_t compute_total_frmwrk_cycles();
#endif

#endif /* __XAF_CLK_TEST_H__ */
