#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "thr_list.h"
#include "server_conf.h"
#include "../include/proto.h"

static pthread_t tid_list;
static int nr_list_ent;
static struct medialib_entry_st *list_ent;

/**
 * @brief thr_list是一个线程函数，该线程的主要职责是：
 * 1.构建频道列表数据包：将媒体库中的频道信息打包成网络数据格式
 * 2.定期广播列表：每秒向客户端发送一次频道列表
 * 3.错误处理：记录发送过程中的错误信息
 *
 * @param p 
 * @return void* 
 */
static void *thr_list(void *p)
{
    int i;
    int size;
    int ret;
    int totalsize;
    struct msg_list_st *entlistp;
    struct msg_listentry_st *entryp;

    totalsize = sizeof(chnid_t);
    for (i = 0; i < nr_list_ent;i++){
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
    }
    entlistp = malloc(totalsize);
    if (entlistp == NULL){
        syslog(LOG_ERR,"malloc() is error%s.",strerror(errno));
    }

    entlistp->chnid = LISTCHNID;
    entryp = entlistp->entry;

    for (i = 0;i < nr_list_ent;i++){
        size = sizeof(struct msg_listentry_st)+strlen(list_ent[i].desc);
        entryp->chnid = list_ent[i].chnid;
        entryp->len = htons(size);
        strcpy(entryp->desc,list_ent[i].desc);
        entryp = (void*)(((char*)entryp) + size);

    }
    while(1){
        ret = sendto(sever_sd,entlistp,totalsize,0,(void *)&sndaddr,sizeof(sndaddr));
        if(ret < 0){
            syslog(LOG_WARNING,"sendto() is error: %s.",strerror(errno));
        }
        else{
            syslog(LOG_DEBUG,"sendto() successed.");
        }
        sleep(1);

    }

}
int thr_list_create(struct medialib_entry_st *lisp,int nr_ent)
{
    int err;
    list_ent = lisp;
    
    err = pthread_create(&tid_list,NULL,thr_list,NULL);
    if (err){
        syslog(LOG_ERR,"pthread_create()%s.",strerror(errno));
        return -1;
    }
    return 0;

} 
int thr_list_destroy(void)
{
    pthread_cancel(tid_list);
    pthread_join(tid_list,NULL);
    return 0;

}