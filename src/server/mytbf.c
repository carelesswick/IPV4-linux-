/*
 * @Author: Yifan Li fan868552@gmail.com
 * @Date: 2026-05-07 04:56:58
 * @LastEditors: Yifan Li fan868552@gmail.com
 * @LastEditTime: 2026-05-07 06:58:25
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
    pthread_mutex_t mut;//防止冲突？？？
    pthread_cond_t cond;//通知机制？？？

};
static struct mytbf_st *job[MYTBF_MAX];//全局数组：存储所有令牌桶实例的指针
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;

mytbf_t *mytbf_init(int cps,int burst);
{
    struct mytbf_st *me;

    me = malloc(sizeof(*me));//malloc返回的 void *指针被隐式转换为 struct mytbf_st *类型后赋值给me
    if (me == NULL)
        return NULL;//melloc为0，返回空指针
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    
    //???
    pthread_mutex_init(&me->mut,NULL);
    pthread_cond_init(&me->cond,NULL);

    //???
    pos = get_free_pos_unlocked();//没办法加锁???
    if (pos < 0){

    }
    me->pos=pos;

    return me;

}
int mytbf_fetchtoken(mytbf_t *,int );

int mytbf_returntocken(mytbf_t *,int);

int mttbf_destroy(mytbf_t *);