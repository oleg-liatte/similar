#ifndef ASYNC_MANAGER_HPP_INCLUDED
#define ASYNC_MANAGER_HPP_INCLUDED


#include <functional>


namespace AsyncManager
{

	typedef std::function<void()> Task;

	/**
	Run given task asynchronously in background worker thread.

	- `group` specifies a group name for this task. See @ref sync(const char*);
	- `allowQueue` specifies if the task can be just put into the queue. If not
	  then caller is blocked until the task is dequeued by a worker thread;
	- `task` holds task body.

	Total number of concurrent workers can be limited to improve performance.
	*/
	void async(const char* group, bool allowQueue, const Task& task);

	/**
	@overload
	*/
	inline void async(const char* group, const Task& task)
	{
		async(group, true, task);
	}

	/**
	@overload
	*/
	inline void async(bool allowQueue, const Task& task)
	{
		async("", allowQueue, task);
	}

	/**
	@overload
	*/
	inline void async(const Task& task)
	{
		async("", task);
	}

	/**
	Synchronize to asynchronous tasks in the given group.
	Caller is blocked until all asynchronous tasks in the group are finished.
	*/
	void sync(const char* group);

	/**
	Synchronize to all asynchronous tasks.
	Caller is blocked until all synchronous and asynchronous tasks are finished.
	*/
	void syncAll();

	/**
	Run given task when tick() is called.
	*/
	void sync(const Task& task);

	/**
	Run all queued synchronous tasks.

	Tasks are run in the same order they were added.

	This function is supposed to be run periodically from main thread.
	*/
	void tick();

}


#endif
