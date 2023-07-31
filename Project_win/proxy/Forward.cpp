#include"_public.h"
#include<unordered_map>
#include<fstream>

#define MAXSOCK 1024  // 最大的连接数

//代理路由参数的结构体
struct st_route
{
	int ListenPort;     // 本地监听的通讯端口
	char DstIP[31];     // 目标主机的IP地址
	int DstPort;        // 目标主机的通讯端口
	int ListenSock;     // 本地监听的socket
}route;

vector<struct st_route> V_Route;  // 代理路由的容器 

int epollfd = 0, timefd = 0;  // epoll与定时器的句柄。

unordered_map<int, int> UM_ClientSocks; // 存放每个socket连接对端的socket的值。
unordered_map<int, int> UM_ClientAcTime;  // 存放每个socket连接最后一次收发报文的时间。


// 把代理路由记载到容器中
bool LoadRoute(const char* inifile);

// 初始化服务端的监听端口
int InitServer(int port);

// 向目标IP和端口发起socket连接。
int ConnToDstIP(const char* ip, const int port);

void EXIT(int sig); // 进程退出函数

CLogFile logfile;

CPActive PActive;

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("\n");
		printf("Using :./inetd logfile inifile\n\n");
		printf("Sample:./inetd /tmp/inetd.log /etc/inetd.conf\n\n");
		printf("        /project/tools1/bin/procctl 5 /project/tools1/bin/inetd /tmp/inetd.log /etc/inetd.conf\n\n");
		return -1;
	}

	// 关闭全部的信号和输入输出。
	// 设置信号,在shell状态下可用 "kill + 进程号" 正常终止些进程。
	// 但请不要用 "kill -9 +进程号" 强行终止。
	CloseIOAndSignal(); signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

	// 打开日志文件。
	if (logfile.Open(argv[1], "a+") == false)
	{
		printf("打开日志文件失败（%s）。\n", argv[1]); return -1;
	}

	PActive.AddPInfo(30, "inetd");   // 设置进程的心跳时间为30秒

	// 把代理路由参数加载到vroute容器。
	if (LoadRoute(argv[2]) == false) return -1;

	logfile.Write("加载代理路由参数成功(%d)。\n", V_Route.size());

	// 创建epoll句柄
	epollfd = epoll_create(1); // 设置初始大小为1是为了节省内存空间。之后可以动态增加
	// 声明事件的数据结构。
	struct epoll_event ev;

	// 初始化服务端用于监听的socket。
	for (auto x : V_Route)
	{
		x.ListenSock = InitServer(x.ListenPort);
		if (x.ListenSock < 0)
		{
			logfile.Write("initserver(%d) failed.\n", x.ListenPort); EXIT(-1);
		}

		// 把监听socket设置为非阻塞。IO服用要使用非阻塞IO
		fcntl(x.ListenSock, F_SETFL, fcntl(x.ListenSock, F_GETFD, 0) | O_NONBLOCK);

		// 为监听的socket准备可读事件。
		ev.events = EPOLLIN;                 // 读事件
		ev.data.fd = x.ListenSock;           // 指定事件的自定义数据，会随着epoll_wait()返回的事件一并返回。
		epoll_ctl(epollfd, EPOLL_CTL_ADD, x.ListenSock, &ev); // 把监听的socket的事件加入epollfd中。

	}

	// 创建定时器。
	timefd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);  // 创建timerfd。

	struct itimerspec timeout;
	memset(&timeout, 0, sizeof(struct itimerspec));
	timeout.it_value.tv_sec = 20;                     // 超时时间为20秒
	timeout.it_value.tv_nsec = 0;
	timerfd_settime(timefd, 0, &timeout, NULL);       // 设置定时器

	// 为定时器准备事件
	ev.events = EPOLLIN | EPOLLET;    // 读事件，注意，一定要ET模式。
	ev.data.fd = timefd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, timefd, &ev);


	vector<struct epoll_event> evs(10);      // 存放epoll返回的事件。

	while (true)
	{
		// 等待监视的socket有事件发生。
		int infds = epoll_wait(epollfd, evs.data(), evs.size(), -1);

		// 返回失败
		if (infds < 0) { logfile.Write("epoll() failed.\n"); break; }

		// 遍历epoll返回的已发生事件的数组evs
		for (int i = 0; i < infds; i++)
		{
			// logfile.Write("events=%d,data.fd=%d\n",evs[ii].events,evs[ii].data.fd);

			////////////////////////////////////////////////////////
			// 如果定时器的时间已到，设置进程的心跳，清理空闲的客户端socket。
			if (evs[i].data.fd == timefd)
			{
				timerfd_settime(timefd, 0, &timeout, NULL);
				{
					timerfd_settime(timefd, 0, &timeout, NULL);  // 重新设置定时器。

					for (int j = 0; j < MAXSOCK; j++)
					{
						// 如果客户端socket空闲的时间超过80秒就关掉它。
						if ((ClientSocks[j] > 0) && ((time(0) - ClientAcTime[j]) > 80))
						{

						}

					}

				}
			}
			////////////////////////////////////////////////////////

			////////////////////////////////////////////////////////
			// 如果发生事件的是listensock，表示有新的客户端连上来。
			int j = 0;
			for (j = 0; j < V_Route.size(); j++)
			{
				if (evs[i].data.fd == V_Route[j].ListenSock)
				{
					// 接收客户端的连接
					struct sockaddr_in client;
					socklen_t len = sizeof(client);
					int ClientSock = accept(V_Route[j].ListenSock, (struct sockaddr*)&client, &len);

					if (ClientSock < 0) break;

					if (ClientSock >= MAXSOCK)
					{
						logfile.Write("连接数已超过最大值%d。\n", MAXSOCK); close(ClientSock); break;
					}

					// 向目标ip和端口发起socket连接。
					int DstSock = ConnToDstIP(V_Route[j].DstIP, V_Route[j].DstPort);
					if (DstSock < 0) break;
					if (DstSock >= MAXSOCK)
					{
						logfile.Write("连接数已超过最大值%d。\n", MAXSOCK); close(ClientSock); close(DstSock); break;
					}

					logfile.Write("accept on port %d client(%d,%d) ok。\n", V_Route[j].ListenPort, ClientSock, DstSock);

					// 为新连接的两个socket准备可读事件，并添加到epoll中。
					// 此为源端点连向本代理的socket
					ev.data.fd = ClientSock; ev.events = EPOLLIN;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, ClientSock, &ev);
					// 此为本代理连向目标端点的socket
					ev.data.fd = DstSock; ev.events = EPOLLIN;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, DstSock, &ev);

					// 更新clientsocks数组中两端soccket的值和活动时间
					UM_ClientSocks[ClientSock] = DstSock;
					UM_ClientSocks[DstSock] = ClientSock;

					UM_ClientAcTime[ClientSock] = timef()
				}
			}



		}

	}


}

