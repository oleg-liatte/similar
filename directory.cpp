#include "directory.hpp"
#include "directory_lister.hpp"
#include <vector>


class Directory::iterator::Data
{
public:
	explicit Data(const char* path):
		dirs_()
	{
		push(path);
		advanceToFile();
	}
	
	std::string current() const
	{
		if(auto d = dir())
		{
			return d->current();
		}
		else
		{
			return std::string();
		}
	}
	
	void next()
	{
		if(auto d = dir())
		{
			d->next();
			advanceToFile();
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
	std::vector<DirectoryLister> dirs_;

	const DirectoryLister* dir() const
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

	DirectoryLister* dir()
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

	void push(const char* path)
	{
		dirs_.emplace_back(path);
	}
	
	void pop()
	{
		dirs_.pop_back();
	}

	void advanceToFile()
	{
		while(auto d = dir())
		{
			if(!d->isValid())
			{
				pop();
				
				if(auto d = dir())
				{
					d->next();
				}
				
				continue;
			}

			if(!d->currentIsDirectory())
			{
				break;
			}

			push(d->current().c_str());
		}
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

Directory::iterator::iterator():
	data_()
{
	// nop
}


Directory::iterator::iterator(const iterator& that):
	data_(that.data_)
{
	// nop
}


Directory::iterator::iterator(const char* path):
	data_(std::make_shared<Data>(path))
{
	// nop
}


Directory::iterator::~iterator()
{
	// nop
}


Directory::iterator& Directory::iterator::operator=(const iterator& that)
{
	data_ = that.data_;
}


std::string Directory::iterator::operator*() const
{
	if(data_)
	{
		return data_->current();
	}
	else
	{
		return std::string();
	}
}


Directory::iterator& Directory::iterator::operator++()
{
	if(data_)
	{
		data_->next();
	}
	
	return *this;
}


bool Directory::iterator::operator==(const iterator& that) const
{
	return *data_ == *that.data_;
}


void Directory::iterator::detach()
{
	if(data_ && !data_.unique())
	{
		data_ = std::make_shared<Data>(*data_);
	}
}

////////////////////////////////////////////////////////////////////////////////

Directory::Directory(const char* path):
	path_(path)
{
}


Directory::iterator Directory::begin()
{
	return iterator(path_.c_str());
}


Directory::iterator Directory::end()
{
	return iterator();
}


