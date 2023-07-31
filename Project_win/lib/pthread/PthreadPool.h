#pragma once
#include<vector>
#include<queue>
#include<atomic>
#include<future>
#include<mutex>
#include<condition_variable>
#include<fstream>
#include<thread>
#include<chrono>
#include<functional>
#include"_public.h"
#include"pthread.h"
using namespace std;
using namespace chrono;

// 安全队列
template<typename T>
class SafeQueue {
private:
	queue<T> m_Queue;
	mutex m_Mutex;

public:
	SafeQueue() = default;

	~SafeQueue() = default;

	SafeQueue(const SafeQueue& other) = delete;

	SafeQueue& operator=(const SafeQueue& other) = delete;

	SafeQueue(SafeQueue&& other) = delete;

	SafeQueue& operator=(const SafeQueue&& other) = delete;

	SafeQueue(const SafeQueue&& other) = delete;

	bool empty();

	int size();

	void push(T& value);

	void push(T&& value);

	bool pop(T& value);

};

class ThreadPool {
public:

	//静止拷贝构造
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(const ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&&) = delete;

	ThreadPool() :v_Threads(thread::hardware_concurrency() + 2), Status(true) {}

	ThreadPool(int ThreadNum) :v_Threads(ThreadNum), Status(true) {}

	~ThreadPool();

private:

	using Task = function<void()>;   // 将任务包装成最终的函数形态

	SafeQueue<Task> m_TaskQueue;     // 任务队列

	vector<thread> v_Threads;        // 线程容器

	condition_variable m_cond;       // 条件变量

	mutex m_Mutex;

	atomic<bool> Status;             // 原子操作，存储线程池状态

	void initialize();               //初始化线程池

	template<class Func, class... Args>
	auto commit(Func&& f, Args&&... args) -> future<decltype(f(args...))>;

};

