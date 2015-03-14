#ifndef DIRECTORY_HPP_INCLUDED
#define DIRECTORY_HPP_INCLUDED


#include <string>
#include <memory>


class Directory
{
public:
	enum Flag
	{
		FollowSymlinks = 1
	};

	class iterator
	{
		friend class Directory; // allow access to constructor

	public:
		iterator();
		iterator(const iterator& that);
		~iterator();

		iterator& operator=(const iterator& that);

		std::string operator*() const;
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
		iterator(const char* path, unsigned flags);

		class Data;

		std::shared_ptr<Data> data_;

		void detach();

	};

	explicit Directory(const char* path, unsigned flags = 0);

	const std::string& path()
	{
		return path_;
	}

	iterator begin();
	iterator end();

private:
	std::string path_;
	unsigned flags_;

};


#endif

