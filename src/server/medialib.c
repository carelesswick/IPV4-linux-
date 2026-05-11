/*用来解析媒体库，媒体库包含很多目录，每个目录下包含描述信息,但应该注意流量控制*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <glob.h>
#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include "../include/proto.h"
#include "medialib.h"
#include "server_conf.h"
#include "mytbf.h"



#define PATHSIZE 1024
#define LINEBUFSIZE 1024


struct channel_context_st
{
    chnid_t chnid;
    char *desc;
    glob_t mp3glob;//结构体？
    int pos;//下标表示歌曲的位置
    int fd;
    off_t offset;//什么东西？定义的起始位置？
    mytbf_t *tbf;//流量控制(多线程并发版)

};

static struct channel_context_st channel [MAXCHNID+1];
static struct channel_context_st *path2entry(const char*path)
{
    //path/desc.text    path/*.mp3
    char pathstr[PATHSIZE];
    char linebuf[LINEBUFSIZE];
    FILE *fp;
    struct channel_context_st *me;
    static chnid_t curr_id = MINCHNID;

    strncpy(pathstr,path,PATHSIZE);
    strncat(pathstr,"/desc.text",PATHSIZE);

    fp = fopen(pathstr,"r");
    if (fp ==NULL){
        syslog(LOG_INFO,"%s is not a channel dir (Can't find desc.text)",path);
        return NULL;
    }

    if (fgets(linebuf,LINEBUFSIZE,fp) == NULL){
        syslog(LOG_INFO,"%s is not a channel dir (Can't find desc.text)",path);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    me = malloc(sizeof(*me));
    if (me == NULL){
        syslog(LOG_ERR,"malloc():%s\n",strerror(errno));
        return NULL;
    }

    me->tbf = mytbf_init(MP3_BITRATE/8,MP3_BITRATE/8*10);
    if (me->tbf == NULL){
        syslog(LOG_ERR,"mytbf_init():%s\n",strerror(errno));
        free(me);
        return NULL;
    }

    me->desc = strdup(linebuf);
    strncpy(pathstr,path,PATHSIZE);
    strncat(pathstr,"/*.mp3",PATHSIZE);
    if (glob(pathstr,0,NULL,&me->mp3glob) != 0){
        curr_id++;
        syslog(LOG_ERR,"%s is not a channel dir(Can't find mp3 files)",path);
        free(me);
        return NULL;
    }
    

    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->mp3glob.gl_pathv[me->pos],O_RDONLY);
    if (me->fd < 0){
        syslog(LOG_WARNING,"%s is failed.",me->mp3glob.gl_pathv[me->pos]);
        free(me);
        return NULL;
    }
    me->chnid = curr_id;
    curr_id++;
    return me;

}
int medialib_getchalist(struct medialib_entry_st **result,int *resnum)
{
    size_t i;
    int num = 0;
    char path[PATHSIZE];
    glob_t globres;
    struct medialib_entry_st *ptr;
    struct channel_context_st *res;

    //初始化
    for (i = 0;i<MAXCHNID+1;i++){
        channel[i].chnid = -1;//写成-1代表当前频道未启用
    }
    snprintf(path,PATHSIZE,"%s/*",server_conf.media_dir);//？在干嘛？
    if (glob(path,0,NULL,&globres)){//解析目录存放到globres结构体中
        return -1;
    }

    ptr = malloc(sizeof(struct medialib_entry_st) * globres.gl_pathc);
    if (ptr ==NULL){//malloc()在出错时会返回NULL
        syslog(LOG_ERR,"malloc() error.");
        exit(-1);
    }
    
    //遍历解析的结构体
    for (i = 0;i < globres.gl_pathc;i++){
        //globres.gl_pathv[i] -> "/media/ch1"(其实是指向"/media/ch1"首个字符的指针)
        res = path2entry(globres.gl_pathv[i]);
        if (res != NULL){
            syslog(LOG_DEBUG,"path2entry() returned: %d %s.",res->chnid,res->desc);
            memcpy(channel+res->chnid,res,sizeof(*res));
            ptr[num].chnid = res->chnid;
            ptr[num].desc = strdup(res->desc);
        }
        num++;//这里出错了？我看老师是这样写的
    }
    *result = realloc(ptr,sizeof(struct medialib_entry_st) * num);//这个和malloc一样吗？
    if (*result == NULL){
        syslog(LOG_ERR,"realloc() failed.");
    }
    *resnum = num;
    return 0;
}

//谁申请、谁释放、谁打开、谁关闭
int medialib_freechalist(struct medialib_entry_st *ptr)
{
    free(ptr);
    return 0;
}

static int open_next(chnid_t chnid)
{
    size_t i;
    for (i = 0;i < channel[chnid].mp3glob.gl_pathc;i++)
    {
        channel[chnid].pos++;
        if (channel[chnid].pos == (int)channel[chnid].mp3glob.gl_pathc){
            channel[chnid].pos = 0;
            break;
        }

        close(channel[chnid].fd);

        channel[chnid].fd = open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],O_RDONLY);

        if (channel[chnid].fd < 0){
            syslog(LOG_WARNING,"open(%s):%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],strerror(errno));
        }
        else{//successed
            channel[chnid].offset = 0;
            return 0;
        }
    }
    syslog(LOG_ERR,"None of mp3s in channel %d is availab.",chnid);
    return -1;
}

//读取某一个频道的信息(读取的频道，读到哪里去，都多少字节)仿照read()函数
ssize_t medialib_readchn(chnid_t chnid,void *buf,size_t size)
{
    int tbfsize;
    ssize_t len;
    tbfsize = mytbf_fetchtoken(channel[chnid].tbf,size);

    while (1)
    {
        len = pread(channel[chnid].fd,buf,tbfsize,channel[chnid].offset);
        if(len < 0){
            syslog(LOG_WARNING,"media file %s pread();%s",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]\
            ,strerror(errno));//读取失败可能说明这个歌曲有问题，切换到下一首
            open_next(chnid);
        }

        else if(len == 0){//如果读完了，那么关掉这一首，读取下一首
            syslog(LOG_DEBUG,"nedia file %s is over.",channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
            open_next(chnid);
        }
        else{//len > 0代表成功读取（这里需要往再在缩进吗？我看老师else往里面又缩进了
            channel[chnid].offset += len;//往后延续
            break;//?这个读取了一次就跳出从循环了？
        }
    }
    if ((tbfsize - len) > 0){
        mytbf_returntocken(channel[chnid].tbf,tbfsize - len);//归还令牌桶不应该是？找一个空的桶吗？
    }
    return len;
}
