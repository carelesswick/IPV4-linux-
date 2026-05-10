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

// 线程模块自身的头文件（声明线程创建/销毁接口，供外部调用）
#include "thr_list.h"
// 服务器配置头文件（包含全局配置：如广播地址、套接字描述符等）
#include "server_conf.h"
// 协议头文件（定义网络通信的数据包格式、频道ID常量等）
#include "../include/proto.h"

// 静态全局变量：广播线程的线程ID（仅本文件可见）
static pthread_t tid_list;
// 静态全局变量：媒体库中频道条目的总数（仅本文件可见）
static int nr_list_ent;
// 静态全局变量：指向媒体库频道条目数组的指针（仅本文件可见）
static struct medialib_entry_st *list_ent;

/**
 * @brief 频道列表广播线程的核心执行函数
 * 
 * 核心职责：
 * 1. 计算频道列表数据包的总大小，动态分配内存；
 * 2. 按照自定义网络协议封装频道列表数据包；
 * 3. 无限循环（每秒一次）向客户端UDP广播该数据包；
 * 4. 记录发送过程中的成功/失败日志。
 * 
 * @param p 线程入参（本场景未使用，pthread_create要求的通用参数）
 * @return void* 线程返回值（本场景未使用，返回NULL）
 */
static void *thr_list(void *p)
{
    int i;                  // 循环计数器
    int size;               // 单个频道条目（含描述符）的字节大小
    int ret;                // 系统调用（sendto）的返回值
    int totalsize;          // 频道列表数据包的总字节大小
    struct msg_list_st *entlistp;      // 指向整个频道列表数据包的指针
    struct msg_listentry_st *entryp;   // 指向单个频道条目数据的指针

    // 第一步：计算数据包总大小（基础大小 + 所有频道条目大小）
    // 基础大小：频道列表的标识ID（chnid_t类型，对应LISTCHNID常量）
    totalsize = sizeof(chnid_t);
    // 累加每个频道条目的大小：固定结构体大小 + 频道描述符的字符串长度
    for (i = 0; i < nr_list_ent; i++) {
        totalsize += sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
    }

    // 第二步：动态分配数据包内存（存放封装后的频道列表数据）
    entlistp = malloc(totalsize);
    if (entlistp == NULL) {
        // 分配失败：记录错误日志（syslog），线程无返回值（后续循环会执行但内存为空，存在风险）
        syslog(LOG_ERR, "malloc() failed: %s.", strerror(errno));
    }

    // 第三步：封装数据包头部（频道列表的标识ID）
    entlistp->chnid = LISTCHNID;  // LISTCHNID是proto.h中定义的“列表频道ID”常量
    // 初始化单个频道条目指针，指向数据包头部后的第一个条目位置
    entryp = entlistp->entry;

    // 第四步：逐个封装每个频道的信息到数据包中
    for (i = 0; i < nr_list_ent; i++) {
        // 计算当前频道条目的总大小（固定结构体 + 描述符字符串长度）
        size = sizeof(struct msg_listentry_st) + strlen(list_ent[i].desc);
        // 填充频道ID（从媒体库条目复制）
        entryp->chnid = list_ent[i].chnid;
        // 填充当前条目总长度（网络字节序，避免大小端问题）
        entryp->len = htons(size);
        // 填充频道描述符（字符串拷贝）
        strcpy(entryp->desc, list_ent[i].desc);
        // 移动指针到下一个频道条目的起始位置（字节偏移）
        entryp = (void*)(((char*)entryp) + size);
    }

    // 第五步：无限循环广播数据包（每秒一次）
    while (1) {
        // 发送UDP数据包：
        // sever_sd：服务器套接字描述符（来自server_conf.h的全局变量，疑似拼写错误：应为server_sd）
        // entlistp：待发送的数据包缓冲区
        // totalsize：数据包总大小
        // 0：发送标志（默认）
        // &sndaddr：广播目标地址（来自server_conf.h的全局变量，包含IP+端口）
        // sizeof(sndaddr)：目标地址结构体大小
        ret = sendto(sever_sd, entlistp, totalsize, 0, (void *)&sndaddr, sizeof(sndaddr));
        if (ret < 0) {
            // 发送失败：记录警告日志（不终止线程，继续尝试）
            syslog(LOG_WARNING, "sendto() failed: %s.", strerror(errno));
        } else {
            // 发送成功：记录调试日志
            syslog(LOG_DEBUG, "sendto() successed. Sent %d bytes.", ret);
        }
        // 休眠1秒，控制广播频率
        sleep(1);
    }

    // 理论上不会执行到此处（while(1)无限循环），返回NULL符合线程函数规范
    return NULL;
}

/**
 * @brief 创建频道列表广播线程
 * 
 * 外部调用接口：初始化线程依赖的全局变量，创建广播线程。
 * 
 * @param lisp 指向媒体库频道条目数组的指针（由外部模块传入）
 * @param nr_ent 媒体库中频道条目的总数（由外部模块传入）
 * @return int 成功返回0，失败返回-1（并记录错误日志）
 */
int thr_list_create(struct medialib_entry_st *lisp, int nr_ent)
{
    int err;  // pthread_create的返回值（线程创建错误码）
    
    // 初始化本文件的静态全局变量：保存媒体库数据的指针和条目数
    list_ent = lisp;
    nr_list_ent = nr_ent;  // 注：原代码遗漏了这行！导致nr_list_ent始终为初始值0，是半成品关键问题
    
    // 创建线程：
    // &tid_list：输出线程ID
    // NULL：使用默认线程属性
    // thr_list：线程执行函数
    // NULL：线程入参（无）
    err = pthread_create(&tid_list, NULL, thr_list, NULL);
    if (err) {
        // 创建失败：记录错误日志，返回-1
        syslog(LOG_ERR, "pthread_create() failed: %s.", strerror(errno));
        return -1;
    }
    
    // 创建成功：返回0
    return 0;
} 

/**
 * @brief 销毁频道列表广播线程
 * 
 * 外部调用接口：取消线程执行，等待线程退出，释放线程资源。
 * 注：本函数未处理thr_list中malloc的内存，存在内存泄漏。
 * 
 * @return int 固定返回0（简化处理，未检查pthread_cancel/pthread_join的错误）
 */
int thr_list_destroy(void)
{
    // 取消线程（向线程发送取消信号，终止while(1)循环）
    pthread_cancel(tid_list);
    // 等待线程退出，回收线程资源（阻塞直到线程终止）
    pthread_join(tid_list, NULL);
    
    // 简化处理：固定返回0（未检查cancel/join的错误）
    return 0;
}