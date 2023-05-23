//守护进程-心跳机制，检测进程是否异常结束
//1、服务程序在共享内存中维护自己的心跳信息
//2、开发守护程序，中止已经死机的服务程序

#include"_public.h"
#define MAXNUMP_ 1000 //最大进程数量。
#define SHMKEP_ 0x5095 //共享内存的key

// 进程心跳信息的结构
struct st_pinfo
{
	int pid; //进程id
	char pname[51]; //进程名称，可为空
	int timeout; //超时时间，单位：秒
	time_t atime; //最后一次心跳的时间，用整数表示
};

int main(int argc, char* argv[])
{
	if (argc < 2) { printf("缺少参数\n"); return 0; }

	//创建/获取共享内存，大小为n*sizeof(struct st_pinfo)
	int m_shmid = 0;
	if ((m_shmid = shmget(SHMKEP_, MAXNUMP_ * sizeof(struct st_pinfo), 0640 | IPC_CREAT)) == -1)
	{
		printf("共享内存%x创建失败\n", SHMKEP_); return -1;
	}
	//将共享内存连接到当前进程的地址空间
	struct st_pinfo* m_shm;
	m_shm = (struct st_pinfo*)shmat(m_shmid, 0, 0);

	//创建当前进程心跳信息结构体变量，把本进程的信息填进去
	struct st_pinfo* stpinfo = (struct st_pinfo*)malloc(sizeof(st_pinfo));
	memset(stpinfo, 0, sizeof(struct st_pinfo));
	stpinfo->pid = getpid();                                      //进程id
	STRNCPY(stpinfo->pname, sizeof(stpinfo->pname), argv[1], 50);  //进程名称
	stpinfo->timeout = 30;                                        //超时时间
	stpinfo->atime = time(0);                                      //最后一次心跳时间

	int m_pos = -1;
	//在共享内存中查找一空位置，把当前进程的心跳信息存入共享内存中。
	for (int i = 0; i < MAXNUMP_; i++)
	{
		if (m_shm[i].pid == 0)
		{
			m_pos = i;
			break;
		}
	}

	if (m_pos == -1)
	{
		printf("共享内存空间已用完。\n");
		return -1;
	}

	memcpy(m_shm + m_pos, stpinfo, sizeof(struct st_pinfo));
	free(stpinfo);
	//更新共享内存中本进程的心跳时间
	while (true)
	{
		//更新共享内存中本进程的心跳时间
		sleep(10);
	}
	//把当前进程从共享内存中移去

	//把共享内存从当前进程中分离。

	return 0;
}

