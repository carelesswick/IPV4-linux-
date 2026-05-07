/*
 * @Author: Yifan Li fan868552@gmail.com
 * @Date: 2026-05-07 04:56:58
 * @LastEditors: Yifan Li fan868552@gmail.com
 * @LastEditTime: 2026-05-07 07:57:19
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
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;//互斥锁：mut_job 保护的是全局数组 job 本身（多个令牌桶的容器）。

mytbf_t *mytbf_init(int cps,int burst);
{
    struct mytbf_st *me;

    me = malloc(sizeof(*me));//malloc返回的 void *指针被隐式转换为 struct mytbf_st *类型后赋值给me
    if (me == NULL)
        return NULL;//melloc为0，返回空指针
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    
    
    pthread_mutex_init(&me->mut,NULL);//&的优先级低于->
    pthread_cond_init(&me->cond,NULL);

    //???
    pthread_mutex_lock(&mut_job);
    pos = get_free_pos_unlocked();//没办法加锁???
    if (pos < 0){

    }

    me->pos=pos;
    job[me->pos] = me;
    pthread_mutex_unlock(&mut_job);



    return me;

}
int mytbf_fetchtoken(mytbf_t *,int );

int mytbf_returntocken(mytbf_t *,int);

int mttbf_destroy(mytbf_t *);