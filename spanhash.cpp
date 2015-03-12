#include "spanhash.hpp"
#include <utility>
#include <fstream>
#include <stdexcept>


namespace
{

	bool isBinary(std::istream& stream)
	{
		auto orig = stream.tellg();
		if(orig == -1)
		{
			// assume unseekable stream is binary
			return true;
		}

		// read up to 1024 bytes
		const size_t MAX_READ = 1024;
		bool result = false;
		for(size_t i = 0; i != MAX_READ; ++i)
		{
			int ch = stream.get();
			if(ch == EOF)
			{
				break;
			}

			if(!std::isprint(ch) && !std::isspace(ch))
			{
				result = true;
				break;
			}
		}

		stream.clear(std::ios_base::eofbit);
		stream.seekg(orig);

		return result;
	}

}


SpanHash::SpanHash():
	size_(0),
	entries_()
{
}


SpanHash::SpanHash(const char* fileName):
	size_(0),
	entries_()
{
	init(fileName);
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


void SpanHash::init(const char* fileName)
{
	std::ifstream stream(fileName, std::ios_base::in | std::ios_base::binary);
	if(!stream.is_open())
	{
		throw std::runtime_error(std::string("failed to open file: ") + fileName);
	}

	size_ = 0;
	entries_.clear();

	bool is_text = !isBinary(stream);

	int n = 0;
	Hasher hasher;
	for(int c = stream.get(), next = stream.get(); c != EOF; c = next, next = stream.get())
	{
		// Ignore CR in CRLF sequence if text
		if (is_text && c == '\r' && next == '\n')
		{
			continue;
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


