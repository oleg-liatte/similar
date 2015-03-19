#include "directory.hpp"

#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <iostream>


Directory::Stat::Stat(const char* path, bool followSymlinks)
{
	struct stat s;
	auto statFn = followSymlinks ? &::stat : &::lstat;
	if(statFn(path, &s) != 0)
	{
		*this = Stat();
		return;
	}

	if(S_ISREG(s.st_mode))
	{
		fileType = Regular;
	}
	else if(S_ISDIR(s.st_mode))
	{
		fileType = Directory;
	}
	else if(S_ISLNK(s.st_mode))
	{
		fileType = Link;
	}
	else if(S_ISBLK(s.st_mode))
	{
		fileType = Block;
	}
	else if(S_ISCHR(s.st_mode))
	{
		fileType = Char;
	}
	else if(S_ISFIFO(s.st_mode))
	{
		fileType = FIFO;
	}
	else if(S_ISSOCK(s.st_mode))
	{
		fileType = Socket;
	}

	size = s.st_size;
	atime = s.st_atime;
	mtime = s.st_mtime;
	ctime = s.st_ctime;
}

////////////////////////////////////////////////////////////////////////////////

class Directory::Data
{
public:
	Data(const char* path, bool followSymlinks):
		path_(),
		followSymlinks_(followSymlinks),
		dir_(nullptr),
		currentName_(),
		currentStat_()
	{
		// strip trailing slashes
		size_t len = strlen(path);
		while(len > 0)
		{
			const char c = path[len - 1];
			if(c == '/' || c == '\\')
			{
				len -= 1;
			}
			else
			{
				break;
			}
		}

		path_.assign(path, path + len);
		memset(&dirent_, 0, sizeof(dirent_));

		dir_ = opendir(path_.c_str());
		if(dir_)
		{
			next();
		}
		else
		{
			std::cerr << "failed to open directory: '" << path_ << "'" << std::endl;
		}
	}

	Data(const Data& that):
		path_(),
		followSymlinks_(false),
		dir_(nullptr),
		currentName_(),
		currentStat_()
	{
		*this = that;
	}

	~Data()
	{
		clear();
	}

	Data& operator=(const Data& that)
	{
		if(this == &that)
		{
			return *this;
		}
		
		clear();

		path_ = that.path_;
		followSymlinks_ = that.followSymlinks_;

		if(that.dir_)
		{
			dir_ = opendir(path_.c_str());
			if(dir_)
			{
				dirent_ = that.dirent_;
				seekdir(dir_, telldir(that.dir_));
			}
		}
		
		currentName_ = that.currentName_;
		currentStat_ = that.currentStat_;
		
		return *this;
	}

	bool isValid() const
	{
		return dir_ != nullptr;
	}

	const std::string& path() const
	{
		return path_;
	}

	const std::string& currentName() const
	{
		return currentName_;
	}

	const Stat& currentStat() const
	{
		return currentStat_;
	}

	void next()
	{
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
				currentName_ = path_ + '/' + dirent_.d_name;
				currentStat_ = Stat(currentName_.c_str(), followSymlinks_);
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
	bool followSymlinks_;
	DIR* dir_;
	dirent dirent_;
	std::string currentName_;
	Stat currentStat_;

	void clear()
	{
		if(dir_)
		{
			closedir(dir_);
			dir_ = nullptr;
		}

		memset(&dirent_, 0, sizeof(dirent_));
	}

};

////////////////////////////////////////////////////////////////////////////////

Directory::Directory(const char* path, bool followSymlinks):
	data_(std::make_shared<Data>(path, followSymlinks))
{
	// nop
}

Directory::Directory(const Directory& that):
	data_(that.data_)
{
	// nop
}

Directory::~Directory()
{
	// nop
}

Directory& Directory::operator=(const Directory& that)
{
	data_ = that.data_;
}

bool Directory::isValid() const
{
	return data_->isValid();
}

const std::string& Directory::path() const
{
	return data_->path();
}

const std::string& Directory::currentName() const
{
	return data_->currentName();
}

const Directory::Stat& Directory::currentStat() const
{
	return data_->currentStat();
}

void Directory::next()
{
	detach();
	data_->next();
}

bool Directory::operator==(const Directory& that) const
{
	return
		data_ == that.data_ ||
		*data_ == *that.data_;
}


void Directory::detach()
{
	if(!data_.unique())
	{
		data_ = std::make_shared<Data>(*data_);
	}
}


