﻿/* 用于生成教室战点观测的分钟数据 */
//  ./idc1/bin/crtsurfdata4 ./idc1/ini/stcode.ini ./idc1/log/demo4.log ./idc1/data csv
#include "_public.h"

//增加生成历史数据文件的功能，为压缩文件和清理文件模块准备历史数据文件
//怎么加信号处理函数，处理2和15信号
//解决调用exit函数退出，局部对象没有调用析构函数的问题
//把心跳信息写入共享内存


//教室号缩写   测控仪器号  测控仪器型号 楼号   楼层  楼层高度
//安徽, 58015, 砀山, 34.27, 116.2, 44.2
//安徽, 58102, 亳州, 33.47, 115.44, 39.1

CPActive PActive;   // 进程心跳。

// 教室站点参数结构体。
struct st_stcode
{
	char classname[31]; // 教室号缩写
	char ocid[11];    // 测控仪器号
	char ocname[31];  // 测控仪器型号
	double lat;        // 楼号
	double lon;        // 楼层
	double height;     // 楼层高度
};

vector<struct st_stcode> vstcode; // 存放教室站点参数的容器。

// 把站点参数文件中加载到vstcode容器中。
bool LoadSTCode(const char* inifile);

// 教室站点分钟观测数据结构
struct st_surfdata
{
	char classname[31];      // 站点代码。
	char ddatetime[21];  // 数据时间：格式yyyymmddhh24miss
	int  t;              // 气温：单位，0.1摄氏度。
	int  p;              // 气压：0.1百帕。
	int  u;              // 相对湿度，0-100之间的值。
	int  wd;             // 空调风向，0-360之间的值。
	int  wf;             // 空调风速：单位0.1m/s
	int  r;              // 教室人数
	int  vis;            // 体感温度
};

vector<struct st_surfdata> vsurfdata;  // 存放教室站点分钟观测数据的容器

char strddatetime[21]; // 观测数据的时间。

// 模拟生成教室站点分钟观测数据，存放在vsurfdata容器中。
void CrtSurfData();

CFile File;  // 文件操作对象。

// 把容器vstcode中的教室站点数据写入文件。
bool CrtSTFile(const char* outpath, const char* datafmt);

// 把容器vsurfdata中的教室站点分钟观测数据写入文件。
bool CrtSurfFile(const char* outpath, const char* datafmt);

CLogFile logfile;    // 日志类。

void EXIT(int sig);  // 程序退出和信号2、15的处理函数。

