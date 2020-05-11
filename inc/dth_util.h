/***********************************************************************
 * @ file dth_util.h
       __DTH_UTIL_H__
 * @ brief util header file
 * @ history
 * Date        Version  Author     description
 * ==========  =======  =========  ====================================
 * 2019-11-14  V1.0     sleng      Create
 *
 * @ Copyright (C)  2019  Disthen  all right reserved
 ***********************************************************************/
#ifndef __DTH_UTIL_H__
#define __DTH_UTIL_H__

#ifdef __cplusplus
extern "C"
{
#endif


extern unsigned int get_file_size(const char *path);
extern unsigned char atox(const char *str);
extern int get_md5str(const char *path, char *out, int size);
extern int get_md5sum(const char *path, unsigned char *out, int size);


#ifdef __cplusplus
extern "C"
};
#endif

#endif //End of __DTH_UTIL_H__
