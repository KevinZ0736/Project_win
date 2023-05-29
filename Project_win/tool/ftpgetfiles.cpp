#include"_public.h"
#include"_ftp.h"

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
	char okfilename[301];    // 已下载成功文件名清单。
	bool checkmtime;         // 是否需要检查服务端文件的时间，true-需要，false-不需要，缺省为false。
	int  timeout;            // 进程心跳的超时时间。
	char pname[51];          // 进程名，建议用"ftpgetfiles_后缀"的方式。
} starg;

//程序退出和信号2、15的处理函数
void EXIT(int sig);
void Help();
bool XmlToarg(char* strxmlbuffer);

int main(int argc, char* argv[])
{
	// 第一步计划，把服务器上某目录的文件全部下载到本地目录（可以指定文件名的匹配规则）
	if (argc != 3)
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
	if (XmlToarg(argv[2]) == false)
		return -1;

	//登录ftp服务器

	//进入ftp服务器存放文件的目录

	//调用ftp.nlist()方法列出服务器目录的文件，结果存放到本地文件中。

	//把ftp.nlist()方法获取到的list文件加载到容器vfilelist中。

	//遍历容器vfilelist


	return 0;
}

void Help()
{
	printf("\n");
	printf("参数：日志文件名 参数缓冲区\n");

	printf("Sample:/project/tools1/bin/procctl 30 /project/tools1/bin/ftpgetfiles /log/idc/ftpgetfiles_surfdata.log \"<host>127.0.0.1:21</host><mode>1</mode><username>wucz</username><password>wuczpwd</password><localpath>/idcdata/surfdata</localpath><remotepath>/tmp/idc/surfdata</remotepath><matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname><listfilename>/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename><ptype>1</ptype><remotepathbak>/tmp/idc/surfdatabak</remotepathbak><okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename><checkmtime>true</checkmtime><timeout>80</timeout><pname>ftpgetfiles_surfdata</pname>\"\n\n\n");

	printf("本程序是通用的功能模块，用于把远程ftp服务端的文件下载到本地目录。\n");
	printf("logfilename是本程序运行的日志文件。\n");
	printf("xmlbuffer为文件下载的参数，如下：\n");
	printf("<host>127.0.0.1:21</host> 远程服务端的IP和端口。\n");
	printf("<mode>1</mode> 传输模式，1-被动模式，2-主动模式，缺省采用被动模式。\n");
	printf("<username>wucz</username> 远程服务端ftp的用户名。\n");
	printf("<password>wuczpwd</password> 远程服务端ftp的密码。\n");
	printf("<remotepath>/tmp/idc/surfdata</remotepath> 远程服务端存放文件的目录。\n");
	printf("<localpath>/idcdata/surfdata</localpath> 本地文件存放的目录。\n");
	printf("<matchname>SURF_ZH*.XML,SURF_ZH*.CSV</matchname> 待下载文件匹配的规则。"\
		"不匹配的文件不会被下载，本字段尽可能设置精确，不建议用*匹配全部的文件。\n");
	printf("<listfilename>/idcdata/ftplist/ftpgetfiles_surfdata.list</listfilename> 下载前列出服务端文件名的文件。\n");
	printf("<ptype>1</ptype> 文件下载成功后，远程服务端文件的处理方式：1-什么也不做；2-删除；3-备份，如果为3，还要指定备份的目录。\n");
	printf("<remotepathbak>/tmp/idc/surfdatabak</remotepathbak> 文件下载成功后，服务端文件的备份目录，此参数只有当ptype=3时才有效。\n");
	printf("<okfilename>/idcdata/ftplist/ftpgetfiles_surfdata.xml</okfilename> 已下载成功文件名清单，此参数只有当ptype=1时才有效。\n");
	printf("<checkmtime>true</checkmtime> 是否需要检查服务端文件的时间，true-需要，false-不需要，此参数只有当ptype=1时才有效，缺省为false。\n");
	printf("<timeout>80</timeout> 下载文件超时时间，单位：秒，视文件大小和网络带宽而定。\n");
	printf("<pname>ftpgetfiles_surfdata</pname> 进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n\n");
}

bool XmlToarg(char* strxmlbuffer)
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

	GetXMLBuffer(strxmlbuffer, "listfilename", starg.listfilename, 300);   // 下载前列出服务端文件名的文件。
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

	GetXMLBuffer(strxmlbuffer, "okfilename", starg.okfilename, 300); // 已下载成功文件名清单。
	if ((starg.ptype == 1) && (strlen(starg.okfilename) == 0))
	{
		logfile.Write("okfilename is null.\n");  return false;
	}

	// 是否需要检查服务端文件的时间，true-需要，false-不需要，此参数只有当ptype=1时才有效，缺省为false。
	GetXMLBuffer(strxmlbuffer, "checkmtime", &starg.checkmtime);

	GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);   // 进程心跳的超时时间。
	if (starg.timeout == 0) { logfile.Write("timeout is null.\n");  return false; }

	GetXMLBuffer(strxmlbuffer, "pname", starg.pname, 50);     // 进程名。
	if (strlen(starg.pname) == 0) { logfile.Write("pname is null.\n");  return false; }

	return true;
}

void EXIT(int sig)
{
	printf("程序退出，sig=%d\n\n", sig);
	exit(0);
}