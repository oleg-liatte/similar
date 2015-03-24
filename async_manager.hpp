#ifndef ASYNC_MANAGER_HPP_INCLUDED
#define ASYNC_MANAGER_HPP_INCLUDED


#include <functional>


namespace AsyncManager
{

	typedef std::function<void()> Operation;

	/**
	Run given function asynchronously in background thread.

	Total number of concurrent asynchronous operation can be limited to improve
	performance. If operation can't be run immediately then it's execution can
	be delayed.
	*/
	void async(const char* group, const Operation& operation);

	inline void async(const Operation& operation)
	{
		async("", operation);
	}

	void joinGroup(const char* group);
	void joinAll();

	/**
	Run given task when tick() is called.
	*/
	void sync(const Operation& operation);

	/**
	Run all queued synchronous tasks.

	Tasks are run in the same order they were added.

	This function is supposed to be run periodically from main thread.
	*/
	void tick();

}


#endif
