#ifndef DIRECTORY_HPP_INCLUDED
#define DIRECTORY_HPP_INCLUDED


#include <string>
#include <memory>
#include <utility>
#include "directory.hpp"


class DirectoryWalker
{
public:
	typedef std::pair<std::string, Directory::Stat> value_type;
	
	class iterator
	{
		friend class DirectoryWalker; // allow access to constructor

	public:
		iterator();
		iterator(const iterator& that);
		~iterator();

		iterator& operator=(const iterator& that);

		value_type operator*() const;
		iterator& operator++();

		iterator operator++(int)
		{
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const iterator& that) const;

		bool operator!=(const iterator& that) const
		{
			return !(*this == that);
		}

	private:
		iterator(const char* path, bool followSymlinks);

		class Data;

		std::shared_ptr<Data> data_;

		void detach();

	};

	DirectoryWalker(const char* path, bool followSymlinks);

	const std::string& path()
	{
		return path_;
	}

	iterator begin();
	iterator end();

private:
	std::string path_;
	bool followSymlinks_;

};


#endif