int main(int argc, char* argv[])
{
	if ((argc != 5) && (argc != 6))
	{
		// 如果参数非法，给出帮助文档。
		printf("Using:./crtsurfdata inifile outpath logfile datafmt [datetime]\n");
		printf("Example:/project/idc1/bin/crtsurfdata /project/idc1/ini/stcode.ini /tmp/idc/surfdata /log/idc/crtsurfdata.log xml,json,csv\n");
		printf("        /project/idc1/bin/crtsurfdata /project/idc1/ini/stcode.ini /tmp/idc/surfdata /log/idc/crtsurfdata.log xml,json,csv 20210710123000\n");
		printf("        ./procctl.out 60 /usr/project/Project_win/crtsurfdata/bin/x64/Debug/crtsurfdata.out /usr/project/Project_win/data/ini/stcode.ini /usr/project/Project_win/data/bin /usr/project/Project_win/log/crtsurfdata.log xml\n\n\n");

		printf("inifile  教室站点参数文件名。\n");
		printf("outpath  教室站点数据文件存放的目录。\n");
		printf("logfile  本程序运行的日志文件名。\n");
		printf("datafmt  生成数据文件的格式，支持xml、json和csv三种格式，中间用逗号分隔。\n");
		printf("datetime 这是一个可选参数，表示生成指定时间的数据和文件。\n\n\n");

		return -1;
	}

	// 关闭全部的信号和输入输出。
	// 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
	// 但请不要用 "kill -9 +进程号" 强行终止。
	CloseIOAndSignal(); signal(SIGINT, EXIT);  signal(SIGTERM, EXIT);

	// 打开程序的日志文件。
	if (logfile.Open(argv[3], "a+", false) == false)
	{
		printf("logfile.Open(%s) failed.\n", argv[3]); return -1;
	}

	logfile.Write("crtsurfdata 开始运行。\n");

	PActive.AddPInfo(20, "crtsurfdata");

	// 把站点参数文件中加载到vstcode容器中。 
	if (LoadSTCode(argv[1]) == false) return -1;

	// 获取当前时间，当作观测时间。
	memset(strddatetime, 0, sizeof(strddatetime));
	if (argc == 5)
		LocalTime(strddatetime, "yyyymmddhh24miss");
	else
		STRCPY(strddatetime, sizeof(strddatetime), argv[5]);

	// 模拟生成教室站点分钟观测数据，存放在vsurfdata容器中。
	CrtSurfData();

	// 把容器vsurfdata中的教室站点分钟观测数据写入文件。
	if (strstr(argv[4], "xml") != 0) { CrtSTFile(argv[2], "xml"); CrtSurfFile(argv[2], "xml"); }
	if (strstr(argv[4], "json") != 0) { CrtSTFile(argv[2], "json"); CrtSurfFile(argv[2], "json"); }
	if (strstr(argv[4], "csv") != 0) { CrtSTFile(argv[2], "csv"); CrtSurfFile(argv[2], "csv"); }

	logfile.Write("crtsurfdata 运行结束。\n");

	return 0;
}

// 把站点参数文件中加载到vstcode容器中。 
bool LoadSTCode(const char* inifile)
{
	// 打开站点参数文件。
	if (File.Open(inifile, "r") == false)
	{
		logfile.Write("File.Open(%s) failed.\n", inifile); return false;
	}

	char strBuffer[301];

	CCmdStr CmdStr;

	struct st_stcode stcode;

	while (true)
	{
		// 从站点参数文件中读取一行，如果已读取完，跳出循环。
		if (File.Fgets(strBuffer, 300, true) == false) break;

		// 把读取到的一行拆分。
		CmdStr.SplitToCmd(strBuffer, ",", true);

		if (CmdStr.CmdCount() != 6) continue;     // 扔掉无效的行。

		// 把站点参数的每个数据项保存到站点参数结构体中。
		memset(&stcode, 0, sizeof(struct st_stcode));
		CmdStr.GetValue(0, stcode.classname, 30); // 教室号缩写
		CmdStr.GetValue(1, stcode.ocid, 10);    // 测控仪器号
		CmdStr.GetValue(2, stcode.ocname, 30);  // 测控仪器型号
		CmdStr.GetValue(3, &stcode.lat);         // 楼号
		CmdStr.GetValue(4, &stcode.lon);         // 楼层
		CmdStr.GetValue(5, &stcode.height);      // 楼层高度

		// 把站点参数结构体放入站点参数容器。
		vstcode.push_back(stcode);
	}

	/*
	for (int ii=0;ii<vstcode.size();ii++)
	  logfile.Write("classname=%s,ocid=%s,ocname=%s,lat=%.2f,lon=%.2f,height=%.2f\n",\
					 vstcode[ii].classname,vstcode[ii].ocid,vstcode[ii].ocname,vstcode[ii].lat,\
					 vstcode[ii].lon,vstcode[ii].height);
	*/

	return true;
}

