#ifndef THR_LIST_H__
#define THR_LIST_H__

#include "medialib.h"
//创建节目单的线程
int thr_list_create(struct medialib_entry_st *,int);

//销毁线程
int thr_list_destroy(void);

#endif