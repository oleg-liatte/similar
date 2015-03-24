#ifndef SPANHASH_HPP_INCLUDED
#define SPANHASH_HPP_INCLUDED


#include <unordered_map>
#include "hasher.hpp"


/**
 * Helper class to compare two files and calculate their similarity.
 * Based on algorithm from git (http://git-scm.com/).
 */
class SpanHash
{
public:
	SpanHash();
	SpanHash(SpanHash&& that);

	SpanHash& operator=(SpanHash&& that);

	bool empty() const;
	bool init(const char* fileName, bool binary);
	void clear();
	
	float compare(const SpanHash& that) const;
	
private:
	typedef std::unordered_map<Hasher::Hash, size_t> Entries;

	size_t size_;
	Entries entries_;

};


#endif