// 把容器vstcode中的教室站点分钟观测数据写入文件。
bool CrtSTFile(const char* outpath, const char* datafmt)
{
	// 拼接生成数据的文件名，例如：/tmp/idc/surfdata/ST_ZH_20210629092200_2254.csv
	char strFileName[301];
	sprintf(strFileName, "%s/ST_%s_%d.%s", outpath, strddatetime, getpid(), datafmt);

	// 打开文件。
	if (File.OpenForRename(strFileName, "w") == false)
	{
		logfile.Write("File.OpenForRename(%s) failed.\n", strFileName); return false;
	}

	if (strcmp(datafmt, "csv") == 0) File.Fprintf("教室号缩写,测控仪器号,测控仪器型号,楼号,楼层,楼层高度\n");
	if (strcmp(datafmt, "xml") == 0) File.Fprintf("<Station>\n");
	if (strcmp(datafmt, "json") == 0) File.Fprintf("{\"station\":[\n");

	// 遍历存放观测数据的vsurfdata容器。
	for (int ii = 0; ii < vsurfdata.size(); ii++)
	{
		// 写入一条记录。 %s,%s,%s,%.2f,%.2f,%.2f 
		if (strcmp(datafmt, "csv") == 0)
			File.Fprintf("%s,%s,%s,%.2f,%.2f,%.2f\n", \
				vstcode[ii].classname, vstcode[ii].ocid, vstcode[ii].ocname, vstcode[ii].lat, \
				vstcode[ii].lon, vstcode[ii].height);

		if (strcmp(datafmt, "xml") == 0)
			File.Fprintf("<classname>%s</classname><ocid>%s</ocid><ocname>%s</ocname><lat>%.2f</lat>"\
				"<lon>%.2f</lon><height>%.2f</height><endl/>\n", \
				vstcode[ii].classname, vstcode[ii].ocid, vstcode[ii].ocname, vstcode[ii].lat, \
				vstcode[ii].lon, vstcode[ii].height);

		if (strcmp(datafmt, "json") == 0)
		{
			File.Fprintf("{\"classname\":\"%s\",\"ocid\":\"%s\",\"ocname\":\"%s\",\"lat\":\"%.2f\","\
				"\"lon\":\"%.2f\",\"height\":\"%.2f\"}", \
				vstcode[ii].classname, vstcode[ii].ocid, vstcode[ii].ocname, vstcode[ii].lat, \
				vstcode[ii].lon, vstcode[ii].height);

			if (ii < vstcode.size() - 1) File.Fprintf(",\n");
			else   File.Fprintf("\n");
		}
	}

	if (strcmp(datafmt, "xml") == 0) File.Fprintf("</Station>\n");
	if (strcmp(datafmt, "json") == 0) File.Fprintf("]}\n");

	// 关闭文件。
	File.CloseAndRename();

	logfile.Write("生成站点数据文件%s成功，数据时间%s，记录数%d。\n", strFileName, strddatetime, vstcode.size());

	return true;
}


// 模拟生成教室站点分钟观测数据，存放在vsurfdata容器中。
void CrtSurfData()
{
	// 播随机数种子。
	srand(time(0));

	struct st_surfdata stsurfdata;

	// 遍历气象站点参数的vstcode容器。
	for (int ii = 0; ii < vstcode.size(); ii++)
	{
		memset(&stsurfdata, 0, sizeof(struct st_surfdata));

		// 用随机数填充分钟观测数据的结构体。
		strncpy(stsurfdata.ocid, vstcode[ii].ocid, 10); // 站点代码。
		strncpy(stsurfdata.ddatetime, strddatetime, 14);  // 数据时间：格式yyyymmddhh24miss
		stsurfdata.t = rand() % 351;       // 气温：单位，0.1摄氏度
		stsurfdata.p = rand() % 265 + 10000; // 气压：0.1百帕
		stsurfdata.u = rand() % 100 + 1;     // 相对湿度，0-100之间的值。
		stsurfdata.wd = rand() % 360;      // 空调风向，0-360之间的值。
		stsurfdata.wf = rand() % 150;      // 风速：单位0.1m/s
		stsurfdata.r = rand() % 16;        // 教室人数：0.1mm
		stsurfdata.vis = rand() % 5001 + 100000;  // 体感温度：0.1米

		// 把观测数据的结构体放入vsurfdata容器。
		vsurfdata.push_back(stsurfdata);
	}
}

