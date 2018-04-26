/******************************************************************
 *                                                                *
 *      Copyright (c)  Mentor Graphics Corporation 2004           *
 *                                                                *
 *                All Rights Reserved.                            *
 *                                                                *
 *    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
 *  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
 *  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
 *                                                                *
 ******************************************************************/

/*
 * Controller definitions.
 * $Revision: 1.3 $
 */

#ifndef __MUSB_CONTROLLER_H__
#define __MUSB_CONTROLLER_H__

#include "plat_cnf.h"

/* Set feature types per controller type */
#ifdef MUSB_MHDRC
#define MGC_OTG
#define MUSB_MHDRC_DMA
#endif

#if defined(MGC_OTG) && !defined(MUSB_OTG)
#define MUSB_OTG
#endif

#if defined(MUSB_MHDRC_DMA) && !defined(MUSB_DMA)
#define MUSB_DMA
#endif

#endif	/* multiple inclusion protection */
