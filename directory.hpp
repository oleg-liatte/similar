#ifndef DIRECTORY_HPP_INCLUDED
#define DIRECTORY_HPP_INCLUDED


#include <string>
#include <memory>


class Directory
{
public:
	class iterator
	{
	public:
		iterator();
		iterator(const iterator& that);
		explicit iterator(const char* path);
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
		class Data;
		
		std::shared_ptr<Data> data_;

		void detach();

	};
	
	explicit Directory(const char* path);

	const std::string& path()
	{
		return path_;
	}

	iterator begin();
	iterator end();

private:
	std::string path_;

};


#endif

