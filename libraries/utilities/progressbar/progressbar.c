/**
 ******************************************************************************
 * @file    progressbar.c
 * @author  Snow Yang
 * @version V1.0.0
 * @date    2018-10-15
 * @brief   This file contains functiond called by the MICO project.
 *          These functions abstract the interaction with AES libraries.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2014 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mico.h"

#define WIDTH 80
#define MARKER '#'
#define FILL '-'
const char *ANIMATED_MARKER = "|/-\\";

static char line[WIDTH];

static uint32_t max_value;
static uint32_t last_marker_width;
static uint32_t last_value;
static uint32_t last_time;
static uint32_t start_time;
static uint32_t width = WIDTH - 2 - 5 - 9 - 12 - 9;

uint32_t seconds(void)
{
    return mico_rtos_get_time() / 1000;
}

void progressbar_init(uint32_t value)
{
    max_value = value;
    last_marker_width = 0;
    last_value = 0;
    last_time = 0;
    start_time = 0;
}

void progressbar_update(uint32_t value)
{
    static uint32_t i = 0;
    uint32_t now_time;
    static uint32_t remain_time = 0;
    uint32_t marker_width, fill_width;
    static float speed = 0;
    char *p;

    if (value > max_value)
        return;

    marker_width = value * width / max_value;
    if (marker_width <= last_marker_width)
        return;
    last_marker_width = marker_width;
    fill_width = width - marker_width;

    now_time = seconds();
    if (last_time == 0)
        start_time = now_time;
    else if (now_time > last_time)
    {
        speed = ((value - last_value) / (now_time - last_time));
        remain_time = (max_value - value) / speed;
    }

    last_value = value;
    last_time = now_time;

    p = line;
    memset(p, MARKER, marker_width);
    p += marker_width;
    memset(p, FILL, fill_width);
    p += fill_width;
    p += sprintf(p, " %c", ANIMATED_MARKER[i = ++i > 3 ? 0 : i]);
    p += sprintf(p, " %3d%%", (int)value * 100 / (int)max_value);
    p += sprintf(p, " %4d KiB", (int)value / 1024);
    p += sprintf(p, " %5.1f KiB/s", speed / 1024.0);
    if (value == max_value)
        remain_time = now_time - start_time;
    p += sprintf(p, " %02d:%02d:%02d", (int)remain_time / 3600, (int)remain_time % 3600 / 60, (int)remain_time % 60);
    printf("\r%s", line);
    fflush(stdout);

    if (value == max_value)
        printf("\r\n");
}

void progressbar_test(void)
{
    uint32_t i;

    progressbar_init(1024 * 36);

    for (i = 1; i <= 36; i++)
    {
        progressbar_update(i * 1024);
        mico_rtos_thread_sleep(1);
    }
}
