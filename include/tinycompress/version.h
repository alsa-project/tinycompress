/* SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause) */
/* Copyright (c) 2011-2012, Intel Corporation */

#ifndef __VERSION_H
#define __VERSION_H


#define TINYCOMPRESS_LIB_MAJOR		0 /* major number of library version */
#define TINYCOMPRESS_LIB_MINOR		2 /* minor number of library version */
#define TINYCOMPRESS_LIB_SUBMINOR	0 /* subminor number of library version */

/** library version */
#define TINYCOMPRESS_LIB_VERSION \
		((TINYCOMPRESS_LIB_MAJOR<<16)|\
		 (TINYCOMPRESS_LIB_MINOR<<8)|\
		  TINYCOMPRESS_LIB_SUBMINOR)

/** library version (string) */
#define TINYCOMPRESS_LIB_VERSION_STR	"0.2.0"

#endif
