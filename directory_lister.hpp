#ifndef DIRECTORY_LISTER_HPP_INCLUDED
#define DIRECTORY_LISTER_HPP_INCLUDED


#include <memory>
#include <string>


class DirectoryLister
{
public:
	explicit DirectoryLister(const char* path);
	DirectoryLister(const DirectoryLister& that);
	~DirectoryLister();
	
	DirectoryLister& operator=(const DirectoryLister& that);

	bool isValid() const;
	const std::string& path() const;
	std::string current() const;
	bool currentIsDirectory() const;
	void next();

	bool operator==(const DirectoryLister& that) const;
	
private:
	class Data;
	
	std::shared_ptr<Data> data_;

	void detach();

};


#endif
