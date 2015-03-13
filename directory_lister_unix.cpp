#include "directory_lister.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>


namespace
{

	bool isDir(const char* path)
	{
		struct stat buf;
		if(lstat(path, &buf) != 0)
		{
			return false;
		}
		
		return S_ISDIR(buf.st_mode);
	}
	
}


class DirectoryLister::Data
{
public:
	explicit Data(const char* path):
		path_(),
		isFile_(false),
		dir_(nullptr)
	{
		size_t len = strlen(path);
		bool hasTrailingSlash = false;
		while(len > 0)
		{
			const char c = path[len - 1];
			if(c == '/' || c == '\\')
			{
				hasTrailingSlash = true;
				len -= 1;
			}
			else
			{
				break;
			}
		}
		
		path_.assign(path, path + len);
		isFile_ = !hasTrailingSlash && !isDir(path_.c_str());
		memset(&dirent_, 0, sizeof(dirent_));

		if(!isFile_)
		{
			dir_ = opendir(path_.c_str());
			next();
		}
	}
	
	Data(const Data& that):
		path_(),
		isFile_(false),
		dir_(nullptr)
	{
		*this = that;
	}
	
	~Data()
	{
		clear();
	}

	Data& operator=(const Data& that)
	{
		clear();
		
		path_ = that.path_;
		isFile_ = that.isFile_;

		if(!isFile_)
		{
			if(that.dir_)
			{
				dir_ = opendir(path_.c_str());
				if(dir_)
				{
					dirent_ = that.dirent_;
					seekdir(dir_, telldir(that.dir_));
				}
			}
		}
	}

	bool isValid() const
	{
		return
			isFile_ ||
			dir_ != nullptr;
	}

	const std::string& path() const
	{
		return path_;
	}

	std::string current() const
	{
		if(isFile_)
		{
			return path_;
		}

		if(dir_)
		{
			return path_ + '/' + dirent_.d_name;
		}
		else
		{
			return std::string();
		}
	}

	bool currentIsDirectory() const
	{
		if(!isFile_ && dir_)
		{
			return dirent_.d_type == DT_DIR;
		}
		else
		{
			return false;
		}
	}

	void next()
	{
		if(isFile_)
		{
			isFile_ = false;
			return;
		}

		if(!dir_)
		{
			return;
		}

		while(true)
		{
			dirent* result = nullptr;
			if(readdir_r(dir_, &dirent_, &result) != 0 || !result)
			{
				clear();
				break;
			}
			
			if(strcmp(dirent_.d_name, ".") != 0 && strcmp(dirent_.d_name, "..") != 0)
			{
				break;
			}
		}
	}

	bool operator==(const Data& that) const
	{
		return
			path_ == that.path_ &&
			strcmp(dirent_.d_name, that.dirent_.d_name) == 0;
	}

private:
	std::string path_;
	bool isFile_;
	DIR* dir_;
	dirent dirent_;

	void clear()
	{
		isFile_ = false;

		if(dir_)
		{
			closedir(dir_);
			dir_ = nullptr;
		}

		memset(&dirent_, 0, sizeof(dirent_));
	}

};


DirectoryLister::DirectoryLister(const char* path):
	data_(std::make_shared<Data>(path))
{
	// nop
}

DirectoryLister::DirectoryLister(const DirectoryLister& that):
	data_(that.data_)
{
	// nop
}

DirectoryLister::~DirectoryLister()
{
	// nop
}

DirectoryLister& DirectoryLister::operator=(const DirectoryLister& that)
{
	data_ = that.data_;
}

bool DirectoryLister::isValid() const
{
	return data_->isValid();
}

const std::string& DirectoryLister::path() const
{
	return data_->path();
}

std::string DirectoryLister::current() const
{
	return data_->current();
}

bool DirectoryLister::currentIsDirectory() const
{
	return data_->currentIsDirectory();
}

void DirectoryLister::next()
{
	detach();
	data_->next();
}

bool DirectoryLister::operator==(const DirectoryLister& that) const
{
	return
		data_ == that.data_ ||
		*data_ == *that.data_;
}


void DirectoryLister::detach()
{
	if(!data_.unique())
	{
		data_ = std::make_shared<Data>(*data_);
	}
}


