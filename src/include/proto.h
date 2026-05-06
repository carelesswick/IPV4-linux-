#ifndef PROTO_H__
#define PROTO_H__
//多播组
#define DEFAULT_MGROUP      "224.2.2.2"//默认多播组地址
#define DEFAULT_RCVPORT     "1989"      // 默认接收端口
#define CHNNR               100 //定义频道数量

#define LISTCHNID           0//节目单，（发送频道列表）
#define MINCHNID            1//最小的频道号
#define MAXCHNID            (MINCHNID+CHNNR-1)//最大频道号

//音视频频道数据包的最大长度：UDP最大负载 - IPv4头 - UDP头
#define MSG_CHANNEL_MAX     (65536-20-8)
//音视频数据的最大可用长度：实际负载 - 频道号的大小
#define MAX_DATA            (MSG_CHANNEL_MAX-sizeof(chnid_t))

#define MSG_LIST_MAX        (65536-20-8)
#define MAX_ENTRY           (MSG_LIST_MAX-sizeof(chnid_t))
#include <site_type.h>

//UDP包的结构体
struct msg_channel_st
{
    chnid_t chnid;               //频道号：其值一定介于[MINCHNID,MAXCHNID]
    uint8_t data[1];             //柔性数组：存实际的音视频数据
}__attribute__((packed));       //GCC编译器特有的指令防止内存对齐，防止接收错误的信息

// 单个频道条目（比如“1:音乐”“2:体育”）
struct msg_listentry_st
{
    chnid_t chnid;// 频道号（1、2、3...）
    uint16_t len;//本条目的总长度（包含chnid + len + desc），这个是服务器端负责填写的。
    uint8_t desc[1];//频道描述（比如“music”）
}__attribute__((packed));



//整个节目单的UDP包：包含0号频道标识 + 多条节目单记录
struct msg_list_st
{
    chnid_t chnid;              //一定是前面定义的LISTCHNID -> 0频道号
    struct msg_listentry_st entry[1];//为什么写入1是变长的数组呢？
}__attribute__((packed));

// |<- 整个 UDP 数据包的有效长度 (就是 recvfrom 返回的 len) ->|

// +-------+-------+-------+--------------------+-------+-------+--------------------+
// | chnid | chnid | len   | desc (字符串)      | chnid | len   | desc (字符串)      |
// +-------+-------+-------+--------------------+-------+-------+--------------------+
// |   0   |   1   |  12   | "1. Jay Chou\0"    |   2   |  15   | "2. Mayday\0"      |
// +-------+-------+-------+--------------------+-------+-------+--------------------+
// ^       ^                                    ^                                    ^
// |       |                                    |                                    |
// |       |-- entry[0] 的起点                  |-- entry[1] 的起点                  |-- 内存边界
// |
// |-- msg_list 的起点


#endif