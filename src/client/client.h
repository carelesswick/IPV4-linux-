#ifndef CLIENT_H__
#define CLIENT_H__

#define DEFAULT_PLAYERCMD       "/usr/bin/mpg123 - > /dev/null"

//用户可以指定的命令结构体
struct client_conf_st
{
    char* rcvport;//接收端口
    char* mulgroup;//多播地址
    char* player_cmd;//播放器
};

extern struct client_conf_st client_conf;


#endif