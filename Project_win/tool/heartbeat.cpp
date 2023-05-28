//守护进程-心跳机制，检测进程是否异常结束
//1、服务程序在共享内存中维护自己的心跳信息
//2、开发守护程序，中止已经死机的服务程序

#include"_public.h"


void EXIT(int sig)
{
	printf("收到信号 sig=%d\n", sig);
	exit(0);//exit函数不会调用局部对象的析构函数，会调用全局对象的析构函数，return会调用局部和全局对象的析构函数
}

CPActive Active;

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("参数不够,例如./heartbeat procname timeout\n"); return 0;
	}

	signal(2, EXIT); signal(15, EXIT);//设置2和15的信号


	Active.AddPInfo(atoi(argv[2]), argv[1]);

	while (true)
	{

		//Active.UptATime();

		sleep(10);
	}

	return 0;

}

