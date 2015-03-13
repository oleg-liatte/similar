#ifndef SPANHASH_HPP_INCLUDED
#define SPANHASH_HPP_INCLUDED


#include <map>
#include "hasher.hpp"


/**
 * Helper class to compare two files and calculate their similarity.
 * Based on algorithm from git (http://git-scm.com/).
 */
class SpanHash
{
public:
	SpanHash();
	SpanHash(const char* fileName, bool binary);
	SpanHash(SpanHash&& that);

	SpanHash& operator=(SpanHash&& that);
	
	void init(const char* fileName, bool binary);
	
	float compare(const SpanHash& that) const;
	
private:
	size_t size_;
	std::map<Hasher::Hash, size_t> entries_;

};


#endif
