#include "progress.hpp"
#include <iostream>
#include <cmath>


Progress::Progress():
	prefix_(),
	postfix_(),
	total_(1.0f),
	current_(0.0f),
	interval_(1000),
	lastFlush_()
{
}


void Progress::setPrefix(const std::string& v)
{
	prefix_ = v;
}


void Progress::setPostfix(const std::string& v)
{
	postfix_ = v;
}


void Progress::setTotal(float v)
{
	total_ = v;
}


void Progress::setCurrent(float v)
{
	current_ = v;
}


void Progress::update()
{
	auto now = clock::now();
	if(now - lastFlush_ < interval_)
	{
		return;
	}
	
	flush(now);
}


void Progress::flush()
{
	flush(clock::now());
}


void Progress::flush(clock::time_point now)
{
	lastFlush_ = now;
	
	float progress;
	if(std::fabs(total_) > 0.0001f)
	{
		progress = current_ / total_ * 100.0f;
	}
	else
	{
		progress = 100.0f;
	}
	
	std::ostream& out = std::cout;

	auto oldFlags = out.setf(std::ios_base::fixed, std::ios_base::floatfield);
	auto oldPrecision = out.precision(1);

	out
		<< "\x1b[2K\r" // erase whole line
		<< prefix_ << progress << postfix_ << std::flush;
		
	out.precision(oldPrecision);
	out.flags(oldFlags);
}