bool LoadRoute(const char* inifile)
{
	ifstream Conf;
	Conf.open(inifile, ios::in);

	if (Conf.is_open() == false)
	{
		logfile.Write("打开代理路由参数文件(%s)失败。\n", inifile);
		return false;
	}
	string buffer;

	while (getline(Conf, buffer))
	{

		const char* conf = buffer.c_str();

		memset(&route, 0, sizeof(struct st_route));

		if (GetXMLBuffer(conf, "ListenPort", &route.ListenPort) == 0) { logfile.Write("ListenPort is null.\n"); return false; }

		GetXMLBuffer(conf, "DstIP", route.DstIP, 30);
		if (strlen(route.DstIP) == 0) { logfile.Write("DstIP is null.\n"); return false; }

		if (GetXMLBuffer(conf, "ListenSock", &route.ListenSock) == 0) { logfile.Write("ListenSock is null.\n"); return false; }

		if (GetXMLBuffer(conf, "DstPort", &route.DstPort) == 0) { logfile.Write("DstPort is null.\n"); return false; }

		V_Route.push_back(route);  // 

		buffer.clear(); // 清空缓存区
	}

	return true;
}

int InitServer(int port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);  // AF_INET IPV4 ,SOCK_STREAM TCP

	if (sock < 0)
	{
		perror("socket() failed"); return -1;
	}

	int opt = 1; unsigned int len = sizeof(opt);

	/*  sock：要设置选项的套接字的文件描述符。
		SOL_SOCKET：这是一个协议层级（Protocol Level）参数，表示设置的选项位于套接字层级。
		SO_REUSEADDR：这是要设置的选项名称。SO_REUSEADDR 是一个布尔类型的选项，用于告诉内核允许重用本地地址（local address）和端口，即允许在同一端口上快速重新启动服务器，而无需等待 TIME_WAIT 状态的释放。
		& opt：一个指向包含选项值的缓冲区的指针。在这里，opt 是一个整数，用于指示是否启用 SO_REUSEADDR 选项。
		len：opt 缓冲区的大小。*/
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, len);

	struct sockaddr_in servaddr;   // 定义一个 IPv4 地址结构体变量 servaddr，用于存储服务器的地址信息

	servaddr.sin_family = AF_INET;   // 设置地址族为 IPv4

	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   // 将地址设置为 INADDR_ANY，表示监听任意本地网络接口的连接请求

	servaddr.sin_port = htons(port);   // 设置端口号，使用 htons 函数将主机字节序转换为网络字节序

	if (bind(sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("bind() failed"); close(sock); return -1;
	}

	if (listen(sock, 5) != 0) //  指定套接字连接请求队列的长度，即在监听状态下等待处理的连接请求的最大数量,并非已连接的最大数量
	{
		perror("listen() failed"); close(sock); return -1;
	}

	return sock;
}
