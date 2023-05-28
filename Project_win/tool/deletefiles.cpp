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
		printf("例如 ./tool/procctl.out 300 ./tool/deletefiles.out ./idc1/data \"*.csv.*\" 0.02\n");
	}
	//关闭全部的信号和输入输出
	//CloseIOAndSignal(true);
    signal(SIGINT, EXIT);  signal(SIGTERM, EXIT);

    // 获取文件超时的时间点。
    char strTimeOut[21];
    LocalTime(strTimeOut, "yyyy-mm-dd hh24:mi:ss", 0 - (int)(atof(argv[3]) * 24 * 60 * 60));

    CDir Dir;
    // 打开目录，CDir.OpenDir()
    if (Dir.OpenDir(argv[1], argv[2], 10000, true) == false)
    {
        printf("Dir.OpenDir(%s) failed.\n", argv[1]); return -1;
    }

    // 遍历目录中的文件名。
    while (true)
    {
        // 得到一个文件的信息，CDir.ReadDir()
        if (Dir.ReadDir() == false) break;
        printf("=%s=\n", Dir.m_FullFileName);
        // 与超时的时间点比较，如果更早，就需要删除。
        if (strcmp(Dir.m_ModifyTime, strTimeOut) < 0)
        {
            if (REMOVE(Dir.m_FullFileName) == true)
                printf("REMOVE %s ok.\n", Dir.m_FullFileName);
            else
                printf("REMOVE %s failed.\n", Dir.m_FullFileName);
        }
    }

    return 0;
}
