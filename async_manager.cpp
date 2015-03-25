#include "async_manager.hpp"

#include <deque>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <mutex>


namespace
{

	class Event
	{
	public:
		Event():
			signaled_(false),
			event_()
		{
		}

		void wait(std::unique_lock<std::mutex>& lock)
		{
			event_.wait(lock, [this]{ return signaled_; });
		}

		void set()
		{
			signaled_ = true;
			event_.notify_all();
		}

	private:
		bool signaled_;
		std::condition_variable event_;

	};

	struct Task
	{
		Task(const AsyncManager::Task& task, const char* group, Event* started = nullptr):
			task(task),
			group(group),
			started(started)
		{
		}

		AsyncManager::Task task;
		std::string group;
		Event* started;
	};

	struct Group
	{
		Group():
			tasks(0),
			waiters(0),
			finished()
		{
		}

		template<class Rep, class Period>
		bool wait_for(std::unique_lock<std::mutex>& lock, const std::chrono::duration<Rep, Period>& rel_time)
		{
			return finished.wait_for(lock, rel_time, [this]{ return tasks == 0; });
		}

		size_t tasks;
		size_t waiters;
		std::condition_variable finished;
	};

	typedef std::map<std::string, Group> Groups;

	std::mutex s_asyncMutex;
	std::vector<std::thread> s_workers;
	Groups s_groups;
	std::deque<Task> s_asyncQueue;
	std::atomic<bool> s_asyncStop(false);
	std::condition_variable s_asyncWorkerAttention;

	std::mutex s_syncMutex;
	std::deque<AsyncManager::Task> s_syncQueue;

	void worker()
	{
		while(true)
		{
			AsyncManager::Task task;
			std::string group;

			{
				// take task
				std::unique_lock<std::mutex> lock(s_asyncMutex);

				while(true)
				{
					if(s_asyncStop.load())
					{
						return;
					}

					if(!s_asyncQueue.empty())
					{
						auto& t = s_asyncQueue.front();

						// extract task
						task.swap(t.task);
						group.swap(t.group);

						// add it to the corresponding group
						s_groups[group].tasks += 1;

						if(t.started)
						{
							t.started->set();
						}

						s_asyncQueue.pop_front();

						break;
					}

					s_asyncWorkerAttention.wait(lock);
				}
			}

			// start task
			task();

			{
				// clean up
				std::unique_lock<std::mutex> lock(s_asyncMutex);

				auto groupIt = s_groups.find(group);

				if(groupIt != s_groups.end())
				{
					auto& g = groupIt->second;
					if(g.tasks > 0)
					{
						g.tasks -= 1;
					}

					if(g.tasks == 0)
					{
						if(g.waiters > 0)
						{
							g.finished.notify_all();
						}
						else
						{
							s_groups.erase(groupIt);
						}
					}
				}
			}
		}
	}

	void initWorkers()
	{
		if(!s_workers.empty())
		{
			return;
		}

		const size_t count = std::max<size_t>(1, std::thread::hardware_concurrency());
		s_workers.reserve(count);
		for(size_t i = 0; i != count; ++i)
		{
			s_workers.emplace_back(&worker);
		}
	}

	void asyncTick(std::unique_lock<std::mutex>& lock)
	{
		lock.unlock();
		AsyncManager::tick();
		lock.lock();
	}

	void syncTask(std::unique_lock<std::mutex>& lock, Group& group)
	{
		group.waiters += 1;
		while(!group.wait_for(lock, std::chrono::milliseconds(100)))
		{
			asyncTick(lock);
		}
		group.waiters -= 1;
	}

	class Unit
	{
	public:
		~Unit()
		{
			// stop all workers
			s_asyncStop.store(true);

			s_asyncWorkerAttention.notify_all();

			for(auto& worker: s_workers)
			{
				worker.join();
			}
		}
	};

	Unit s_unit;

}


namespace AsyncManager
{

	void async(const char* group, bool allowQueue, const Task& task)
	{
		if(!task)
		{
			return;
		}

		initWorkers();

		if(!group)
		{
			group = "";
		}

		std::unique_lock<std::mutex> lock(s_asyncMutex);

		if(allowQueue)
		{
			s_asyncQueue.emplace_back(task, group, nullptr);
			s_asyncWorkerAttention.notify_one();
		}
		else
		{
			Event started;
			s_asyncQueue.emplace_back(task, group, &started);
			s_asyncWorkerAttention.notify_one();
			started.wait(lock);
		}
	}

	void sync(const char* group)
	{
		std::unique_lock<std::mutex> lock(s_asyncMutex);

		auto groupIt = s_groups.find(group ? group : "");
		if(groupIt != s_groups.end())
		{
			syncTask(lock, groupIt->second);
			s_groups.erase(groupIt);
		}
	}

	void syncAll()
	{
		std::unique_lock<std::mutex> lock(s_asyncMutex);

		while(true)
		{
			asyncTick(lock);

			auto groupIt = s_groups.begin();
			if(groupIt == s_groups.end())
			{
				break;
			}

			syncTask(lock, groupIt->second);
			s_groups.erase(groupIt);
		}
	}

	void sync(const Task& task)
	{
		if(!task)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(s_syncMutex);
		s_syncQueue.push_back(task);
	}

	void tick()
	{
		while(true)
		{
			Task task;

			{
				std::lock_guard<std::mutex> lock(s_syncMutex);
				if(s_syncQueue.empty())
				{
					break;
				}

				task = std::move(s_syncQueue.front());
				s_syncQueue.pop_front();
			}

			task();
		}
	}

}


