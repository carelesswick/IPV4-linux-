#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "server_conf.h"
#include "medialib.h"
#include "memory.h"
#include "thr_channel.h"
#include "thr_list.h"
#include <proto.h>

struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,\
										.mulgroup = DEFAULT_MGROUP,\
										.media_dir = DEFAULT_MEDIADIR,\
										.infcname = DEFAULT_INFCNAME,\
										.runmode = RUN_DAEMON};

int sever_sd;
struct sockaddr_in sndaddr;//发送方的地址（man 7 ip）
static void printfhelp()
{
	printf("-M	指定接收端口\n\
            -P  指定多播组\n\
            -F  指定播放器\n\
            -D  显示帮助\n\
			-I 	指定网络设设备\n\
			-H	显示帮助\n");

}

//创建守护进程函数（一般就是这种固定的套路，在笔记中有提到）
static int daemonize(void)
{
	pid_t pid;
	int fd;
	pid = fork();
	if (pid < 0){
		// perror("fork()");
		syslog(LOG_ERR,"fork():%s",strerror(errno));//不能写\n,函数会自动换行
		return -1;
	}
	if (pid>0){
		exit(0);//父进程退出，子进程变为孤儿进程(一定不是组长进程)，后续再将其变为守护进程
	}
	//继续往下执行代码的，百分之百只能是子进程（pid == 0）写不写 else 都一样，省略 else 可以减少代码的缩进层级
	fd = open("/dev/null",0,O_RDWR);//打开黑洞设备，后续将守护进程的0、1、2重新定向到此，防止出现I/O错误。
	if (fd < 0){
		// perror("open()");
		syslog(LOG_WARNING,"open():%s",strerror(errno));
		return -2;
	}
	else{
		dup2(fd,0);
		dup2(fd,1);
		dup2(fd,2);
		if (fd > 2){
			close(fd);//防御性编程
		}
	}
	
	setsid();//创建守护进程
	chdir("/");//将脱离任务终端的守护进程指定到一个合适的路径上去
	umask(0);//umask将自身 umask 修改为 0，也就是创建文件的基础权限为 0777，方便后续基于此继续修改权限
	return 0;

}

static void daemon_exit(int s)
{
	/*待补充*/
	closelog();//关闭日志

	exit(0);
}

static int socket_init(void)
{
	struct ip_mreqn mreq;
	sever_sd = socket(AF_INET,SOCK_DGRAM,0);
	if (sever_sd < 0){
		syslog(LOG_ERR,"socket():%s",strerror(errno));
		exit(-1);
	}

	inet_pton(AF_INET,server_conf.mulgroup,&mreq.imr_multiaddr);//inet_pton是直接将字符串转换为二进制网络字节序
	inet_pton(AF_INET,"0.0.0.0",mreq.imr_address);
	mreq.imr_ifindex = if_nametoindex(server_conf.infcname);//网卡转索引号

	if (setsockopt(sever_sd,IPPROTO_IP,IP_MULTICAST_IF,&mreq,sizeof(mreq)) < 0){
		syslog(LOG_ERR,"setsockopt(IP_MULTICAST_IF):%s",strerror(errno));
		exit(-1);
	}
	//bind()省略
	sndaddr.sin_family = AF_INET;
	sndaddr.sin_port = htons(atoi((server_conf.rcvport)));//主机字节序转网络字节序（字符串转整数）
	inet_pton(AF_INET,server_conf.mulgroup,sndaddr.sin_addr.s_addr);//字符串IP→ 网络字节序二进制IP
	return 0;
}

