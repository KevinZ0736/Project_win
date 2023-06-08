﻿#include "_public.h"
#include<fstream>

CTcpServer TcpServer;
// 程序运行的参数结构体。
struct st_arg
{
	int  clienttype;          // 客户端需求的服务类型，1-上传文件；2-下载文件。
	char ip[31];              // 服务端的IP地址。
	int  port;                // 服务端的端口。
	int  ptype;               // 文件成功传输后的处理方式：1-删除文件；2-移动到备份目录。
	char clientpath[301];     // 客户端文件存放的根目录。
	bool andchild;            // 是否传输各级子目录的文件，true-是；false-否。
	char matchname[301];      // 待传输文件名的匹配规则，如"*.TXT,*.XML"。
	char srvpath[301];        // 服务端文件存放的根目录。
	char srvpathbak[301];     // 服务端文件存放的根目录。
	int  timetvl;             // 扫描目录文件的时间间隔，单位：秒。
	int  timeout;             // 进程心跳的超时时间。
	char pname[51];           // 进程名，建议用"tcpgetfiles_后缀"的方式。
} starg;

struct ClientFile
{
	char clientfilename[301];
	char mtime[21];
	int  filesize = 0;
}clfile;

CLogFile logfile;

void FathEXIT(int sig);  // 父进程退出函数。
void ChldEXIT(int sig);  // 子进程退出函数。


bool XmlToArg(const char* strxmlbuffer);

const char* LoadConfig(const char* local);

char strrecvbuffer[1024];   // 发送报文的buffer。
char strsendbuffer[1024];   // 接收报文的buffer。

bool ClientLogin();     //登录业务处理函数

void RecvFilesMain();   //接收文件的主函数

// 接受上传文件的内容
bool RecvFile(const int sockfd, const char* filename, const char* mtime, int filesize);

CPActive PActive;

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("参数文件:./fileserver port logfile\n");
		return -1;
	}
	// 关闭全部的信号和输入输出。
  // 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程
  // 但请不要用 "kill -9 +进程号" 强行终止
	CloseIOAndSignal(); signal(SIGINT, FathEXIT); signal(SIGTERM, FathEXIT);

	if (logfile.Open(argv[2], "a+") == false) { printf("logfile.Open(%s) failed.\n", argv[2]); return -1; }

	// 服务端初始化。
	if (TcpServer.InitServer(atoi(argv[1])) == false)
	{
		logfile.Write("TcpServer.InitServer(%s) failed.\n", argv[1]); return -1;
	}
	while (true)
	{
		//等待客户端的请求
		if (TcpServer.Accept() == false)
		{
			logfile.Write("TcpServer.Accept() failed\n");
			FathEXIT(-1);
		}

		logfile.Write("客户端（%s）已连接。\n", TcpServer.GetIP());

		if (fork() > 0)
		{
			TcpServer.CloseClient(); continue;//父进程继续回到Accept()
		}
		// 子进程重新设置退出信号。
		signal(SIGINT, ChldEXIT); signal(SIGTERM, ChldEXIT);

		TcpServer.CloseListen();

		//处理登录客户端的登录报文
		if (ClientLogin() == false) ChldEXIT(-1);

		//如果clienttype==1,调用接收文件的主函数
		if (starg.clienttype == 1) RecvFilesMain();

		ChldEXIT(0);
	}
}

// 登录。
bool ClientLogin()
{
	memset(strrecvbuffer, 0, sizeof(strrecvbuffer));
	memset(strsendbuffer, 0, sizeof(strsendbuffer));

	if (TcpServer.Read(strrecvbuffer, 20) == false)
	{
		logfile.Write("TcpServer.Read() failed.\n"); return false;
	}
	logfile.Write("收到报文: %s\n", strrecvbuffer);

	//解析客户端登录报文
	XmlToArg(strrecvbuffer);

	if ((starg.clienttype != 1) && (starg.clienttype != 2))
		strcpy(strsendbuffer, "failed");
	else
		strcpy(strsendbuffer, "ok");

	if (TcpServer.Write(strsendbuffer) == false)
	{
		logfile.Write("TcpServer.Write() failed.\n"); return false;
	}

	logfile.Write("%s login %s.\n", TcpServer.GetIP(), strsendbuffer);

	return true;
}

