#include"_public.h"
#include"_ftp.h"
#include<fstream>

CLogFile logfile;

Cftp ftp;

struct st_arg
{
	char host[31];           // 远程服务端的IP和端口。
	int  mode;               // 传输模式，1-被动模式，2-主动模式，缺省采用被动模式。
	char username[31];       // 远程服务端ftp的用户名。
	char password[31];       // 远程服务端ftp的密码。
	char remotepath[301];    // 远程服务端存放文件的目录。
	char localpath[301];     // 本地文件存放的目录。
	char matchname[101];     // 待下载文件匹配的规则。
	char listfilename[301];  // 下载前列出服务端文件名的文件。
	int  ptype;              // 下载后服务端文件的处理方式：1-什么也不做；2-删除；3-备份。
	char remotepathbak[301]; // 下载后服务端文件的备份目录。

} starg;

struct st_fileinfo
{
	char filename[301];      //文件名
	char mtime[21];          //文件时间
};

vector<struct st_fileinfo> vlistfile;    //存放下载前，列出服务器文件名的窗口

//程序退出和信号2、15的处理函数
void EXIT(int sig);
void Help();
bool XmlToarg(const char* strxmlbuffer);  //把xml解析到参数starg结构中。
bool FtpGetFiles();                 //下载文件功能的主函数
bool LoadListFile();                //把ftp.nlist()方法获取到的list文件加载到容器vfilelist中
const char* LoadConfig();           //加载配置文件

int main(int argc, char* argv[])
{
	// 第一步计划，把服务器上某目录的文件全部下载到本地目录（可以指定文件名的匹配规则）
	if (argc != 2)
	{
		Help(); return -1;
	}

	//处理信号函数
	//CloseIOAndSignal(true);
	signal(SIGINT, EXIT);  signal(SIGTERM, EXIT);

	//打开日志文件
	if (logfile.Open(argv[1], "a+") == false)
	{
		printf("logfile open(%s) failed\n", argv[1]);
		return -1;
	}

	//解析xml,得到程序运行的参数
	if (XmlToarg(LoadConfig()) == false)
		return -1;

	//登录ftp服务器
	if (ftp.login(starg.host, starg.username, starg.password, starg.mode) == false)
	{
		logfile.Write("ftp.login(%s,%s,%s) failed.\n", starg.host, starg.username, starg.password); return -1;
	}
	logfile.Write("ftp.login ok.\n");  // 正式运行后，可以注释这行代码。

	FtpGetFiles();

	ftp.logout();

	return 0;
}

void Help()
{
	printf("\n");
	printf("参数：日志文件名 参数缓冲区\n");

	printf("Sample:/project/tools1/bin/procctl 30 Project_win/FTP/obj/x64/Debug/ftpgetfiles.out /log/idc/ftpgetfiles_surfdata.log \"<host>124.222.175.81:21</host><mode>1</mode><username>KevinZ</username><password>19980408</password><localpath>/usr/project/Project_win/FTP/data</localpath><remotepath>/usr/project/Project_win/idc/bin/x64/Debug/idc1/data</remotepath><matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname><listfilename>/usr/project/Project_win/FTP/data/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename><ptype>1</ptype><remotepathbak>/tmp/idc/surfdatabak</remotepathbak><okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename><checkmtime>true</checkmtime><timeout>80</timeout><pname>ftpgetfiles_surfdata</pname>\"\n\n\n");

	printf("本程序是通用的功能模块，用于把远程ftp服务端的文件下载到本地目录。\n");
	printf("logfilename是本程序运行的日志文件。\n");
	printf("xmlbuffer为文件下载的参数，如下：\n");
	printf("<host>124.222.175.81:21</host> 远程服务端的IP和端口。\n");
	printf("<mode>1</mode> 传输模式，1-被动模式，2-主动模式，缺省采用被动模式。\n");
	printf("<username>KevinZ</username> 远程服务端ftp的用户名。\n");
	printf("<password>19980408</password> 远程服务端ftp的密码。\n");
	printf("<remotepath>/usr/project/Project_win/idc/bin/x64/Debug/idc1/data</remotepath> 远程服务端存放文件的目录。\n");
	printf("<localpath>/usr/project/Project_win/FTP/data</localpath> 本地文件存放的目录。\n");
	printf("<matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname> 待下载文件匹配的规则。"\
		"不匹配的文件不会被下载，本字段尽可能设置精确，不建议用*匹配全部的文件。\n");
	printf("<listfilename>/usr/project/Project_win/FTP/data/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename> 下载前列出服务端文件名的文件。\n");
	printf("<ptype>1</ptype> 文件下载成功后，远程服务端文件的处理方式：1-什么也不做；2-删除；3-备份，如果为3，还要指定备份的目录。\n");
	printf("<remotepathbak>/tmp/idc/surfdatabak</remotepathbak> 文件下载成功后，服务端文件的备份目录，此参数只有当ptype=3时才有效。\n");
	printf("<okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename> 已下载成功文件名清单，此参数只有当ptype=1时才有效。\n");
	printf("<checkmtime>true</checkmtime> 是否需要检查服务端文件的时间，true-需要，false-不需要，此参数只有当ptype=1时才有效，缺省为false。\n");
	printf("<timeout>80</timeout> 下载文件超时时间，单位：秒，视文件大小和网络带宽而定。\n");
	printf("<pname>ftpgetfiles_surfdata</pname> 进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n\n");
}

