#ifndef DIRECTORY_LISTER_HPP_INCLUDED
#define DIRECTORY_LISTER_HPP_INCLUDED


#include <stddef.h> // for size_t
#include <time.h> // for time_t

#include <memory>
#include <string>


class Directory
{
public:
	struct Stat
	{
		enum FileType
		{
			Unknown,
			Regular,
			Directory,
			Link,
			Block,
			Char,
			FIFO,
			Socket
		};

		Stat():
			fileType(Unknown),
			size(0),
			atime(0),
			mtime(0),
			ctime(0)
		{
		}

		explicit Stat(const char* path, bool followSymlinks);
		
		FileType fileType;
		size_t size;
		time_t atime;
		time_t mtime;
		time_t ctime;

	};

	Directory(const char* path, bool followSymlinks);
	Directory(const Directory& that);
	~Directory();

	Directory& operator=(const Directory& that);

	bool isValid() const;
	const std::string& path() const;
	const std::string& currentName() const;
	const Stat& currentStat() const;
	void next();

	bool operator==(const Directory& that) const;

private:
	class Data;

	std::shared_ptr<Data> data_;

	void detach();

};


#endif
