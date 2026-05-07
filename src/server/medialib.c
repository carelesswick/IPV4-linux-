/*用来解析媒体库，媒体库包含很多目录，每个目录下包含描述信息,但应该注意流量控制*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "medialib.h"

struct channel_context_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;//结构体？
    int pos;下标表示歌曲的位置
    int fd;
    off_t offset;//什么东西？
    mytbf_t *tbf;//流量控制(多线程并发版)

}