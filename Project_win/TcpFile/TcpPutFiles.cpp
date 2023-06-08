#include"_public.h"
#include<unordered_map>
#include<fstream>

CTcpClient TcpClient;

// 程序运行的参数结构体。
struct st_arg
{
	int  clienttype;          // 客户端类型，1-上传文件；2-下载文件。
	char ip[31];              // 服务端的IP地址。
	int  port;                // 服务端的端口。
	int  ptype;               // 文件上传成功后本地文件的处理方式：1-删除文件；2-移动到备份目录。
	char clientpath[301];     // 本地文件存放的根目录。
	char clientpathbak[301];  // 文件成功上传后，本地文件备份的根目录，当ptype==2时有效。
	bool andchild;            // 是否上传clientpath目录下各级子目录的文件，true-是；false-否。
	char matchname[301];      // 待上传文件名的匹配规则，如"*.TXT,*.XML"。
	char srvpath[301];        // 服务端文件存放的根目录。
	int  timetvl;             // 扫描本地目录文件的时间间隔，单位：秒。
	int  timeout;             // 进程心跳的超时时间。
	char pname[51];           // 进程名，建议用"tcpputfiles_后缀"的方式。
} starg;

CLogFile logfile;

// 程序退出和信号2、15的处理函数。
void EXIT(int sig);

void help();

bool HeartBeat(); //长连接心跳

bool Login(const char* local);     //登录

bool XmlToArg(const char* strxmlbuffer);

const char* LoadConfig(const char* local);

char strrecvbuffer[1024];   // 发送报文的buffer。
char strsendbuffer[1024];   // 接收报文的buffer。

unordered_map<char*, char*> FileMap;

bool TcpPutFiles();      // 文件上传的主函数，执行一次文件上传的任务。

// 把文件的内容发送给对端。
bool SendFile(const int sockfd, const char* filename, const int filesize);

bool bcontinue = true;   // 如果调用_tcpputfiles发送了文件，bcontinue为true，初始化为true。

// 删除或者转存本地的文件。
bool AckMessage(const char* strrecvbuffer);

// 文件下载的主函数。
void TcpGetFiles();

// 接收文件的内容。
bool RecvFile(const int sockfd, const char* filename, const char* mtime, int filesize);

CPActive PActive;  // 进程心跳。

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("缺少参数（运行文件,模式,参数文件,日志文件）\n");
		help();
	}
	CloseIOAndSignal();
	signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

	if (logfile.Open(argv[2], "a+") == false)
	{
		printf("打开日志文件失败(%s).\n", argv[2]); return -1;
	}

	if (XmlToArg(LoadConfig(argv[1])) == false) return -1;

	PActive.AddPInfo(starg.timeout, starg.pname);

	// 向服务端发起连接请求。
	if (TcpClient.ConnectToServer(starg.ip, starg.port) == false)
	{
		logfile.Write("TcpClient.ConnectToServer(%s,%d) failed.\n", starg.ip, starg.port); EXIT(-1);
	}
	if (strcmp(argv[1], "1"))
	{
		if (Login(LoadConfig(argv[1]), 1) == false)
		{
			logfile.Write("Login() failed.\n"); EXIT(-1);
		}
	}
	else if (strcmp(argv[1], "2"))
	{
		if (Login(LoadConfig(argv[1]), 2) == false)
		{
			logfile.Write("Login() failed.\n"); EXIT(-1);
		}
	}

	while (true)
	{
		if (strcmp(argv[1], "1"))
		{
			// 调用文件上传的主函数，执行一次文件上传的任务。
			if (TcpPutFiles() == false) { logfile.Write("TcpPutFiles() failed.\n"); EXIT(-1); }

			if (bcontinue == false)
			{
				sleep(starg.timetvl);

				if (HeartBeat() == false) break;
			}
		}
		else if (strcmp(argv[1], "2"))
		{
			// 调用文件下载的主函数。
			TcpGetFiles();

			sleep(starg.timetvl);

			if (HeartBeat() == false) break;
		}

		PActive.UptATime();
	}

	EXIT(0);

}

