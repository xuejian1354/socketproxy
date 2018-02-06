/*
 * mtypes.h
 *
 * Sam Chen <xuejian1354@163.com>
 *
 */
#ifndef __MTYPES_H__
#define __MTYPES_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef signed   char   int8;
typedef unsigned char   uint8;

typedef signed   short  int16;
typedef unsigned short  uint16;

typedef signed   int   int32;
typedef unsigned int   uint32;

typedef uint8   byte;

#define st(x)      do { x } while (0)

#ifdef __cplusplus
}
#endif

#endif //__MTYPES_H__
