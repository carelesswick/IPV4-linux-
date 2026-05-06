#ifndef SEVER_CONF_H__
#define SEVER_CONF_H__

#define DEFAULT_MEDIADIR    "/var/media/"
#define DEFAULT_INFCNAME    "ens33"//ens33

enum{
    RUN_DAEMON = 1,//作为守护进程
    RUN_FRONTDESK = 2//作为前台运行
};

//默认配置文件结构体
struct server_conf_st
{
    char* rcvport;
    char* mulgroup;
    char* media_dir;
    char  runmode;
    char* infcname;
};
extern struct server_conf_st server_conf;


#endif