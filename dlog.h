/*
 * dlog.h
 *
 * Sam Chen <xuejian1354@163.com>
 *
 */
#ifndef __DLOG_H__
#define __DLOG_H__

#include "mtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLOG_PRINT

#define AI_PRINTF(format, args...)	   \
st(    \
	printf(format, ##args);    \
)

#ifdef DLOG_PRINT
#define AO_PRINTF AI_PRINTF
#else
#define AO_PRINTF(format, args...)  \
st(  \
	FILE *fp = NULL;    \
    if((fp = fopen(TMP_LOG, "a+")) != NULL)   \
    {   \
        fprintf(fp, format, ##args);  \
        fclose(fp); \
    }   \
)
#endif


#define PRINT_HEX(data, len)	\
st(	\
        int x;	\
        for(x=0; x<len; x++)	\
        {	\
                AO_PRINTF("%02X ", data[x]);	\
        }	\
        AO_PRINTF("\n");	\
)


#ifdef __cplusplus
}
#endif

#endif  //__DLOG_H__
