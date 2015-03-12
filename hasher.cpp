#include "hasher.hpp"


Hasher::Hasher():
	accum1_(0),
	accum2_(0)
{
}


void Hasher::start()
{
	accum1_ = 0;
	accum2_ = 0;
}


void Hasher::push(unsigned char c)
{
	unsigned int old_1 = accum1_;
	accum1_ = (accum1_ << 7) ^ (accum2_ >> 25);
	accum2_ = (accum2_ << 7) ^ (old_1 >> 25);
	accum1_ += c;
}


Hasher::Hash Hasher::stop()
{
	const Hash HASHBASE = 107927;
	Hash r = (accum1_ + accum2_ * 0x61) % HASHBASE;
	start();
	return r;
}


