#ifndef MYTBF_H__
#define MYTBF_H__


#define MYTBF_MAX    1024    //令牌桶的上限
typedef void mytbf_t;//不透明指针（Opaque Pointer），是 C语言实现“面向对象封装”的工具


mytbf_t *mytbf_init(int cps,int burst);

int mytbf_fetchtoken(mytbf_t *ptr,int size);

int mytbf_returntocken(mytbf_t *ptr,int size);

int mytbf_destroy(mytbf_t *ptr);


#endif