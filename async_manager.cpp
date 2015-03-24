#include "async_manager.hpp"

#include <deque>
#include <map>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <iostream>


namespace
{

	size_t getMaxWorkers()
	{
		size_t result = static_cast<size_t>(std::thread::hardware_concurrency());

		return std::max<size_t>(1, result);
	}

	typedef std::map<std::thread::id, std::thread> Workers;
	typedef std::multimap<std::string, Workers::iterator> WorkerGroups;

	std::mutex s_asyncMutex;
	size_t s_maxWorkers = getMaxWorkers();
	Workers s_workers;
	WorkerGroups s_workerGroups;
	std::condition_variable s_asyncFinished;

	std::mutex s_syncMutex;
	std::deque<AsyncManager::Operation> s_syncQueue;

	template<typename Extractor>
	void joinWorkers(const Extractor& extractor)
	{
		while(true)
		{
			std::thread worker;

			{
				std::lock_guard<std::mutex> guard(s_asyncMutex);

				auto groupsIt = extractor();
				if(groupsIt == s_workerGroups.end())
				{
					break;
				}

				worker = std::move(groupsIt->second->second);
				s_workers.erase(groupsIt->second);
				s_workerGroups.erase(groupsIt);
			}

			worker.join();

			AsyncManager::tick();
		}

		AsyncManager::tick();
	}

	class CleanUp
	{
	public:
		~CleanUp()
		{
			// don't allow more workers to be added
			s_maxWorkers = 0;

			// join all workers
			AsyncManager::joinAll();
		}
	};

	CleanUp s_cleanUp;

	void startWorker(const std::string& group, const AsyncManager::Operation& operation)
	{
		std::thread worker([=]()
		{
			// perform effective work
			if(operation)
			{
				operation();
			}

			// cleanup
			std::lock_guard<std::mutex> guard(s_asyncMutex);

			// remove self
			auto workersIt = s_workers.find(std::this_thread::get_id());
			if(workersIt != s_workers.end())
			{
				auto groupRange = s_workerGroups.equal_range(group);
				for(auto groupsIt = groupRange.first; groupsIt != groupRange.second; ++groupsIt)
				{
					if(groupsIt->second == workersIt)
					{
						s_workerGroups.erase(groupsIt);
						break;
					}
				}

				workersIt->second.detach();
				s_workers.erase(workersIt);
			}

			s_asyncFinished.notify_one();
		});

		auto it = s_workers.emplace(worker.get_id(), std::move(worker)).first;
		s_workerGroups.emplace(group, it);
	}

}


namespace AsyncManager
{

	void async(const char* group, const Operation& operation)
	{
		if(!group)
		{
			group = "";
		}

		auto pred = []
		{
			return s_workers.size() < s_maxWorkers;
		};

		std::unique_lock<std::mutex> guard(s_asyncMutex);

		s_asyncFinished.wait(guard, []
		{
			return s_workers.size() < s_maxWorkers;
		});

		startWorker(group, operation);
	}

	void joinGroup(const char* group)
	{
		const std::string g = group ? group : "";

		joinWorkers([&]
		{
			auto range = s_workerGroups.equal_range(g);
			if(range.first != range.second)
			{
				return range.first;
			}
			else
			{
				return s_workerGroups.end();
			}
		});
	}

	void joinAll()
	{
		joinWorkers([]{ return s_workerGroups.begin(); });
	}

	void sync(const Operation& operation)
	{
		std::lock_guard<std::mutex> guard(s_syncMutex);
		s_syncQueue.push_back(operation);
	}

	void tick()
	{
		while(true)
		{
			Operation operation;

			{
				std::lock_guard<std::mutex> guard(s_syncMutex);
				if(s_syncQueue.empty())
				{
					break;
				}

				operation = s_syncQueue.front();
				s_syncQueue.pop_front();
			}

			if(operation)
			{
				operation();
			}
		}
	}

}