bool XmlToarg(const char* strxmlbuffer)
{
	memset(&starg, 0, sizeof(struct st_arg));

	GetXMLBuffer(strxmlbuffer, "host", starg.host, 30);   // 远程服务端的IP和端口。
	if (strlen(starg.host) == 0)
	{
		logfile.Write("host is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "mode", &starg.mode);   // 传输模式，1-被动模式，2-主动模式，缺省采用被动模式。
	if (starg.mode != 2)  starg.mode = 1;

	GetXMLBuffer(strxmlbuffer, "username", starg.username, 30);   // 远程服务端ftp的用户名。
	if (strlen(starg.username) == 0)
	{
		logfile.Write("username is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "password", starg.password, 30);   // 远程服务端ftp的密码。
	if (strlen(starg.password) == 0)
	{
		logfile.Write("password is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "remotepath", starg.remotepath, 300);   // 远程服务端存放文件的目录。
	if (strlen(starg.remotepath) == 0)
	{
		logfile.Write("remotepath is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "localpath", starg.localpath, 300);   // 本地文件存放的目录。
	if (strlen(starg.localpath) == 0)
	{
		logfile.Write("localpath is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "matchname", starg.matchname, 100);   // 待下载文件匹配的规则。
	if (strlen(starg.matchname) == 0)
	{
		logfile.Write("matchname is null.\n");  return false;
	}

	GetXMLBuffer(strxmlbuffer, "listfilename", starg.listfilename, 300);   // 下载前,列出服务端文件名的文件。
	if (strlen(starg.listfilename) == 0)
	{
		logfile.Write("listfilename is null.\n");  return false;
	}

	// 下载后服务端文件的处理方式：1-什么也不做；2-删除；3-备份。
	GetXMLBuffer(strxmlbuffer, "ptype", &starg.ptype);
	if ((starg.ptype != 1) && (starg.ptype != 2) && (starg.ptype != 3))
	{
		logfile.Write("ptype is error.\n"); return false;
	}

	GetXMLBuffer(strxmlbuffer, "remotepathbak", starg.remotepathbak, 300); // 下载后服务端文件的备份目录。
	if ((starg.ptype == 3) && (strlen(starg.remotepathbak) == 0))
	{
		logfile.Write("remotepathbak is null.\n");  return false;
	}

	return true;
}

bool FtpGetFiles()
{
	//进入ftp服务器存放文件的目录
	if (ftp.chdir(starg.remotepath) == false)
	{
		logfile.Write("ftp.chdir(%s) failed.\n", starg.remotepath);
		return false;
	}
	//调用ftp.nlist()方法列出服务器目录的文件，结果存放到本地文件中。
	if (ftp.nlist(".", starg.listfilename) == false)
	{
		logfile.Write("ftp.nlist(%s) failed.\n,starg.remotepath");
		return false;
	}
	//把ftp.nlist()方法获取到的list文件加载到容器vfilelist中。
	if (LoadListFile() == false)
	{
		logfile.Write("LoadListFile() failed.\n");
		return false;
	}
	char strremotefilename[301], strlocalfilename[301];//远程文件的地址和本地文件的地址
	//遍历容器vfilelist
	for (auto x : vlistfile)
	{
		SNPRINTF(strremotefilename, sizeof(strremotefilename), 300, "%s/%s", starg.remotepath, x.filename);
		SNPRINTF(strlocalfilename, sizeof(strlocalfilename), 300, "%s/%s", starg.localpath, x.filename);

		// 调用ftp.get()方法从服务器下载文件
		logfile.Write("get %s ...", strremotefilename);
		if (ftp.get(strremotefilename, strlocalfilename) == false)
		{
			logfile.WriteEx("failed.\n"); return false;
		}
		logfile.WriteEx("ok\n");

		//删除文件
		if (starg.ptype == 2)
		{
			if (ftp.ftpdelete(strremotefilename) == false)
			{
				logfile.Write("ftp.ftpdelete(%s) failed.\n", strremotefilename);
			}
		}
		//转存到备份目录，远程目录不会自动创建，受限权限问题
		if (starg.ptype == 3)
		{
			char strremotefilenamebak[301];
			SNPRINTF(strremotefilenamebak, sizeof(strremotefilenamebak), 300, "%s/%s", starg.remotepathbak, x.filename);
			if (ftp.ftprename(strremotefilename, strremotefilenamebak) == false)
			{
				logfile.Write("ftp.ftprename(%s,%s) failed.\n", strremotefilename, strremotefilenamebak);
				return false;
			}
		}
	}

}

bool LoadListFile()
{
	vlistfile.clear();

	CFile File;

	if (File.Open(starg.listfilename, "r") == false)
	{
		logfile.Write("File.Open(%s) 失败。\n", starg.listfilename);
		return false;
	}

	struct st_fileinfo stfileinfo;

	while (true)
	{
		memset(&stfileinfo, 0, sizeof(struct st_fileinfo));

		if (File.Fgets(stfileinfo.filename, 300, true) == false) break;

		if (MatchStr(stfileinfo.filename, starg.matchname) == false) continue;

		vlistfile.push_back(stfileinfo);
	}

	/*for (auto x : vlistfile)
	{
		logfile.Write("filename=%s=\n", x.filename);
	}*/
	return true;
}

//读取配置文件
const char* LoadConfig()
{
	ifstream Conf;
	Conf.open("config.txt", ios::in);
	if (Conf.is_open() == false)
	{
		logfile.Write("config open failed.\n");
	}
	string buffer, config;
	while (getline(Conf, buffer))
	{
		config = config + buffer;
	}
	const char* conf = config.c_str();
	return conf;
}

void EXIT(int sig)
{
	printf("程序退出，sig=%d\n\n", sig);
	exit(0);
}