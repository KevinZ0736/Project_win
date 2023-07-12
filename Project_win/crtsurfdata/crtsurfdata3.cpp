/* 用于生成全国气象战点观测的分钟数据 */
//  ./idc1/bin/crtsurfdata4 ./idc1/ini/stcode.ini ./idc1/log/crtsurfdata1.log
#include "_public.h"

//省   站号  站名 纬度   经度  海拔高度
//安徽, 58015, 砀山, 34.27, 116.2, 44.2
//安徽, 58102, 亳州, 33.47, 115.44, 39.1
struct st_stcode
{
	char provname[31];    //省
	char obtid[31];       //站号
	char obtname[31];     //站名
	double lat;           //纬度
	double lon;           //经度
	double height;        //海拔高度
};

struct st_surfdata
{
	char obtid[11];      // 站点代码。
	char ddatetime[21];  // 数据时间：格式yyyymmddhh24miss
	int  t;              // 气温：单位，0.1摄氏度。
	int  p;              // 气压：0.1百帕。
	int  u;              // 相对湿度，0-100之间的值。
	int  wd;             // 风向，0-360之间的值。
	int  wf;             // 风速：单位0.1m/s
	int  r;              // 降雨量：0.1mm。
	int  vis;            // 能见度：0.1米。
};

vector<struct st_surfdata> vsurfdata; // 存放全国气象站点分钟观测数据的容器
vector<struct  st_stcode> vstcode;   //存放站点信息
CLogFile logfile;
CCmdStr CmdStr;
struct st_stcode stcode;
void CrtSurfData();                  //模拟生成全国站点分钟观测数据，存放在vsurfdata容器中
bool LoadSTCode(const char* inifile);//加载站点信息

bool LoadSTCode(const char* inifile)
{
	CFile File;

	//打开站点参数文件
	if (File.Open(inifile, "r") == false)
	{
		logfile.Write("打开 %s 失败\n", inifile); return false;
	}

	char buffer[301];

	while (true)
	{
		//从站点参数文件，读取一行
		if (File.Fgets(buffer, 300, true) == false) break;
		//logfile.Write("=%s=\n", buffer);

		//拆分数据流
		CmdStr.SplitToCmd(buffer, ",", true);
		if (CmdStr.CmdCount() != 6)continue;//去掉无效的行

		CmdStr.GetValue(0, stcode.provname, 30);
		CmdStr.GetValue(1, stcode.obtid, 30);
		CmdStr.GetValue(2, stcode.obtname, 30);
		CmdStr.GetValue(3, &stcode.lat);
		CmdStr.GetValue(4, &stcode.lon);
		CmdStr.GetValue(5, &stcode.height);

		vstcode.push_back(stcode);
	}

	/*
	for (auto x : vstcode)
	{
		logfile.Write("prov:%s,obtid=%s,obtname=%s,lat=%.2f,lon=%.2f,height=%.2f\n", x.provname, x.obtid, x.obtname, x.lat, x.lon, x.height);
	}
	*/
	return true;
}

void CrtSurfData()
{
	//播随机数种子
	srand(time(0));

	char strddatetime[21];
	memset(strddatetime, 0, sizeof(strddatetime));
	LocalTime(strddatetime, "yyyymmddhh24miss");
	struct st_surfdata stsurfdata;

	for (auto x : vstcode)
	{
		memset(&stsurfdata, 0, sizeof(struct st_surfdata));

		//用随机数填充分钟观测数据的结构体
		strncpy(stsurfdata.obtid, x.obtid, 10);//站点代码
		strncpy(stsurfdata.ddatetime, strddatetime, 14);//数据时间
		stsurfdata.t = rand() % 351;                    //气温
		stsurfdata.p = rand() % 265 + 10000;            //气压
		stsurfdata.u = rand() % 100 + 1;                //相对湿度
		stsurfdata.wd = rand() % 360;                   //风向
		stsurfdata.wf = rand() % 150;                   //风速
		stsurfdata.r = rand() % 16;                     //降雨量
		stsurfdata.vis = rand() % 5001 + 100000;        //能见度

		vsurfdata.push_back(stsurfdata);
	}
}

int main(int argc, char* argv[])
{
	//参数文件，测试数据目录，程序运行日志
	if (argc != 4)
	{
		printf("请输入参数文件名，测试数据目录，程序运行日志\n\n");
		return -1;
	}

	if (logfile.Open(argv[3], "a+", false) == false)
	{
		printf("日志文件打开失败\n", argv[3]);
		return -1;
	}

	logfile.Write("demo2 开始运行。\n");

	//把站点参数文件加载到vstcode中
	if (LoadSTCode(argv[2]) == false) return -1;

	CrtSurfData();

	logfile.Write("demo2 运行结束。\n");

	return 0;
}