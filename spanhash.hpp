#ifndef SPANHASH_HPP_INCLUDED
#define SPANHASH_HPP_INCLUDED


#include <map>
#include "hasher.hpp"


class SpanHash
{
public:
	SpanHash();
	explicit SpanHash(const char* fileName);
	SpanHash(SpanHash&& that);

	SpanHash& operator=(SpanHash&& that);
	
	void init(const char* fileName);
	
	float compare(const SpanHash& that) const;
	
private:
	size_t size_;
	std::map<Hasher::Hash, size_t> entries_;

};


#endif