//上传文件的主函数
void RecvFilesMain()
{
	PActive.AddPInfo(starg.timeout, starg.pname);

	while (true)
	{
		memset(strsendbuffer, 0, sizeof(strsendbuffer));
		memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

		PActive.UptATime();

		//接受客户端的报文
		//第二个参数的取值必须大于starg.timetvl.小于starg.timeout
		if (TcpServer.Read(strrecvbuffer, starg.timetvl + 10) == false)
		{
			logfile.Write("TcpServer.Read() failed.\n");
			return;
		}
		// logfile.Write("strrecvbuffer=%s\n",strrecvbuffer);

		//处理心跳报文
		if (strcmp(strrecvbuffer, "<activetest>ok</activetest>"))
		{
			strcpy(strsendbuffer, "ok");
			if (TcpServer.Write(strsendbuffer) == false)
			{
				logfile.Write("TcpServer.Write() failed.\n");
				return;
			}
		}

		//处理上传文件的请求报文
		if (strncmp(strrecvbuffer, "<filename>", 10) == 0)
		{
			// 解析上传文件请求的xml
			memset(&clfile, 0, sizeof(clfile));
			GetXMLBuffer(strrecvbuffer, "filename",clfile.clientfilename, 300);
			GetXMLBuffer(strrecvbuffer, "mtime", clfile.mtime, 19);
			GetXMLBuffer(strrecvbuffer, "size", &clfile.filesize);

			// 客户端和服务端文件的目录是不一样的，以下代码生成服务端的文件名。
	        // 把文件名中的clientpath替换成srvpath，要小心第三个参数
			char serverfilename[301];  memset(serverfilename, 0, sizeof(serverfilename));
			strcpy(serverfilename, clfile.clientfilename);
			UpdateStr(serverfilename, starg.clientpath, starg.srvpath, false);

			// 接收文件的内容。
			logfile.Write("recv %s(%d) ...", serverfilename, clfile.filesize);
			if (RecvFile(TcpServer.m_connfd, serverfilename, clfile.mtime, clfile.filesize) == true)
			{
				logfile.WriteEx("ok.\n");
				SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><result>ok</result>", clfile.clientfilename);
			}
			else
			{
				logfile.WriteEx("failed.\n");
				SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><result>failed</result>", clfile.clientfilename);
			}

			// 把接收结果返回给对端。
	        // logfile.Write("strsendbuffer=%s\n",strsendbuffer);
			if (TcpServer.Write(strsendbuffer) == false)
			{
				logfile.Write("TcpServer.Write() failed.\n"); return;
			}
		}

	}
}

//接受文件的内容
bool RecvFile(const int sockfd, const char* filename, const char* mtime, int filesize)
{
	//生成临时文件名
	char strfilenametmp[301];
	SNPRINTF(strfilenametmp, sizeof(strfilenametmp), 300, "%s.tmp", filename);

	int  totalbytes = 0;        // 已接收文件的总字节数。
	int  onread = 0;            // 本次打算接收的字节数。
	char buffer[1000];          // 接收文件内容的缓冲区。
	FILE* fp = NULL;

	// 创建临时文件。
	if ((fp = FOPEN(strfilenametmp, "wb")) == NULL) return false;

	while (true)
	{
		memset(buffer, 0, sizeof(buffer));

		// 计算本次应该接收的字节数。
		if (filesize - totalbytes > 1000) onread = 1000;
		else onread = filesize - totalbytes;

		// 接收文件内容。
		if (Readn(sockfd, buffer, onread) == false) { fclose(fp); return false; }

		// 把接收到的内容写入文件。
		fwrite(buffer, 1, onread, fp);

		// 计算已接收文件的总字节数，如果文件接收完，跳出循环。
		totalbytes = totalbytes + onread;

		if (totalbytes == filesize) break;
	}

	// 关闭临时文件。
	fclose(fp);

	// 重置文件的时间。
	UTime(strfilenametmp, mtime);

	// 把临时文件RENAME为正式的文件。
	if (RENAME(strfilenametmp, filename) == false) return false;

	return true;
}

// 把xml解析到参数starg结构中
bool XmlToArg(const char* strxmlbuffer)
{
	memset(&starg, 0, sizeof(struct st_arg));

	// 不需要对参数做合法性判断，客户端已经判断过了。
	GetXMLBuffer(strxmlbuffer, "clienttype", &starg.clienttype);
	GetXMLBuffer(strxmlbuffer, "ptype", &starg.ptype);
	GetXMLBuffer(strxmlbuffer, "clientpath", starg.clientpath);
	GetXMLBuffer(strxmlbuffer, "andchild", &starg.andchild);
	GetXMLBuffer(strxmlbuffer, "matchname", starg.matchname);
	GetXMLBuffer(strxmlbuffer, "srvpath", starg.srvpath);
	GetXMLBuffer(strxmlbuffer, "srvpathbak", starg.srvpathbak);

	GetXMLBuffer(strxmlbuffer, "timetvl", &starg.timetvl);
	if (starg.timetvl > 30) starg.timetvl = 30;

	GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);
	if (starg.timeout < 50) starg.timeout = 50;

	GetXMLBuffer(strxmlbuffer, "pname", starg.pname, 50);
	strcat(starg.pname, "_srv");

	return true;
}

const char* LoadConfig(const char* local)
{
	ifstream Conf;
	Conf.open(local, ios::in);
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