void help()
{
	printf("本程序是数据中心的公共功能模块，采用tcp协议把文件上传给服务端。\n");
	printf("logfilename   本程序运行的日志文件。\n");
	printf("xmlbuffer     本程序运行的参数，如下：\n");
	printf("ip            服务端的IP地址。\n");
	printf("port          服务端的端口。\n");
	printf("ptype         文件上传成功后的处理方式：1-删除文件；2-移动到备份目录。\n");
	printf("clientpath    本地文件存放的根目录。\n");
	printf("clientpathbak 文件成功上传后，本地文件备份的根目录，当ptype==2时有效。\n");
	printf("andchild      是否上传clientpath目录下各级子目录的文件，true-是；false-否，缺省为false。\n");
	printf("matchname     待上传文件名的匹配规则，如\"*.TXT,*.XML\"\n");
	printf("srvpath       服务端文件存放的根目录。\n");
	printf("timetvl       扫描本地目录文件的时间间隔，单位：秒，取值在1-30之间。\n");
	printf("timeout       本程序的超时时间，单位：秒，视文件大小和网络带宽而定，建议设置50以上。\n");
	printf("pname         进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。\n\n");
}

bool XmlToArg(const char* strxmlbuffer)
{
	memset(&starg, 0, sizeof(struct st_arg));

	GetXMLBuffer(strxmlbuffer, "ip", starg.ip);
	if (strlen(starg.ip) == 0) { logfile.Write("ip is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "port", &starg.port);
	if (starg.port == 0) { logfile.Write("port is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "ptype", &starg.ptype);
	if ((starg.ptype != 1) && (starg.ptype != 2)) { logfile.Write("ptype not in (1,2).\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "clientpath", starg.clientpath);
	if (strlen(starg.clientpath) == 0) { logfile.Write("clientpath is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "clientpathbak", starg.clientpathbak);
	if ((starg.ptype == 2) && (strlen(starg.clientpathbak) == 0)) { logfile.Write("clientpathbak is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "andchild", &starg.andchild);

	GetXMLBuffer(strxmlbuffer, "matchname", starg.matchname);
	if (strlen(starg.matchname) == 0) { logfile.Write("matchname is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "srvpath", starg.srvpath);
	if (strlen(starg.srvpath) == 0) { logfile.Write("srvpath is null.\n"); return false; }

	GetXMLBuffer(strxmlbuffer, "timetvl", &starg.timetvl);
	if (starg.timetvl == 0) { logfile.Write("timetvl is null.\n"); return false; }

	// 扫描本地目录文件的时间间隔，单位：秒。
	// starg.timetvl没有必要超过30秒。
	if (starg.timetvl > 30) starg.timetvl = 30;

	// 进程心跳的超时时间，一定要大于starg.timetvl，没有必要小于50秒。
	GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);
	if (starg.timeout == 0) { logfile.Write("timeout is null.\n"); return false; }
	if (starg.timeout < 50) starg.timeout = 50;

	GetXMLBuffer(strxmlbuffer, "pname", starg.pname, 50);
	if (strlen(starg.pname) == 0) { logfile.Write("pname is null.\n"); return false; }

	return true;
}

// 心跳。 
bool HeartBeat()
{
	memset(strsendbuffer, 0, sizeof(strsendbuffer));
	memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

	SPRINTF(strsendbuffer, sizeof(strsendbuffer), "<activetest>ok</activetest>");
	// logfile.Write("发送：%s\n",strsendbuffer);
	if (TcpClient.Write(strsendbuffer) == false) return false; // 向服务端发送请求报文。

	if (TcpClient.Read(strrecvbuffer, 20) == false) return false; // 接收服务端的回应报文。
	// logfile.Write("接收：%s\n",strrecvbuffer);

	return true;
}

bool Login(const char* local, int a)
{
	memset(strsendbuffer, 0, sizeof(strsendbuffer));
	memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

	SPRINTF(strsendbuffer, sizeof(strsendbuffer), "%s<clienttype>%d</clienttype>", local, a);
	logfile.Write("发送：%s\n", strsendbuffer);
	if (TcpClient.Write(strsendbuffer) == false) return false;//向服务端发送请求报文
	if (TcpClient.Read(strrecvbuffer, 20) == false) return false;//接收服务端的回应报文
	logfile.Write("接受：%s\n", strrecvbuffer);

	logfile.Write("登录(%s:%d)成功。\n", starg.ip, starg.port);

	return true;
}


// 文件上传的主函数，执行一次文件上传的任务。
bool TcpPutFiles()
{
	CDir Dir;

	// 调用OpenDir()打开starg.clientpath目录。
	if (Dir.OpenDir(starg.clientpath, starg.matchname, 10000, starg.andchild) == false)
	{
		logfile.Write("Dir.OpenDir(%s) 失败。\n", starg.clientpath); return false;
	}

	int delayed = 0;        // 未收到对端确认报文的文件数量。
	int buflen = 0;         // 用于存放strrecvbuffer的长度。

	bcontinue = false;

	while (true)
	{
		memset(strsendbuffer, 0, sizeof(strsendbuffer));
		memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

		// 遍历目录中的每个文件，调用ReadDir()获取<一个>文件名。
		if (Dir.ReadDir() == false) break;

		bcontinue = true;

		//判断文件是否是增量文件
		if ((FileMap.find(Dir.m_FileName) != FileMap.end()) && (strcmp(Dir.m_ModifyTime, FileMap[Dir.m_FileName]) == 0))
		{
			continue;
		}
		FileMap[Dir.m_FileName] = Dir.m_ModifyTime;

		// 把文件名、修改时间、文件大小组成报文，发送给对端。
		SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><mtime>%s</mtime><size>%d</size>", Dir.m_FileName, Dir.m_ModifyTime, Dir.m_FileSize);

		// logfile.Write("strsendbuffer=%s\n",strsendbuffer);
		if (TcpClient.Write(strsendbuffer) == false)
		{
			logfile.Write("TcpClient.Write() failed.\n"); return false;
		}

		// 把文件的内容发送给对端。
		logfile.Write("send %s(%d) ...", Dir.m_FullFileName, Dir.m_FileSize);
		if (SendFile(TcpClient.m_connfd, Dir.m_FullFileName, Dir.m_FileSize) == true)
		{
			logfile.WriteEx("ok.\n"); delayed++;
		}
		else
		{
			logfile.WriteEx("failed.\n"); TcpClient.Close(); return false;
		}

		PActive.UptATime();

		// 接收对端的确认报文。
		while (delayed > 0)
		{
			memset(strrecvbuffer, 0, sizeof(strrecvbuffer));
			// 最后参数为-1，表示不等待，立即判断socket的缓冲区中是否有数据，如果没有，返回false，如此便实现了发送与接收分离
			if (TcpRead(TcpClient.m_connfd, strrecvbuffer, &buflen, -1) == false) break;
			{ logfile.Write("strrecvbuffer=%s\n", strrecvbuffer); }

			// 删除或者转存本地的文件。
			delayed--;
			AckMessage(strrecvbuffer);
		}
	}

	// 继续接收对端的确认报文。
	while (delayed > 0)
	{
		memset(strrecvbuffer, 0, sizeof(strrecvbuffer));
		if (TcpRead(TcpClient.m_connfd, strrecvbuffer, &buflen, 10) == false) break;
		logfile.Write("strrecvbuffer=%s\n", strrecvbuffer);

		// 删除或者转存本地的文件。
		delayed--;
		AckMessage(strrecvbuffer);
	}

	return true;
}


// 把文件的内容发送给对端。
bool SendFile(const int sockfd, const char* filename, const int filesize)
{
	int  onread = 0;        // 每次调用fread时打算读取的字节数。 
	int  bytes = 0;         // 调用一次fread从文件中读取的字节数。
	char buffer[1000];    // 存放读取数据的buffer。
	int  totalbytes = 0;    // 从文件中已读取的字节总数。
	FILE* fp = NULL;

	// 以"rb"的模式打开文件。
	if ((fp = fopen(filename, "rb")) == NULL)  return false;

	while (true)
	{
		memset(buffer, 0, sizeof(buffer));

		// 计算本次应该读取的字节数，如果剩余的数据超过1000字节，就打算读1000字节。
		if (filesize - totalbytes > 1000) onread = 1000;
		else onread = filesize - totalbytes;

		// 从文件中读取数据。
		bytes = fread(buffer, 1, onread, fp);

		// 把读取到的数据发送给对端。
		if (bytes > 0)
		{
			if (Writen(sockfd, buffer, bytes) == false) { fclose(fp); return false; }
		}

		// 计算文件已读取的字节总数，如果文件已读完，跳出循环。
		totalbytes = totalbytes + bytes;

		if (totalbytes == filesize) break;
	}

	fclose(fp);

	return true;
}

// 删除或者转存本地的文件。
bool AckMessage(const char* strrecvbuffer)
{
	char filename[301];
	char result[11];

	memset(filename, 0, sizeof(filename));
	memset(result, 0, sizeof(result));

	GetXMLBuffer(strrecvbuffer, "filename", filename, 300);
	GetXMLBuffer(strrecvbuffer, "result", result, 10);

	// 如果服务端接收文件不成功，直接返回。
	if (strcmp(result, "ok") != 0) return true;

	// ptype==1，删除文件。
	if (starg.ptype == 1)
	{
		if (REMOVE(filename) == false) { logfile.Write("REMOVE(%s) failed.\n", filename); return false; }
	}

	// ptype==2，移动到备份目录。
	if (starg.ptype == 2)
	{
		// 生成转存后的备份目录文件名。
		char bakfilename[301];
		STRCPY(bakfilename, sizeof(bakfilename), filename);
		UpdateStr(bakfilename, starg.clientpath, starg.clientpathbak, false);
		if (RENAME(filename, bakfilename) == false)
		{
			logfile.Write("RENAME(%s,%s) failed.\n", filename, bakfilename); return false;
		}
	}

	return true;
}

// 文件下载的主函数。
void TcpGetFiles()
{
	PActive.AddPInfo(starg.timeout, starg.pname);

	while (true)
	{
		memset(strsendbuffer, 0, sizeof(strsendbuffer));
		memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

		PActive.UptATime();

		// 接收服务端的报文。
		// 第二个参数的取值必须大于starg.timetvl，小于starg.timeout。
		if (TcpClient.Read(strrecvbuffer, starg.timetvl + 10) == false)
		{
			logfile.Write("TcpClient.Read() failed.\n"); return;
		}
		// logfile.Write("strrecvbuffer=%s\n",strrecvbuffer);

		// 处理心跳报文。
		if (strcmp(strrecvbuffer, "<activetest>ok</activetest>") == 0)
		{
			strcpy(strsendbuffer, "ok");
			// logfile.Write("strsendbuffer=%s\n",strsendbuffer);
			if (TcpClient.Write(strsendbuffer) == false)
			{
				logfile.Write("TcpClient.Write() failed.\n"); return;
			}
		}

		// 处理下载文件的请求报文。
		if (strncmp(strrecvbuffer, "<filename>", 10) == 0)
		{
			// 解析下载文件请求报文的xml。
			char serverfilename[301];  memset(serverfilename, 0, sizeof(serverfilename));
			char mtime[21];            memset(mtime, 0, sizeof(mtime));
			int  filesize = 0;
			GetXMLBuffer(strrecvbuffer, "filename", serverfilename, 300);
			GetXMLBuffer(strrecvbuffer, "mtime", mtime, 19);
			GetXMLBuffer(strrecvbuffer, "size", &filesize);

			// 客户端和服务端文件的目录是不一样的，以下代码生成客户端的文件名。
			// 把文件名中的srvpath替换成clientpath，要小心第三个参数
			char clientfilename[301];  memset(clientfilename, 0, sizeof(clientfilename));
			strcpy(clientfilename, serverfilename);
			UpdateStr(clientfilename, starg.srvpath, starg.clientpath, false);

			// 接收文件的内容。
			logfile.Write("recv %s(%d) ...", clientfilename, filesize);
			if (RecvFile(TcpClient.m_connfd, clientfilename, mtime, filesize) == true)
			{
				logfile.WriteEx("ok.\n");
				SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><result>ok</result>", serverfilename);
			}
			else
			{
				logfile.WriteEx("failed.\n");
				SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><result>failed</result>", serverfilename);
			}

			// 把接收结果返回给对端。
			// logfile.Write("strsendbuffer=%s\n",strsendbuffer);
			if (TcpClient.Write(strsendbuffer) == false)
			{
				logfile.Write("TcpClient.Write() failed.\n"); return;
			}
		}
	}
}

// 接收文件的内容。
bool RecvFile(const int sockfd, const char* filename, const char* mtime, int filesize)
{
	// 生成临时文件名。
	char strfilenametmp[301];
	SNPRINTF(strfilenametmp, sizeof(strfilenametmp), 300, "%s.tmp", filename);

	int  totalbytes = 0;        // 已接收文件的总字节数。
	int  onread = 0;            // 本次打算接收的字节数。
	char buffer[1000];        // 接收文件内容的缓冲区。
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

