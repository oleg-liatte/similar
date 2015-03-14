#ifndef DIRECTORY_LISTER_HPP_INCLUDED
#define DIRECTORY_LISTER_HPP_INCLUDED


#include <memory>
#include <string>


class DirectoryLister
{
public:
	enum FileType
	{
		Unknown,
		RegularFile,
		Directory,
		Link,
		Block,
		Char,
		FIFO,
		Socket
	};

	DirectoryLister(const char* path, bool followSymlinks);
	DirectoryLister(const DirectoryLister& that);
	~DirectoryLister();

	DirectoryLister& operator=(const DirectoryLister& that);

	bool isValid() const;
	const std::string& path() const;
	std::string currentName() const;
	FileType currentType() const;
	void next();

	bool operator==(const DirectoryLister& that) const;

private:
	class Data;

	std::shared_ptr<Data> data_;

	void detach();

};


#endif