// 把容器vsurfdata中的教室站点分钟观测数据写入文件。
bool CrtSurfFile(const char* outpath, const char* datafmt)
{
	// 拼接生成数据的文件名，例如：/tmp/idc/surfdata/SURF_ZH_20210629092200_2254.csv
	char strFileName[301];
	sprintf(strFileName, "%s/SURF_ZH_%s_%d.%s", outpath, strddatetime, getpid(), datafmt);

	// 打开文件。
	if (File.OpenForRename(strFileName, "w") == false)
	{
		logfile.Write("File.OpenForRename(%s) failed.\n", strFileName); return false;
	}

	if (strcmp(datafmt, "csv") == 0) File.Fprintf("站点代码,数据时间,气温,气压,相对湿度,空调风向,风速,教室人数,体感温度\n");
	if (strcmp(datafmt, "xml") == 0) File.Fprintf("<data>\n");
	if (strcmp(datafmt, "json") == 0) File.Fprintf("{\"data\":[\n");

	// 遍历存放观测数据的vsurfdata容器。
	for (int ii = 0; ii < vsurfdata.size(); ii++)
	{
		// 写入一条记录。
		if (strcmp(datafmt, "csv") == 0)
			File.Fprintf("%s,%s,%.1f,%.1f,%d,%d,%.1f,%.1f,%.1f\n", \
				vsurfdata[ii].ocid, vsurfdata[ii].ddatetime, vsurfdata[ii].t / 10.0, vsurfdata[ii].p / 10.0, \
				vsurfdata[ii].u, vsurfdata[ii].wd, vsurfdata[ii].wf / 10.0, vsurfdata[ii].r / 10.0, vsurfdata[ii].vis / 10.0);

		if (strcmp(datafmt, "xml") == 0)
			File.Fprintf("<ocid>%s</ocid><ddatetime>%s</ddatetime><t>%.1f</t><p>%.1f</p>"\
				"<u>%d</u><wd>%d</wd><wf>%.1f</wf><r>%.1f</r><vis>%.1f</vis><endl/>\n", \
				vsurfdata[ii].ocid, vsurfdata[ii].ddatetime, vsurfdata[ii].t / 10.0, vsurfdata[ii].p / 10.0, \
				vsurfdata[ii].u, vsurfdata[ii].wd, vsurfdata[ii].wf / 10.0, vsurfdata[ii].r / 10.0, vsurfdata[ii].vis / 10.0);

		if (strcmp(datafmt, "json") == 0)
		{
			File.Fprintf("{\"ocid\":\"%s\",\"ddatetime\":\"%s\",\"t\":\"%.1f\",\"p\":\"%.1f\","\
				"\"u\":\"%d\",\"wd\":\"%d\",\"wf\":\"%.1f\",\"r\":\"%.1f\",\"vis\":\"%.1f\"}", \
				vsurfdata[ii].ocid, vsurfdata[ii].ddatetime, vsurfdata[ii].t / 10.0, vsurfdata[ii].p / 10.0, \
				vsurfdata[ii].u, vsurfdata[ii].wd, vsurfdata[ii].wf / 10.0, vsurfdata[ii].r / 10.0, vsurfdata[ii].vis / 10.0);

			if (ii < vsurfdata.size() - 1) File.Fprintf(",\n");
			else   File.Fprintf("\n");
		}
	}

	if (strcmp(datafmt, "xml") == 0) File.Fprintf("</data>\n");
	if (strcmp(datafmt, "json") == 0) File.Fprintf("]}\n");

	// 关闭文件。
	File.CloseAndRename();

	UTime(strFileName, strddatetime);  // 修改文件的时间属性。

	logfile.Write("生成数据文件%s成功，数据时间%s，记录数%d。\n", strFileName, strddatetime, vsurfdata.size());

	return true;
}

// 程序退出和信号2、15的处理函数。
void EXIT(int sig)
{
	logfile.Write("程序退出，sig=%d\n\n", sig);

	exit(0);
}
