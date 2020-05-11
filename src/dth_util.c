/***********************************************************************
 * @ file dth_util.c
       __DTH_UTIL_C__
 * @ brief disthen util source file
 * @ history
 * Date        Version  Author     description
 * ==========  =======  =========  ====================================
 * 2019-11-11  V1.0     sleng      Create
 *
 * @ Copyright (C)  2019  Disthen  all right reserved
 ***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "sleng_debug.h"


unsigned int get_file_size(const char *path)
{
    unsigned int filesize = -1;
    struct stat statbuff;

    if (stat(path, &statbuff) < 0)
    {
        return filesize;
    }
    else
    {
        filesize = statbuff.st_size;
    }
    return filesize;
}


unsigned char atox(const char *str)
{
    unsigned char low, high, l, h;
    low = (strlen(str) == 1)? str[0]: str[1];
    high = (strlen(str) == 1)? '0': str[0];
    if (low >= '0' && low <= '9') {
        l = low - '0';
    } else if (low >= 'a' && low <= 'f') {
        l = low - 'a' + 10;
    }
    if (high >= '0' && high <= '9') {
        h = high - '0';
    } else if (high >= 'a' && high <= 'f') {
        h = high - 'a' + 10;
    }
    return (h<<4)|l;
}


int get_md5str(const char *path, char *out, int size)
{
    int ret = 0;
    char cmd[256] = {0,};
    char buf[128] = {0, };
    FILE *fp = NULL;

    do {
        if (out == NULL || size < 32)
        {
            errno = EINVAL;
            ret = -1;
            break;
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "md5sum %s", path);
        fp = popen(cmd, "r");
        if (fp == NULL) {
            perror("popen md5sum failed");
            break;
        }
        fread(buf, 1, sizeof(buf), fp);
        sleng_debug("buf=%s\n", buf);

        memcpy(out, buf, 32);

    } while(0);

    if (fp) pclose(fp);
    return ret;
}


int get_md5sum(const char *path, unsigned char *out, int size)
{
    int ret = 0;
    char cmd[256] = {0,};
    char tmp[3] = {0, };
    char buf[128] = {0, };
    int i;
    FILE *fp = NULL;

    do {
        if (out == NULL || size < 16)
        {
            errno = EINVAL;
            ret = -1;
            break;
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "md5sum %s", path);
        fp = popen(cmd, "r");
        if (fp == NULL) {
            perror("popen md5sum failed");
            break;
        }
        fread(buf, 1, sizeof(buf), fp);
        sleng_debug("buf=%s\n", buf);

        for (i = 0; i < 32; i += 2) {
            tmp[0] = buf[i];
            tmp[1] = buf[i+1];
            tmp[2] = '\0';
            out[i/2] = atox(tmp);
            // sleng_debug("tmp=%s, local_md5[%d]=%02hhx\n", tmp, i/2, local_md5[i/2]);
        }
        if (buf[i] != ' ') {
            sleng_error("local md5sum format error");
            ret = -1;
            break;
        }
    } while(0);

    if (fp) pclose(fp);
    return ret;
}
