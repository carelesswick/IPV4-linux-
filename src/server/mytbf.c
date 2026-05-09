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
static pthread_once_t init_once = PTHREAD_ONCE_INIT;//创建多线程单次执行的宏
static pthread_t tid;//线程的PID
#define min(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief 创建thr_alrm线程，用来遍历全局数组job中的所有令牌桶,每隔一秒为每个存在的令牌桶补充令牌
 * 
 * @param p 
 * @return void* 
 */
static void *thr_alrm(void *p)
{
    //thr_alrm函数是作为线程的入口函数使用的，根据pthread_create的要求，线程函数必须符合void *(*)(void *)
    int i;
    while(1){
        pthread_mutex_lock(&mut_job);
        for (i = 0;i<MYTBF_MAX;i++)
        {
            if (job[i] != NULL){//安全访问，确保只对存在的令牌桶实例进行令牌补充操作
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

/**
 * @brief 线程收尾函数，会在程序退出时自动执行。
 * 
 */
static void module_unload(void)
{
    int i;
    pthread_cancel(tid);//取消thr_alrm线程
    pthread_join(tid,NULL);//等待线程结束

    for (i = 0;i < MYTBF_MAX;i++){
        free(job[i]);//释放全局数组job中所有令牌桶实例的内存
    }
    return ;
}


/**
 * @brief module_load函数是这个模块的初始化函数，它负责：
 * 1.创建一个后台线程(thr_alrm)来定期补充令牌
 * 2.注册模块卸载函数(module_unload)作为程序退出时的回调
 */
static void module_load(void)
{
    int err;

    err = pthread_create(&tid,NULL,thr_alrm,NULL);//在传参时，函数名会退化为指针
    if (err){
        fprintf(stderr,"pthread_create():%s\n",strerror(errno));
        exit(-1);
    }
    atexit(module_unload);//退出钩子函数：atexit注册的函数会在程序退出时被"钩住"并执行，因此可以称为退出钩子。
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
 * @brief 令牌桶的初始化函数,实现令牌桶的注册
 * @param cps 令牌的速率
 * @param burst 令牌桶的最大容量
 * @return mytbf_t* 一个令牌桶结构体的地址
 */
mytbf_t *mytbf_init(int cps,int burst)
{
    struct mytbf_st *me;//使用指针来声明，而不是直接声明结构体，可以继续使用令牌桶
    int pos;
    pthread_once(&init_once,module_load);
    //////////////////////////////////////////////////
    //第一个调用mytbf_init的线程会执行module_load;后续线程直接跳过module_load，直接执行后续代码
    //////////////////////////////////////////////////
    me = malloc(sizeof(*me));//malloc返回的 void *指针被隐式转换为 struct mytbf_st *类型后赋值给me
    if (me == NULL)
        return NULL;//melloc为0，返回空指针
    me->cps = cps;
    me->burst = burst;
    me->token = 0;
    
    pthread_mutex_init(&me->mut,NULL);//动态创建互斥锁，因为是melloc得来的
    pthread_cond_init(&me->cond,NULL);

    pthread_mutex_lock(&mut_job);//加锁保护全局数组 job
    pos = get_free_pos_unlocked();//查找空闲位置存储新令牌桶
    if (pos < 0){//如果没找到空闲位置
        pthread_mutex_unlock(&mut_job);//解锁
        free(me);//释放内存
        return NULL;
    }

    me->pos=pos;
    job[me->pos] = me;//注册到全局数组
    pthread_mutex_unlock(&mut_job);
    return me;
}

/**
 * @brief 该函数用来获取token
 * 
 * @param ptr 一个令牌桶的指针
 * @param size 要取得token大小
 * @return int 返回取得token大小
 */
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

/**
 * @brief 将未使用的令牌归还到令牌桶中，这是令牌桶算法中的一个重要操作。
 * 
 * @param ptr 
 * @param size 
 * @return int 
 */
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


/**
 * @brief 销毁一个令牌桶实例并释放其占用的所有资源
 * @param ptr 
 * @return int 
 */
int mytbf_destroy(mytbf_t *ptr)
{
    struct mytbf_st *me = ptr;

    pthread_mutex_lock(&mut_job);
    job[me->pos] = NULL;//将令牌桶在全局数组中的位置设为 NULL，这样后台线程 thr_alrm 就不会再访问这个令牌桶
    pthread_mutex_unlock(&mut_job);

    pthread_mutex_destroy(&me->mut);//销毁令牌桶的互斥锁
    pthread_cond_destroy(&me->cond);//销毁令牌桶的条件变量
    free(ptr);//释放令牌桶内存，因为令牌桶是通过mallc得来的，一定要free()
    return 0;
}