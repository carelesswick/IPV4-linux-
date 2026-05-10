#ifndef THR_LIST_H__
#define THR_LIST_H__

//这个文件实现了一个独立的线程，用于定期向客户端广播频道列表信息。
#include "medialib.h"
//创建节目单的线程
int thr_list_create(struct medialib_entry_st *lisp,int nr_ent);

//销毁线程
int thr_list_destroy(void);

#endif