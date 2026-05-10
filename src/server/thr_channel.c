#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>



#include "server_conf.h"
#include "thr_channel.h"
#include "../include/site_type.h"
#include "../include/proto.h"

static int tid_nextpos = 0;
struct thr_channel_entry_st
{
    chnid_t chnid;
    pthread_t tid;
};

struct thr_channel_entry_st  thr_channel[CHNNR];//结构体数组

/**
 * @brief 组织好线程待发送的数据，通过socket发送数据
 * 
 * @param ptr 
 * @return void* 
 */
static void *thr_channel_snder(void *ptr)
{
    int datasize;
    ssize_t len;
    struct msg_channel_st *sbufp;//变长的结构体
    struct medialib_entry_st *ent = ptr;

    sbufp = malloc(MSG_CHANNEL_MAX);
    if(sbufp == NULL){
        syslog(LOG_ERR,"malloc():%s\n",strerror(errno));
        exit(-1);
    }

    sbufp->chnid = ent->chnid;
    while(1){
        len = medialib_readchn(ent -> chnid,sbufp->data,MAX_DATA);

        if (sendto(sever_sd,sbufp,len + sizeof(chnid_t),0,(void*)&sndaddr,sizeof(sndaddr)) < 0);
        {
            syslog(LOG_ERR,"thr_channel(%d) sendto() is failed:%s,",ent->chnid,strerror(errno));
        }

        sched_yield();
    }
    pthread_exit(NULL);

}

int thr_channel_create(struct medialib_entry_st *ptr)
{
    int err;
    err = pthread_create(&(thr_channel[tid_nextpos].tid),NULL,thr_channel_snder,ptr);
    if (err){
        syslog(LOG_WARNING,"pthread_create():%s",strerror(err));
        return -err;//pthread_create出错返回错误码(是正数)，执行成功返回0
    }
    thr_channel[tid_nextpos].chnid = ptr->chnid;
    tid_nextpos++;
    return 0;
}


int thr_channel_destroy(struct medialib_entry_st *ptr)
{
    int i;
    for(i = 0;i< CHNNR;i++){

        if (thr_channel[i].chnid == ptr->chnid){
            if (pthread_cancel(thr_channel[i].tid) < 0){
                syslog(LOG_ERR,"pthread_cancel():the thread of channel %d is err.",ptr->chnid);
                return -ESRCH;
            }
        }
        pthread_join(thr_channel[i].tid);
        thr_channel[i]=-1;
        return 0;
    }

}

int thr_channel_destroyall(void)
{
    int i;
    for(i = 0;i < CHNNR; i++){
        if (thr_channel[i].chnid > 0){
            if (pthread_cancel(thr_channel[i].tid) < 0){
                syslog(LOG_ERR,"pthread_cancel(): the tthread of channel %d",thr_channel[i].chnid);
                return -ESRCH;
            }
            pthread_join();
            thr_channel[i] = -1;
        }

    }
    return 0;

}