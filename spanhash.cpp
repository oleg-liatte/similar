#include "spanhash.hpp"
#include <utility>
#include <iostream>
#include <fstream>
#include <stdexcept>


SpanHash::SpanHash():
	valid_(false),
	size_(0),
	entries_()
{
}


SpanHash::SpanHash(SpanHash&& that):
	valid_(that.valid_),
	size_(that.size_),
	entries_(std::move(that.entries_))
{
	that.valid_ = false;
	that.size_ = 0;
	that.entries_.clear();
}


SpanHash& SpanHash::operator=(SpanHash&& that)
{
	valid_ = that.valid_;
	size_ = that.size_;
	entries_ = std::move(that.entries_);

	that.valid_ = false;
	that.size_ = 0;
	that.entries_.clear();

	return *this;
}


bool SpanHash::isEmpty() const
{
	return entries_.empty();
}


bool SpanHash::isValid() const
{
	return valid_;
}


bool SpanHash::init(const char* fileName, bool binary)
{
	valid_ = false;
	size_ = 0;
	entries_.clear();

	std::ifstream stream(fileName, std::ios_base::in | std::ios_base::binary);
	if(!stream.is_open())
	{
		std::cerr << "ERROR: failed to open file: '" << fileName << "'" << std::endl;
		return false;
	}

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

	valid_ = true;
	return true;
}


void SpanHash::clear()
{
	// not just clear() to ensure there is no pre-allocated memory left
	Entries empty;
	entries_.swap(empty);
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

	for(const auto& thisEntry: entries_)
	{
		auto thatEntryIt = that.entries_.find(thisEntry.first);
		if(thatEntryIt == that.entries_.end())
		{
			continue;
		}

		src_copied += std::min(thisEntry.second, thatEntryIt->second);
	}

	return
		static_cast<float>(src_copied) /
		static_cast<float>(std::max(size_, that.size_));
}


