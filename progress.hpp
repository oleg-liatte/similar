#ifndef PROGRESS_HPP_INCLUDED
#define PROGRESS_HPP_INCLUDED


#include <string>
#include <chrono>
#include <iostream>


class Progress
{
public:
	explicit Progress();
	
	const std::string& prefix() const
	{
		return prefix_;
	}
	
	void setPrefix(const std::string& v);

	const std::string& postfix() const
	{
		return postfix_;
	}

	void setPostfix(const std::string& v);

	std::chrono::milliseconds interval() const
	{
		return interval_;
	}
	
	template<class Rep, class Period>
	void setInterval(std::chrono::duration<Rep, Period> v)
	{
		interval_ = v;
	}

	float total() const
	{
		return total_;
	}
	
	void setTotal(float v);
	
	float current() const
	{
		return current_;
	}
	
	void setCurrent(float v);
	
	void update();
	void flush();
	
private:
	typedef std::chrono::steady_clock clock;

	std::string prefix_;
	std::string postfix_;
	float total_;
	float current_;
	std::chrono::milliseconds interval_;
	clock::time_point lastFlush_;
	
	void flush(clock::time_point now);

};


#endif

