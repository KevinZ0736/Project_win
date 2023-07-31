#include"PthreadPool.h"

template<typename T>
bool SafeQueue<T>::empty()
{
	unique_lock<mutex> locker(m_Mutex);
	return  m_Queue.empty();
}

template<typename T>
int SafeQueue<T>::size() {
	unique_lock<mutex> locker(m_Mutex);
	return m_Queue.size();
}

template<typename T>
void SafeQueue<T>::push(T& value)
{
	unique_lock<mutex> locker(m_Mutex);
	m_Queue.emplace_back(value);
}

template<typename T>
void SafeQueue<T>::push(T&& value)
{
	unique_lock<mutex> locker(m_Mutex);
	m_Queue.emplace_back(move(value));
}

template<typename T>
bool SafeQueue<T>::pop(T& value)
{
	unique_lock<mutex> locker(m_Mutex);
	if (m_Queue.empty()) {
		return false;
	}
	else {
		value = move(m_Queue.front());
		// move()将左值转义为右值，若是对象很大，使用移动语义避免拷贝
		// 左值对象被转移资源后，不会立刻析构，只有在离开自己的作用域的时候才会析构
		m_Queue.pop();
		return true;
	}
}

ThreadPool::~ThreadPool()
{
	Status = false;

	m_cond.notify_all();  //激活所有等待的线程，让他们继续运行，不用再等了

	for (auto& thread : v_Threads) {
		if (thread.joinable()) {               //若没有被系统接管的线程，等待他们
			thread.join();
		}
	}
}

void ThreadPool::initialize()
{
	for (int i = 0; i < v_Threads.size(); i++)
	{
		auto worker = [this, i]() {
			while (Status)
			{
				Task task;
				bool isSuccess = false;
				{
					unique_lock<mutex> locker(m_Mutex);
					while (m_TaskQueue.empty()) {
						m_cond.wait(locker);
					}
					isSuccess = m_TaskQueue.pop(task); //用task接收pop出来的移动语义
				}
				if (isSuccess)
				{
					//logfile.write("开始任务:%d\n",i);
					task();
					//logfile.write("结束任务:%d\n",i);
				}
			}
		};

		v_Threads[i] = thread(worker);
	}
}

template<class Func, class ...Args>
inline auto ThreadPool::commit(Func&& f, Args && ...args) -> future<decltype(f(args ...))>
{
	if (Status)    // stoped ??
		cout << "Status is stop" << endl;

	using ReturnType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型

	function<ReturnType()> taskwrapper1 = bind(forward<Func>(f), forward<Args>(args)...); // 把函数入口及参数,打包(绑定)

	auto taskWrapper2 = make_shared<packaged_task<ReturnType()>>(taskwrapper1);

	Task wrapperFunction = [taskWrapper2]() {
		(*taskWrapper2)();
	}



	future<RetType> future = task->get_future();

}




