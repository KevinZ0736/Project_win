#include"_public.h"

//程序运行的日志
CLogFile logfile;

int main(int argc, char* argv[])
{
	//程序的帮助
	if (argc != 2)
	{
		printf("\n");
		printf("参数：./checkproc logfilename\n 例如：./procctl.out 10 ./checkproc.out /tmp/log/checkproc.log\n\n");
		printf("本程序用于检查后台服务程序是否超时，如果已超时就终止他\n注意：\n 1)本程序由procctl启动，运行周期建议10秒，建议以root用户启动\n\n");
		return 0;
	}

	//忽略全部信号和IO
	CloseIOAndSignal();

	//打开日志文件
	if (logfile.Open(argv[1], "a+") == false)
	{
		printf("logfile open(%s) failed\n", argv[1]);
		return -1;
	}
	int shmid = 0;

	//创建/获取共享内存
	if ((shmid = shmget((key_t)SHMKEYP, MAXNUMP * sizeof(struct st_procinfo), 0666 | IPC_CREAT)) == -1)
	{
		printf("共享内存%x创建失败\n", SHMKEYP); return false;
	}
	//将共享内存连接到当前进程的地址空间
	struct st_procinfo* shm = (struct st_procinfo*)shmat(shmid, 0, 0);
	//遍历共享内存中的全部记录
	for (int i = 0; i < MAXNUMP; i++)
	{
		//如果进程记录为0，表示空记录，不为0，则记录服务程序的心跳记录
		if (shm[i].pid == 0) continue;

		logfile.Write("i=%d,pid=%d,pname=%s,timeout=%d,atime=%d\n", \
			i, shm[i].pid, shm[i].pname, shm[i].timeout, shm[i].atime);

		// 向进程发送信号0，判断它是否还存在，如果不存在，从共享内存中删除该记录
		/*如果返回值为0，表示进程存在且可以向其发送信号。
			如果返回值为 - 1，表示出现了错误，可以通过检查 errno 来获取具体的错误信息。
			如果返回值为其他正整数，表示进程存在，但当前进程没有权限向其发送信号。*/
		int iret = kill(shm[i].pid, 0);
		if (iret == -1)
		{
			logfile.Write("进程pid=%d(%s)已经不存在。\n", shm[i].pid, shm[i].pname);
			memset(shm + i, 0, sizeof(struct st_procinfo));//从共享内存中删除该纪录
			continue;
		}

		time_t now = time(0); //取当前时间

		//未超时
		if (now - shm[i].atime < shm[i].timeout) continue;
		//已超时
		logfile.Write("进程pid=%d(%s)已经超时。\n", shm[i].pid, shm[i].pname);

		//保存要清除进程id,与进程名
		char tmpname[51];
		int tmpid = shm[i].pid;
		STRNCPY(tmpname, sizeof(shm[i].pname), shm[i].pname, 50);

		//发送信号15，尝试正常终止进程
		kill(shm[i].pid, 15);

		//每隔一秒判断进程是否存在，累计5秒
		for (int j = 0; j < 5; j++)
		{
			sleep(1);
			iret = kill(tmpid, 0);
			logfile.Write("%d pid=%d(%s)\n", iret, tmpid, tmpname);
			if (iret == -1)
			{
				break;
			}
		}
		if (iret == -1)
		{
			logfile.Write("进程pid=%d(%s)已经正常终止。\n", tmpid, tmpname);
		}
		else
		{
			kill(shm[i].pid, 9);  //如果仍存在，发送信号9，终止它
			logfile.Write("进程pid=%d(%s)已经强制终止。\n", tmpid, tmpname);
		}

		// 从共享内存中删除已超时进程的心跳记录。
		memset(shm + i, 0, sizeof(struct st_procinfo)); // 从共享内存中删除该记录。

	}

	//把共享内存从当前进程中分离
	shmdt(shm);
	return 0;
}