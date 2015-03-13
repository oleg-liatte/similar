#ifndef MATCH_MAP_HPP_INCLUDED
#define MATCH_MAP_HPP_INCLUDED


#include <string>
#include <unordered_map>


class MatchMap
{
public:
	bool add(const std::string& src, const std::string& dst, float similarity);

	size_t size() const
	{
		return srcToDst_.size();
	}
	
	template<typename Fn>
	void for_each(Fn fn) const
	{
		for(const auto& m: srcToDst_)
		{
			fn(m.first, m.second.first, m.second.second);
		}
	}

private:
	std::unordered_map<std::string, std::pair<std::string, float>> srcToDst_;
	std::unordered_map<std::string, std::pair<std::string, float>> dstToSrc_;
	
};


#endif

