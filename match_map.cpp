#include "match_map.hpp"


bool MatchMap::add(const std::string& src, const std::string& dst, float similarity)
{
	auto srcRange = srcToDst_.equal_range(src);
	if(srcRange.first != srcRange.second && srcRange.first->second.second >= similarity)
	{
		return false;
	}

	auto dstRange = dstToSrc_.equal_range(dst);
	if(dstRange.first != dstRange.second && dstRange.first->second.second >= similarity)
	{
		return false;
	}

	bool added = false;
	
	auto d = std::make_pair(dst, similarity);
	if(srcRange.first == srcRange.second)
	{
		srcRange.first = srcToDst_.emplace_hint(srcRange.second, src, std::move(d));
		added = true;
	}
	else
	{
		srcRange.first->second = std::move(d);
	}
	
	auto s = std::make_pair(src, similarity);
	if(dstRange.first == dstRange.second)
	{
		dstRange.first = dstToSrc_.emplace_hint(dstRange.second, dst, std::move(s));
	}
	else
	{
		dstRange.first->second = std::move(s);
	}
	
	return added;
}


