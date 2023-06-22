#include "ConnectionPool.h"

ConnectionPool* ConnectionPool::getConnectPool()
{
	static ConnectionPool pool;//由于静态成员是与类关联而不是与类的实例关联的，因此只需在类中进行一次声明。
	return &pool;
}

ConnectionPool::ConnectionPool()
{
	// 打开日志文件。
	if (DBlogfile.Open("Database.log", "a+") == false)
	{
		printf("打开日志文件失败（%s）。\n", "Database.log");
	}
	//加载配置文件
	if (!LoadConfig())
	{
		return;
	}

	for (int x = 0; x < m_maxSize; x++)
	{
		MysqlConn* conn = new MysqlConn;
		conn->connect(m_user, m_passwd, m_dbName, m_ip, m_port);
		m_connectionQ.push(conn);
	}
	thread producer(&ConnectionPool::produceConnection, this);  //生产者线程
	thread recycler(&ConnectionPool::recycleConnection, this); //消费者线程

	producer.detach();  //调用detach()成员函数分离子线程，子线程退出时，系统将自动回收资源。
	recycler.detach();
}

bool ConnectionPool::LoadConfig()
{
	ifstream Conf;
	Conf.open("DBconfig.xml", ios::in);
	if (Conf.is_open() == false)
	{
		DBlogfile.Write("配置文件打开失败(%s)。\n", "DBconfig.xml");
		return false;
	}
	string buffer, config;
	while (getline(Conf, buffer))
	{
		config = config + buffer;
	}
	const char* conf = config.c_str();
	if ((GetXMLBuffer(conf, "ip", m_ip)) &&
		(GetXMLBuffer(conf, "port", &m_port)) &&
		(GetXMLBuffer(conf, "userName", m_user)) &&
		(GetXMLBuffer(conf, "password", m_passwd)) &&
		(GetXMLBuffer(conf, "dbName", m_dbName)) &&
		(GetXMLBuffer(conf, "minSize", &m_minSize)) &&
		(GetXMLBuffer(conf, "maxSize", &m_maxSize)) &&
		(GetXMLBuffer(conf, "maxIdleTime", &m_maxIdleTime)) == false)
	{
		DBlogfile.Write("参数读取失败(%s)。\n", "DBconfig.xml");
		return false;
	}
	return true;
}

void ConnectionPool::produceConnection()
{
}

void ConnectionPool::recycleConnection()
{
}
