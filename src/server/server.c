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





#include "server_conf.h"
#include "medialib.h"
#include "memory.h"
#include "thr_channel.h"
#include "thr_list.h"
#include <proto.h>

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
	umask(0);
	return 0;

}

static void daemon_exit(int s)
{
	/*待补充*/
	closelog();//关闭日志

	exit(0);
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

	//当这些信号来临时，会执行daemon_exit函数安全退出。
	sigaction(SIGTERM,&sa,NULL);// 处理终止信号(15)
	sigaction(SIGINT,&sa,NULL);// 处理中断信号(2)，如Ctrl+C
	sigaction(SIGQUIT,&sa,NULL);// 处理退出信号(3)，如Ctrl+\

	//系统日志
	openlog("netradio",LOG_PID | LOG_PERROR,LOG_DAEMON);//打开日志文件


	struct server_conf_st server_conf = {.rcvport = DEFAULT_RCVPORT,\
										.mulgroup = DEFAULT_MGROUP,\
										.media_dir = DEFAULT_MEDIADIR,\
										.infcname = DEFAULT_INFCNAME,\
										.runmode = RUN_DAEMON};
   
	while(1){
		opt = getopt(argc,argv,"M:P:FD:I:H");//因为每次getopt每次只解析一个参数，所以呢要循环解析
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
				server_conf.runmode = RUN_FRONTDESK;//默认是后台守护进程，现在呢改为前台
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
    
    /*当前进程作为守护进程（需要查阅资料）*/
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

    /*SOCKET初始化*/

    /*获取频道信息*/

    /*创建节目单线程*/

    /*创建频道线程*/

    //在干嘛？
    while(1)
        pause();

	//但下面的执行不到
	
    exit(0);
}