/*
 *  emmc_test.c  --  emmc test application
 *
 *  Copyright (c) 2017 Rockchip Electronics Co. Ltd.
 *  Author: Panzhenzhuan Wang <randy.wang@rock-chips.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "unistd.h"

#include "emmc_test.h"

#define LOG_TAG "emmc_test"
#include "common.h"


//
#ifdef PCBA_PX3SE
    #define EMMCPATH "/sys/bus/mmc/devices/mmc0:1234/block/mmcblk0/size"
#else
    #define EMMCPATH "/sys/bus/mmc/devices/mmc0:0001/block/mmcblk0/size"
#endif

#define READ_DDR_COMMAND "cat /proc/zoneinfo | busybox grep present | \
busybox awk 'BEGIN{a=0}{a+=$2}END{print a}'"

#define RKNAND_TEST_CMD "cat /proc/rknand > %s"
#define RKNAND_TEST_FILE "/tmp/rknand_test.txt"


/* for emmc  */
static int readFromFile(const char *path, char *emmcsize_char, size_t size)
{
    if(!path)
    {
        return -1;
    }

    int fd = open(path, O_RDONLY | O_NOCTTY | O_NDELAY, 0);
    if(fd == -1)
    {
        printf("open '%s' failed!\n", path);
        return -1;
    }
    ssize_t count = read(fd, emmcsize_char, size);

    //计算出有效的字符字节数
    if(count > 0)
    {
        while(count > 0 && emmcsize_char[count - 1] == '\n')
        {
            count--;
        }
        emmcsize_char[count] = '\0';
    }
    else
    {
        emmcsize_char[0] = '\0';
    }
    close(fd);
    return count;
}

int get_emmc_size(char *size_data)
{
    int count;
    int emmc_size = 1;
    double size = (double)(atoi(size_data)) / 2 / 1024 / 1024; //需要 #include<stdlib.h>

    if(size > 0 && size <= 1)   /*1 GB */
    {
        return 1;
    }
    for(count = 1; count < 10; count++)
    {
        if((size > emmc_size) && (size <= emmc_size * 2))  /*2 - 512 GB*/
        {
            return emmc_size * 2;
        }

        emmc_size *= 2;
    }
    return -1;
}

#ifdef PCBA_PX3SE//emmc
void *emmc_test(void *argv)
{
    int emmc_ret = 0;
    char emmcsize_char[20];
    int emmc_size = 0;

    int ddr_ret = 0;
    char ddrsize_char[20];
    int ddr_size = 0;
    char cmd[128];

    printf("=======  emmc test starting   ========\n");
    //sprintf(cmd,"aplay %s/emmc_test_start.wav",AUDIO_PATH);
    //system(cmd);
    //system("aplay /data/test/emmc_test_start.wav");
    /* For emmc */
    memset(emmcsize_char, 0, sizeof(emmcsize_char));
    emmc_ret = readFromFile(EMMCPATH, emmcsize_char, sizeof(emmcsize_char));

    printf("readFromFile effective bytes is %d \n", emmc_ret);
    if(emmc_ret >= 0)     /*read back normal*/
    {
        emmc_size = get_emmc_size(emmcsize_char);
        if(emmc_size < 0)
        {
            emmc_ret = -1;
            goto fail;
        }
        printf("=======  emmc_size is: %d GB ========\n", emmc_size);
        if(EMMC_CAPACITY != emmc_size)
        {
            goto fail;
        }
    }
    else
    {
        goto fail;
    }
    printf("=======  emmc_test success  ========\n");

    return (void*)emmc_ret;
fail:
    printf("=======  emmc_test failed  ========\n");

    return (void*)emmc_ret;
}
#endif

#ifdef PCBA_3308
void *emmc_test(void *argv)
{
    char cmd[128];
    char buf[128];
    FILE *fp;
    int emmc_ret = -1;

    printf("=======  emmc test starting   ========\n");

    sprintf(cmd, RKNAND_TEST_CMD, RKNAND_TEST_FILE);
    system(cmd);
    printf("cmd is: %s.\n", cmd);
    sleep(1);
    fp = fopen(RKNAND_TEST_FILE, "r");
    //如果文件打开失败，则输出错误信息
    if(!fp)
    {
        printf("%s fopen err:%s\n", __func__, strerror(errno));
        return (void*) -1;
    }

    //检测是否包含 "Device Capacity: 4096 MB"信息，如果有说明测试正常，否则测试失败
    while(!feof(fp))
    {
        fgets(buf, sizeof(buf), fp);
        if(strstr(buf, "Device Capacity: 256 MB") != NULL)
        {
            emmc_ret = 0;
            break;
        }
    }
    fclose(fp);
    remove(RKNAND_TEST_FILE);

    printf("\n=================== function :%s finish======================\n", __func__);
    return (void*)emmc_ret;
}
#endif

int main(int argc, char *argv[])
{
    int test_flag = 0, err_code = 0;
    char buf[COMMAND_VALUESIZE] = "emmc_test";
    char result[COMMAND_VALUESIZE] = RESULT_PASS;
    test_flag = (int)emmc_test(argv[0]);
    if(test_flag < 0)
    {
        strcpy(result, RESULT_FAIL);
        err_code = EMMC_PROC_ERR;
    }
    send_msg_to_server(buf, result, err_code);
}
