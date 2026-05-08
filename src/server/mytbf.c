/*
 * @Author: Yifan Li fan868552@gmail.com
 * @Date: 2026-05-07 04:56:58
 * @LastEditors: Yifan Li fan868552@gmail.com
 * @LastEditTime: 2026-05-08 06:45:10
 * @FilePath: /carelesswick/projects/src/server/mytbf.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>


#include "mytbf.h"

struct mytbf_st
{
    int cps;    // 每秒生成的令牌数（速率）
    int burst;  // 桶的最大容量（突发上限）
    int token;  // 当前桶里的令牌数
    int pos;    // 该令牌桶在全局数组 job 中的索引（方便销毁时定位）
    pthread_mutex_t mut;//互斥锁：确保同一时间只有一个线程能修改令牌桶数据。
    pthread_cond_t cond;//条件变量：与互斥锁配合使用，用于线程间通知机制。当令牌不足时，线程可以等待（阻塞），直到其他线程通知令牌可用。

};
static struct mytbf_st *job[MYTBF_MAX];//全局数组：存储所有令牌桶实例的指针
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;//静态互斥锁：mut_job保护的是全局数组 job 本身（多个令牌桶的容器）。
static pthread_once_t init_once = PTHREAD_ONCE_INIT;//在干嘛？
static pthread_t tid;

static void *thr_alrm(void *p)
{
    int i;
    while(1){
        pthread_mutex_lock(&mut_job);
        for (i = 0;i<MYTBF_MAX;i++)
        {
            if (job[i] != NULL){
                pthread_mutex_lock(&job[i]->mut);
                job[i]->token += job[i]->cps;
                if (job[i]->token > job[i]->burst)
                    job[i]->token = job[i]->burst;
                pthread_cond_broadcast(&job[i]->cond);
                pthread_mutex_unlock(&job[i]->mut);
            }
        }
        pthread_mutex_unlock(&mut_job);
        sleep(1);
    }


}

//在干嘛？模块加载？
static void module_load(void)
{
    int err;

    err = pthread_create(&tid,NULL,thr_alrm,NULL);//在传参时，函数名会退化为指针
    if (err){
        fprintf(stderr,"pthread_create():%s\n",strerror(errno));
        exit(-1);
    }
    atexit(module_unload);//老师说这是钩子函数，这是在干嘛？
}

//模块卸载？
static void module_unload(void)
{
    int i;
    pthread_cancel(tid);
    pthread_join(tid,NULL);

    for (i = 0;i < MYTBF_MAX;i++){
        free(job[i]);
    }
    return ;
}


/**
 * @brief 在全局数组 job 中查找一个空闲位置,表示该位置未被使用，可以存储新的令牌桶  
 * 
 * @return 如果找到空闲位置，返回该位置的索引(int),如果遍历完也没找到空闲的返回-1
 */
static int get_free_pos_unlocked(void)
{
    int i;
    for (i = 0;i < MYTBF_MAX;i++){
        if (job[i] == NULL){
            return i;
        }
    }
    return -1;
}

/**
 * @brief 
 * 
 * @param cps 令牌的速率
 * @param burst 令牌桶的最大容量
 * @return mytbf_t* 
 */
mytbf_t *mytbf_init(int cps,int burst)
{
    struct mytbf_st *me;
    int pos;
    pthread_once(&init_once,module_load);   //动态模块的单实例加载

    me = malloc(sizeof(*me));//malloc返回的 void *指针被隐式转换为 struct mytbf_st *类型后赋值给me
    if (me == NULL)
        return NULL;//melloc为0，返回空指针
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    
    pthread_mutex_init(&me->mut,NULL);//动态创建互斥锁，因为是melloc得来的
    pthread_cond_init(&me->cond,NULL);

    //看不懂
    pthread_mutex_lock(&mut_job);//加锁
    pos = get_free_pos_unlocked();//没办法加锁???
    if (pos < 0){
        pthread_mutex_unlock(&mut_job);//解锁
        free(me);
        return NULL;
    }

    me->pos=pos;
    job[me->pos] = me;
    pthread_mutex_unlock(&mut_job);
    return me;
}
int mytbf_fetchtoken(mytbf_t *ptr,int size)
{
    struct mytbf_st *me = ptr;
    int n;
    pthread_mutex_lock(&me->mut);//防止别人和我一起访问token
    while (me->token <= 0){//
        pthread_cond_wait(&me->cond,&me->mut);
    }

    n = min(me->token,size);
    me ->token -= n;


    pthread_mutex_unlock(&me->mut);//有加锁一定会有解锁，否则这把锁永远打不开了，其他线程会全部死锁（卡死），程序直接瘫痪。
    return n;
}

int mytbf_returntocken(mytbf_t *ptr,int size)
{
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&me->mut);
    me->token += size;
    if (me->token > me->burst)
        me->token = me->burst;
    pthread_cond_broadcast(&me->cond);
    pthread_mutex_unlock(&me->mut);
    return 0;
}


//看不懂？？？
int mttbf_destroy(mytbf_t *ptr)
{
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&mut_job);
    job[me->pos] = NULL;
    pthread_mutex_unlock(&mut_job);

    pthread_mutex_destroy(&me->mut);
    pthread_cond_destroy(&me->cond);
    free(ptr);
    return 0;
}