/*用来解析媒体库，媒体库包含很多目录，每个目录下包含描述信息,但应该注意流量控制*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <glob.h>




#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"



#define PATHSIZE 1024



struct channel_context_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;//结构体？
    int pos;//下标表示歌曲的位置
    int fd;
    off_t offset;//什么东西？
    mytbf_t *tbf;//流量控制(多线程并发版)

}

static struct channel_context_st channel [MAXCHNID+1];

int medialib_getchalist(struct medialib_entry_st **result,int *resnum)
{
    int i;
    int num = 0;
    char path[PATHSIZE];
    glob_t globres;
    //初始化
    for (i = 0;i<MAXCHNID+1;i++){
        channel[i].chnid = -1;//写成-1代表当前频道未启用
    }
    snprintf(path,PATHSIZE,"%s/*",server_conf.media_dir);
    if (glob(path,0,NULL,&globres)){//解析目录存放到globres结构体中
        return -1;
    }
    //遍历解析的结构体
    for (i = 0;i < globres.gl_pathc;i++){
        //globres.gl_pathv[i] -> "/media/ch1"
        path2entry(globres.gl_pathv[i]);
        num++;
    }
    *result = 

    *resnum = num;
    return 0;
}

//谁申请、谁释放、谁打开、谁关闭
int medialib_freechalist(struct medialib_entry_st *)
{


}

//读取某一个频道的信息(读取的频道，读到哪里去，都多少字节)仿照read()函数
ssize_t medialib_readchn(chnid_t,void *,size_t)
{




}