#include"_public.h"
/*压缩历史文件，或者过时文件，节省空间*/

void EXIT(int sig)
{
	printf("收到信号 sig=%d\n", sig);
	exit(0);//exit函数不会调用局部对象的析构函数，会调用全局对象的析构函数，return会调用局部和全局对象的析构函数
}



int main(int argc, char* argv[])
{
	//程序帮助
	if (argc != 4)
	{
		printf("命令格式 ./gzipfiles.out pathname matchstr timeout(day)\n\n");
		printf("例如 ./gzipfiles.out ./log/idc \"*.log,*.json\" 0.02\n");
	}
	//关闭全部的信号和输入输出
	//CloseIOAndSignal(true);
	signal(SIGINT, EXIT); signal(SIGTERM, EXIT);//设置2和15的信号

	//获取文件超时的时间点
	char strTimeOut[21];
	LocalTime(strTimeOut, "yyyy-mm-dd hh24:mi:ss", 0 - (int)(atof(argv[3]) * 24 * 60 * 60));

	CDir Dir;
	//遍历目录中的文件名
	/*打开目录*/
	if (Dir.OpenDir(argv[1], argv[2], 10000, true) == false)
	{
		printf("Dir.OpenDir(%s) 失败\n", argv[1]); return -1;
	}
	char strCmd[1024]; //存放gzip压缩文件的命令

	while (true)
	{
		//得到一个文件的信息，CDir.ReadDir()
		if (Dir.ReadDir() == false) break;
		//printf("DirName=%s,FileName=%s,ModifyTime=%s\n", Dir.m_DirName, Dir.m_FileName, Dir.m_ModifyTime);
		printf("FullFileName=%s\n", Dir.m_FullFileName);

		//与超时的时间点比较，如果跟早，就需要压缩
		if ((strcmp(Dir.m_ModifyTime, strTimeOut) < 0) && (MatchStr(Dir.m_FileName, "*.gz") == false))
		{
			//压缩文件，调用操作系统的gzip命令
			SNPRINTF(strCmd, sizeof(strCmd), 1000, "/usr/bin/gzip -f %s 1>/dev/null 2>/dev/null", Dir.m_FullFileName);
			if (system(strCmd) == 0)
				printf("gzip %s ok.\n", Dir.m_FullFileName);
			else
				printf("gzip %s failed.\n", Dir.m_FullFileName);
		}
	}
	return 0;
}
