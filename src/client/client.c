#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <unistd.h>
#include "client.h"
#include <proto.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/if.h>
/*主要接受Server端的数据，然后子进程来进行解析播放*/
/*  -M --mulgroup 指定多播组
*   -P --port     指定接收端口
*   -p --player   指定播放器
*   -H --help     显示帮助   */
//这是结构体变量的初始化，不是赋值。指定默认的多播组的信息（端口，地址，播放器）
struct client_conf_st client_conf = {\
    .rcvport = DEFAULT_RCVPORT,\
    .mulgroup = DEFAULT_MGROUP,\
    .player_cmd = DEFAULT_PLAYERCMD};

static void printfhelp(void)
{
    printf("-P --port     指定接收端口\n\
            -M --mulgroup 指定多播组\n\
            -p --player   指定播放器\n\
            -H --help     显示帮助\n");


}

static ssize_t writen(int fd, const char *buf, size_t len)
{
    int ret;
    int pos = 0;
    while(len > 0){

        ret = write(fd,buf+pos,len);
        if (ret < 0){
            if (errno == EINTR)
                continue;
            perror("write()");
            return -1;
        }
        len -= ret;
        pos += ret;
    }
    return pos;
}

int main(int argc,char** argv)
{
    int c;
    int index = 0;
    int sd;
    int val;
    int pd[2];//无名管道通信的数组参数， pd[0]：固定为读端；pd[1]：固定为写端；
    int pid;
    int len,len2;
    int ret,chosen_id;
    //定义长选项的结构体数组
    /*四个选项分别为：{长选项的值；0无参数1带参数2可选带不带参数；
                    设置为NULL解析成功会返回第四个参数；返回值}*/
    struct option argarr[] = {
        {"port",1,NULL,'P'},
        {"mulgroup",1,NULL,'M'},
        {"player",1,NULL,'p'},
        {"help",0,NULL,'H'},
        {NULL,0,NULL,0}
    };

    //这个是setsockopt中第四个参数中用到的结构体
    struct ip_mreqn mreq;
    /////////////////////////////////////////////////////////////////////////
    //  struct ip_mreqn {
    //                   struct in_addr imr_multiaddr; /* 多播组地址（二进制） */
    //                   struct in_addr imr_address;   /* 本地接口地址（二进制） */
    //                   int            imr_ifindex;   /* 网络接口索引 */
    //               };
    /////////////////////////////////////////////////////////////////////////
    //规定的本地IPv4地址结构，在man 7 IP 中可以查看
    struct sockaddr_in localaddr;
    struct sockaddr_in serveraddr;//父进程rcvfrom()函数用来存放节目单包的IP+端口
    struct sockaddr_in remoteaddr;//父进程rcvfrom()函数用来存放Sever的IP+端口
    
    socklen_t serveraddr_len = sizeof(serveraddr);
    socklen_t remoteaddr_len = sizeof(remoteaddr);
/////////////////////////////////////////////////////////////////////////
    // struct sockaddr_in {
    //            sa_family_t    sin_family; /* 地址族，固定填 AF_INET */
    //            in_port_t      sin_port;   /* 16 位端口号，必须用网络字节序*/
    //            struct in_addr sin_addr;   /* 32 位 IP 地址 */
   //             unsigned char  sin_zero[8];// 填充位，无意义，置0即可
    //        };

    //        /* Internet address. */
    //        struct in_addr {
    //            uint32_t       s_addr;     /* 32 位 IPv4 地址（网络字节序 )*/
    //        };
/////////////////////////////////////////////////////////////////////////   
    struct msg_channel_st* msg_channel;

/*  初始化
*   级别：默认值<配置文件<环境变量<命令行参数*/
    //分析命令行
    while(1){
        c = getopt_long(argc,argv,"P:M:p:H",argarr,&index);
        if (c < 0)
            break;
        switch (c)
        {
            case 'P':
                client_conf.rcvport = optarg;
                break;
            case 'M':
                client_conf.mulgroup = optarg;
                break;
            case 'p':
                client_conf.player_cmd = optarg;
                break;
            case 'H':
                printfhelp();//要求打印帮助信息
                exit(0);
                break;
            default:
                abort();
                break;
        }
    }
    //创建网络套接字
    sd = socket(AF_INET,SOCK_DGRAM,0);
    if (sd < 0){
        perror("socket() is wrong");
        exit(-1);
    }

    //设置套接字选项

    //int inet_pton(int af, const char *src, void *dst);
    inet_pton(AF_INET,client_conf.mulgroup,&mreq.imr_multiaddr);
    inet_pton(AF_INET,"0.0.0.0",&mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex("eth33");

    if ((setsockopt(sd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq))) < 0){
        perror("setsocketopt is wrong");
        exit(-1);
    }

    val = 1;
    if (setsockopt(sd,IPPROTO_IP,IP_MULTICAST_LOOP,&val,sizeof(val)) < 0){
        perror("setsockopt is wrong");
        exit(-1);
    }

    //绑定本地的IP地址,inet_pton()将IP地址转换为网络字节序，就是转换为大端字节序
    localaddr.sin_family = AF_INET;
    localaddr.sin_port = htons(atoi(client_conf.rcvport));
    inet_pton(AF_INET,"0.0.0.0",&localaddr.sin_addr);

    if (bind(sd,(void*)&localaddr,sizeof(localaddr)) < 0){
        perror("bind is wrong");
        exit(-1);
    }



    // fflush(NULL);
    if (pipe(pd) < 0){
        perror("pipe is wrong:");
        exit(-1);
    }

    pid = fork();//并且一定是先pipe创建管道；然后再fork，这样父子进程才可以进行通信。
    if (pid < 0){
        perror("fork is wrong:");
        exit(-1);
    }

    //子进程：调用解码器只读，即使用pd[0];
    if (pid == 0){
        //子进程继承父进程的 fd：
        // 所以子进程里要关闭不用的 fd（比如网络套接字sd、管道写端pd[1]）
        close(sd);
        close(pd[1]);
        dup2(pd[0],0);//把标准输入（0）重定向到管道读端pd[0]
        if (pd[0] > 0){
            close(pd[0]);//dup2后，0 已经代替了 pd[0] 指向管道读端，pd[0] 没用了所以要关掉；判断 >0 是为了保险，避免误关标准输入。
        }
        execl("/bin/sh","sh","-c",client_conf.player_cmd,NULL);//利用shell来解析播放器指令
        perror("execl()");//因为execl函数成功没有返回值，因此不需要判断，只有出错了才可以走到这里
        exit(-1);
    }
    
    //父进程：从网络上收包，发送给子进程
    else{
        //收节目单
        struct msg_list_st* msg_list;
        
        
        
        msg_list = malloc(MSG_LIST_MAX);//给节目单包分配一块最大的内存
        if (msg_list == NULL){
            perror("malloc()");
            exit(-1);
        }

        //UDP核心接收数据函数
        while(1){
            len = recvfrom(sd,msg_list,MSG_LIST_MAX,0,(void*)&serveraddr,&serveraddr_len);//接收返回的字节，代表了msg_list的大小。
            if (len < sizeof(struct msg_list_st)){
                fprintf(stderr,"message is too small.\n");
                continue;
            }
            if (msg_list -> chnid != LISTCHNID){
                fprintf(stderr,"chnid is not match.\n");
                continue;
            }
            break;
    
        }

        //打印节目单并选择频道
        struct msg_listentry_st* pos;
        //这个for循环看不懂，请帮我解答
        for (pos = msg_list ->entry;(char*)pos < (((char*)msg_list) + len) ;pos = (void*)(((char*)pos) + ntohs(pos->len)))//为什么转成指针类型来计算位置？转来转去的？
        {
            printf("channel: %d : %s\n",pos->chnid,pos->desc);
        }

        free(msg_list);/////
        
        puts("plese input chnid:");
        while(1){
            ret = scanf("%d",&chosen_id);//输入你选择的ID
            if (ret != 1){
                exit(1);
            }
            break;// 成功读取后，跳出循环继续往下执行！
        }
            
        //收频道包，发送给子进程
        fprintf(stdout,"chosenid = %d\n",ret);

        msg_channel = malloc(MSG_CHANNEL_MAX);
        if (msg_channel == NULL){
            perror("msg_channel malloc is wrong");
            exit(-1);
        }
        
        while (1)
        {
            len2 = recvfrom(sd,msg_channel,MSG_CHANNEL_MAX,0,(void*)&remoteaddr,&remoteaddr_len);
            //如果地址或者端口不匹配，报错并继续收
            if (remoteaddr.sin_addr.s_addr != serveraddr.sin_addr.s_addr || remoteaddr.sin_port != serveraddr.sin_port){
                fprintf(stderr,"Ignore:add isn't match.\n");
                continue;//
            }

            //如果包太小也出错，继续收
            if (len2 < sizeof(struct msg_channel_st)){
                fprintf(stderr,"Ignore:msg_channel_st is too small.\n");
                continue;
            }

            //写入管道
            if (msg_channel->chnid == chosen_id){
                fprintf(stdout,"accept msg:%d\n",msg_channel->chnid);
                if (writen(pd[1],(const char*)msg_channel->data,len2 - sizeof(chnid_t)) < 0)//为什么不用fwrite?,第二个问题，为什么不能用.来引出结构体成员，必须使用->?
                    exit(-1);
            
            }



        }
        
    }
    free(msg_channel);
    close(sd);
    exit(0);
}