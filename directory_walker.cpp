#include "directory_walker.hpp"
#include "directory.hpp"
#include <vector>


class DirectoryWalker::iterator::Data
{
public:
	Data(const char* path, bool followSymlinks):
		dirs_(),
		followSymlinks_(followSymlinks)
	{
		if(!push(path)->isValid())
		{
			pop();
		}
	}

	DirectoryWalker::value_type current() const
	{
		if(auto d = dir())
		{
			return value_type(d->currentName(), d->currentStat());
		}
		else
		{
			return value_type(std::string(), Directory::Stat());
		}
	}

	void next()
	{
		auto d = dir();
		if(!d)
		{
			return;
		}

		if(d->currentStat().fileType == Directory::Stat::Directory)
		{
			d = push(d->currentName().c_str());
		}
		else
		{
			d->next();
		}

		while(!d->isValid())
		{
			pop();
			d = dir();
			if(!d)
			{
				return;
			}

			d->next();
		}
	}

	bool operator==(const Data& that) const
	{
		bool thisIsValid = isValid();
		bool thatIsValid = that.isValid();

		if(!thisIsValid && !thatIsValid)
		{
			return true;
		}

		if(!thisIsValid || !thatIsValid)
		{
			return false;
		}

		return dirs_ == that.dirs_;
	}

private:
	std::vector<Directory> dirs_;
	bool followSymlinks_;

	const Directory* dir() const
	{
		if(!dirs_.empty())
		{
			return &dirs_.back();
		}
		else
		{
			return nullptr;
		}
	}

	Directory* dir()
	{
		if(!dirs_.empty())
		{
			return &dirs_.back();
		}
		else
		{
			return nullptr;
		}
	}

	Directory* push(const char* path)
	{
		dirs_.emplace_back(path, followSymlinks_);
		return &dirs_.back();
	}

	void pop()
	{
		dirs_.pop_back();
	}

	bool isValid() const
	{
		if(this == nullptr)
		{
			return false;
		}

		auto d = dir();
		return d && d->isValid();
	}

};


////////////////////////////////////////////////////////////////////////////////

DirectoryWalker::iterator::iterator():
	data_()
{
	// nop
}


DirectoryWalker::iterator::iterator(const iterator& that):
	data_(that.data_)
{
	// nop
}


DirectoryWalker::iterator::iterator(const char* path, bool followSymlinks):
	data_(std::make_shared<Data>(path, followSymlinks))
{
	// nop
}


DirectoryWalker::iterator::~iterator()
{
	// nop
}


DirectoryWalker::iterator& DirectoryWalker::iterator::operator=(const iterator& that)
{
	data_ = that.data_;
}


DirectoryWalker::value_type DirectoryWalker::iterator::operator*() const
{
	if(data_)
	{
		return data_->current();
	}
	else
	{
		return value_type(std::string(), Directory::Stat());
	}
}


DirectoryWalker::iterator& DirectoryWalker::iterator::operator++()
{
	detach();

	if(data_)
	{
		data_->next();
	}

	return *this;
}


bool DirectoryWalker::iterator::operator==(const iterator& that) const
{
	return *data_ == *that.data_;
}


void DirectoryWalker::iterator::detach()
{
	if(data_ && !data_.unique())
	{
		data_ = std::make_shared<Data>(*data_);
	}
}

////////////////////////////////////////////////////////////////////////////////

DirectoryWalker::DirectoryWalker(const char* path, bool followSymlinks):
	path_(path),
	followSymlinks_(followSymlinks)
{
}


DirectoryWalker::iterator DirectoryWalker::begin()
{
	return iterator(path_.c_str(), followSymlinks_);
}


DirectoryWalker::iterator DirectoryWalker::end()
{
	return iterator();
}


