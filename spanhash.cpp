#include "spanhash.hpp"
#include <utility>
#include <fstream>
#include <stdexcept>


SpanHash::SpanHash():
	size_(0),
	entries_()
{
}


SpanHash::SpanHash(const char* fileName, bool binary):
	size_(0),
	entries_()
{
	init(fileName, binary);
}


SpanHash::SpanHash(SpanHash&& that):
	size_(that.size_),
	entries_(std::move(that.entries_))
{
	that.size_ = 0;
	that.entries_.clear();
}


SpanHash& SpanHash::operator=(SpanHash&& that)
{
	size_ = that.size_;
	entries_ = std::move(that.entries_);

	that.size_ = 0;
	that.entries_.clear();
	
	return *this;
}


void SpanHash::init(const char* fileName, bool binary)
{
	std::ifstream stream(fileName, std::ios_base::in | std::ios_base::binary);
	if(!stream.is_open())
	{
		throw std::runtime_error(std::string("failed to open file: '") + fileName + "'");
	}

	size_ = 0;
	entries_.clear();

	int n = 0;
	Hasher hasher;
	for(int c = stream.get(), next = stream.get(); c != EOF; c = next, next = stream.get())
	{
		// don't distinguish between CR, LF, and CRLF
		if(!binary && c == '\r' && next == '\n')
		{
			continue;
		}

		if(c == '\r')
		{
			c = '\n';
		}

		size_ += 1;
		
		hasher.push(static_cast<unsigned char>(c));
		if(++n < 64 && c != '\n')
		{
			continue;
		}
		
		entries_[hasher.stop()] += n;

		n = 0;
	}
}


float SpanHash::compare(const SpanHash& that) const
{
	if(size_ == 0 && that.size_ == 0)
	{
		return 1.0f;
	}
	
	if(size_ == 0 || that.size_ == 0)
	{
		return 0.0f;
	}
	
	size_t src_copied = 0;

	auto thatIt = that.entries_.begin();
	for(auto thisIt = entries_.begin(); thisIt != entries_.end(); ++thisIt)
	{
		// unique that literals
		while(thatIt != that.entries_.end() && thatIt->first < thisIt->first)
		{
			++thatIt;
		}
		
		if(thatIt == that.entries_.end() || thatIt->first != thisIt->first)
		{
			// that literal doesn't match this one
			continue;
		}

		if(thatIt->second > thisIt->second)
		{
			// there are more such literals than here
			src_copied += thisIt->second;
		}
		else
		{
			src_copied += thatIt->second;
		}

		++thatIt;
	}

	while(thatIt != that.entries_.end())
	{
		++thatIt;
	}

	return
		static_cast<float>(src_copied) /
		static_cast<float>(std::max(size_, that.size_));
}


