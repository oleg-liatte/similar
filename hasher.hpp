#ifndef HASHER_HPP_INCLUDED
#define HASHER_HPP_INCLUDED


class Hasher
{
public:
	typedef unsigned Hash;
	
	Hasher();
	
	void start();
	void push(unsigned char c);
	Hash stop();
	
private:
	unsigned accum1_;
	unsigned accum2_;

};


#endif
