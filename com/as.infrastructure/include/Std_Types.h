/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef STD_TYPES_H
#define STD_TYPES_H
/* ============================ [ INCLUDES  ] ====================================================== */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "Compiler.h"
#ifndef __QT__
#include "asdebug.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#ifndef MIN
#define MIN(_x,_y) (((_x) < (_y)) ? (_x) : (_y))
#endif
#ifndef MAX
#define MAX(_x,_y) (((_x) > (_y)) ? (_x) : (_y))
#endif
#ifndef E_OK
#define E_OK 					(Std_ReturnType)0
#endif
#ifndef E_NOT_OK
#define E_NOT_OK 				(Std_ReturnType)1
#endif

#define E_NO_DTC_AVAILABLE		(Std_ReturnType)2
#define E_SESSION_NOT_ALLOWED	(Std_ReturnType)4
#define E_PROTOCOL_NOT_ALLOWED	(Std_ReturnType)5
#define E_REQUEST_NOT_ACCEPTED	(Std_ReturnType)8
#define E_REQUEST_ENV_NOK		(Std_ReturnType)9
#ifndef E_PENDING	// for WIN32
#define E_PENDING				(Std_ReturnType)10
#endif
#define E_COMPARE_KEY_FAILED	(Std_ReturnType)11
#define E_FORCE_RCRRP			(Std_ReturnType)12

#ifndef NULL
#define NULL 0
#endif

#ifndef NULL_PTR
#define NULL_PTR (void*)0
#endif

#ifndef TRUE
#define TRUE                    (boolean)1
#endif
#ifndef FALSE
#define FALSE                   (boolean)0
#endif

#ifndef True
#define True                    (boolean)1
#endif
#ifndef False
#define False                   (boolean)0
#endif

#define STD_HIGH		0x01
#define STD_LOW			0x00

#define STD_ACTIVE		0x01
#define STD_IDLE		0x00

#define STD_ON			0x01
#define STD_OFF			0x00

/*
 * code for ArcCore 3.1 AUTOSAR stack
 */
#ifndef imask_t
#define imask_t uint32
#endif

#define Irq_Save(irq_state)  		{extern imask_t portGetIrqStateAndDisableIt(void);irq_state = portGetIrqStateAndDisableIt();}
#define Irq_Restore(irq_state)		{extern void portRestroeIrqState(imask_t irq_state);portRestroeIrqState(irq_state);}

/* ============================ [ TYPES     ] ====================================================== */
typedef bool		                boolean;
typedef int8_t        				sint8;
typedef uint8_t			       		uint8;
typedef int16_t		        		sint16;
typedef uint16_t		      		uint16;
/* note for 64 bit ubuntu, long is 8 bytes */
typedef int32_t	         	    	sint32;
typedef uint32_t		       		uint32;
typedef int64_t			 			sint64;
typedef uint64_t			 		uint64;
typedef uint32                      uint8_least;
typedef uint32                      uint16_least;
typedef uint32                      uint32_least;
typedef uint32                      sint8_least;
typedef uint32                      sint16_least;
typedef uint32                      sint32_least;
typedef float                       float32;
typedef double                      float64;

typedef uint8                       Std_ReturnType;

typedef struct {
	// TODO: not done!!
	uint16 vendorID;
	uint16 moduleID;
	uint8  instanceID;

	uint8 sw_major_version;    /**< Vendor numbers */
	uint8 sw_minor_version;    /**< Vendor numbers */
	uint8 sw_patch_version;    /**< Vendor numbers */

	uint8 ar_major_version;    /**< Autosar spec. numbers */
	uint8 ar_minor_version;    /**< Autosar spec. numbers */
	uint8 ar_patch_version;    /**< Autosar spec. numbers */
} Std_VersionInfoType;

/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* STD_TYPES_H */