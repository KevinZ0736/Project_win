#pragma once
#include<queue>
#include<mutex>
#include<condition_variable>
#include<fstream>
#include<thread>
#include"_public.h"
#include"MysqlConn.h"
using namespace std;

class ConnectionPool
{
public:
	static ConnectionPool* getConnectPool();
	ConnectionPool(const ConnectionPool& obj) = delete;
	ConnectionPool& operator=(const ConnectionPool& obj) = delete;

private:
	ConnectionPool();
	bool LoadConfig();
	void produceConnection();  //生产数据库连接
	void recycleConnection();  //销毁数据库连接

	string m_ip;
	string m_user;
	string m_passwd;
	string m_dbName;
	int m_port;
	int m_minSize;  //连接池中的最小连接数
	int m_maxSize;  //连接池中的最大连接数
	int m_timeout;  //超时机制
	int m_maxIdleTime;  //最长空闲时间

	queue<MysqlConn*> m_connectionQ; //用于存放连接的容器
	mutex m_mutexQ;
	condition_variable m_cond;
	static CLogFile DBlogfile;
};