int main (int argc,char **argv)
{
    int opt;
	//sigaction结构体，主要包含信号处理函数sa_handler，阻塞信号sa_mask，标志位sa_flags
    struct sigaction sa;
	//调用信号
	sa.sa_handler = daemon_exit;
	sigemptyset(&sa.sa_mask);//清空阻塞（处理函数是需要暂时屏蔽）的信号
	sigaddset(&sa.sa_mask,SIGINT);//将下面三个信号添加到sa_mask中
	sigaddset(&sa.sa_mask,SIGQUIT);
	sigaddset(&sa.sa_mask,SIGTERM);
	/*这里的 sa.sa_mask 意思是：
	当 daemon_exit 正在执行时，如果这时候又收到了 SIGINT, SIGQUIT, SIGTERM，
	系统会先把它们阻塞住，等 daemon_exit 执行完了再处理。这防止了处理函数被其他信号中断。*/

	//当这些信号来临时，会执行daemon_exit函数安全退出。
	sigaction(SIGTERM,&sa,NULL);// 处理终止信号(15)
	sigaction(SIGINT,&sa,NULL);// 处理中断信号(2)，如Ctrl+C
	sigaction(SIGQUIT,&sa,NULL);// 处理退出信号(3)，如Ctrl+\

	//系统日志
	openlog("netradio",LOG_PID | LOG_PERROR,LOG_DAEMON);//打开日志文件

	//该结构体用来存储默认的配置信息
	// struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,\
	// 									.mulgroup = DEFAULT_MGROUP,\
	// 									.media_dir = DEFAULT_MEDIADIR,\
	// 									.infcname = DEFAULT_INFCNAME,\
	// 									.runmode = RUN_DAEMON};
   
	while(1){
		opt = getopt(argc,argv,"M:P:FD:I:H");//因为每次getopt每次只解析一个参数，所以需要循环解析
		if (opt < 0)
			break;//解析到-1说明解析完毕
		switch (opt)
		{
			case 'M':
				server_conf.mulgroup = optarg;
				break;
			case 'P':
				server_conf.rcvport = optarg;
				break;
			case 'F':
				server_conf.runmode = RUN_FRONTDESK;//默认是后台守护进程，传递F参数就表示改为前台运行
				break;
			case 'D':
				server_conf.media_dir = optarg;
				break;
			case 'I':
				server_conf.infcname = optarg;
				break;
			case 'H':
				printfhelp();
				exit(0);//为什么加一个退出呢？因为显示帮助信息通常是一个独立的功能，用户只需要查看帮助信息，不需要程序继续执行其他初始化操作
				break;
			default:
				abort();
				break;
		}
	}
    
    /*如果当前进程作为守护进程*/
	if (server_conf.runmode == RUN_DAEMON){
		if (daemonize() < 0){
			exit(-1);//
		}
	}
	else if (server_conf.runmode == RUN_FRONTDESK){
		/*do nothing*/
	}
	else{
		// fprintf(stderr,"runmode invalid\n");
		syslog(LOG_ERR,"severconf_st.runmode invalid:");
		exit(-1);
	}

    /*SOCKET初始化*/ //？？还需要了解复习UDP通信的原理，比如服务器（主动）端和客户（被动）端的通信流程等 (老师为什么说客户端不可以省略bind，而服务器端可以省略bind)
	socket_init();

    /*获取频道信息(应该从medialib中获取)*/
	struct medialib_entry_st *list;
	int list_size;
	int err;
	err = medialib_getchalist(&list,&list_size);
	if (err){
		syslog(LOG_ERR,"medialib_getchalist():%s.",strerror(errno));
		exit(-1);
	}
    /*创建节目单线程(thr_list)*/
	err = thr_list_create(list,list_size);
	if (err){
		exit(-1);
	}

    /*创建频道线程(thr_channel)*/
	int i;
	for(i = 0;i < list_size;i++){
		err = thr_channel_create(list+i);
		if (err){
			fprintf(stderr,"thr_channel_create():%s\n",strerror(err));
			exit(-1);
		}
	}
	syslog(LOG_DEBUG,"%d channel threads created.",i);
    //在干嘛？
    while(1)
        pause();

	//但下面的执行不到
	
    exit(0);
}