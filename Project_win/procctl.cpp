#include"_public.h"

int main(int argc, char* argv[])
{
	//先执行fork函数，创建一个子进程,让子进程调用execv执行新的程序
	//新程序将完全替换子进程，不会影响父进程
	//在父进程中，可以调用wait函数等待新程序运行的结果，这样就可以实现调度的功能
	//./procctl 10 /usr/bin/ls -lt /tmp/project.tgz
	if (argc < 3)
	{
		printf("命令类似  ./procctl timetvl program argv ...\n Example:/project/tools1/bin/procctl 5 /usr/bin/ls -lt /tmp\n\n");
		printf("本程序是服务程序的调度程序，周期性启动服务程序或shell脚本。\n timetvl 运行周期，单位，秒，被调度程序运行结束后，在timevl秒后会被procctl重新启动。\n argvs 被调度的程序的参数。\n");
		printf("注意，本程序不会被kill杀死，但可以用kill -9强行杀死\n\n");

	}
	//关闭信号和IO,本程序不希望被打扰
	for (int i = 0; i < 64; i++)
	{
		signal(i, SIG_IGN);
		close(i);
	}

	//生成子进程，父进程退出，让程序运行在后台，由系统1号进程托管
	if (fork() != 0) exit(0);

	// 启用SIGCHLD,让父进程可以wait子进程退出的状态
	signal(SIGCHLD, SIG_DFL);

	char* pargv[argc];
	for (int i = 2; i < argc; i++)
		pargv[i - 2] = argv[i];

	pargv[argc - 2] = NULL;

	while (true)
	{
		if (fork() == 0)
		{
			execv(argv[2], pargv);
			exit(0);
		}
		else {
			int status = 0;
			wait(&status);
			sleep(atoi(argv[1]));
		}
	}
}