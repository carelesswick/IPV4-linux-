#ifndef MEDIALIB_H__
#define MEDIALIB_H__
#include "../include/proto.h"
struct medialib_entry_st
{
    chnid_t chnid;
    char *desc;//只能在本地自己用指针类型的变量，因为指针类型在网络传输中根本没有用
};

//获取节目单函数
int medialib_getchalist(struct medialib_entry_st **result,int *resnum);


//谁申请、谁释放、谁打开、谁关闭
int medialib_freechalist(struct medialib_entry_st *ptr);

//读取某一个频道的信息(读取的频道，读到哪里去，都多少字节)仿照read()函数
ssize_t medialib_readchn(chnid_t chnid,void *buf,size_t size);

#endif