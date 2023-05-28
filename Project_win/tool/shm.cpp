#include"_public.h"

CSEM sem;

struct st_pid
{
	int pid; //进程编号
	char pname[51]; //进程名字
};

int main(int argc, char* argv[])
{
	if (argc < 2) { printf("缺少参数\n"); return 0; }

	//共享内存的标志
	int shmid;

	if (sem.init(0x5005, 1, SEM_UNDO) == false)
	{
		printf("sem.init(0x5005) 失败\n");
	}
	//获取或者创建共享内存，键值为0x5005
	if ((shmid = shmget(0x5005, sizeof(struct st_pid), 0640 | IPC_CREAT)) == -1)
	{
		printf("shmget(0x5005) 失败\n"); return -1;
	}
	//用于指向共享内存的结构体变量
	struct st_pid* stpid = 0;
	if ((stpid = (struct st_pid*)shmat(shmid, 0, 0)) == (void*)-1)
	{
		printf("shmat 失败\n");
		return -1;
	}
	printf("1-time=%d,val=%d\n", time(0), sem.value());
	sem.P();
	printf("2-time=%d,val=%d\n", time(0), sem.value());
	printf("pid=%d,name=%s\n", stpid->pid, stpid->pname);
	stpid->pid = getpid();
	strcpy(stpid->pname, argv[1]);
	printf("pid=%d,name=%s\n", stpid->pid, stpid->pname);
	printf("3-time=%d,val=%d\n", time(0), sem.value());
	sem.V();
	printf("4-time=%d,val=%d\n", time(0), sem.value());
	//把共享内存从当前进程中分离。
	shmdt(stpid);
	return 0;
}

//ipcs -m 查看共享内存
//ipcrm -m 删除共享内存 ipcrm sem
//ipcs -s 查看信号量