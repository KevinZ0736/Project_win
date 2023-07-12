/* 用于生成全国气象战点观测的分钟数据 */
//  usr/project/Project_win/project/idc1/bin/crtsurfdata4 /usr/project/Project_win/project/idc1/ini/stcode.ini /usr/project/Project_win/project/idc1/log/crtsurfdata1.log
#include "_public.h"

CLogFile logfile;

int main(int argc, char* argv[])
{
	//参数文件，测试数据目录，程序运行日志
	if (argc != 4)
	{
		printf("请输入参数文件名，测试数据目录，程序运行日志\n\n");
		return -1;
	}

	if (logfile.Open(argv[3]) == false)
	{
		printf("日志文件打开失败\n", argv[3]);
		return -1;
	}

	logfile.Write("crtsurfdata1 开始运行。\n");
	//这里插入业务代码

	logfile.Write("crtsurfdata1 运行结束。\n");

	return 0;
}