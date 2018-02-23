/**
 ******************************************************************************
 * @file    mico_poll.h
 * @author  William Xu
 * @version V1.0.0
 * @date    16-Feb-2018
 * @brief   This file provides all the headers of BSD poll APIs.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2018 MXCHIP Inc.
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
#ifndef __MICO_POLL_H__
#define __MICO_POLL_H__

struct pollfd {
    int fd; /**< fd related to */
    short events; /**< which POLL... events to respond to */
    short revents; /**< which POLL... events occurred */
};
#define POLLIN      0x0001
#define POLLPRI     0x0002
#define POLLOUT     0x0004
#define POLLERR     0x0008
#define POLLHUP     0x0010
#define POLLNVAL    0x0020

/** A mechanism to multiplex input/output over a set of file handles(file descriptors).
 * For every file handle provided, poll() examines it for any events registered for that particular
 * file handle.
 *
 * @param fhs     an array of PollFh struct carrying a FileHandle and bitmasks of events
 * @param nfhs    number of file handles
 * @param timeout timer value to timeout or -1 for loop forever
 *
 * @return number of file handles selected (for which revents is non-zero). 0 if timed out with nothing selected. -1 for error.
 */
int poll(struct pollfd *fds, int nfds, int timeout);


#endif //MICO_POLL_H
