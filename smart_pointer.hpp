#ifndef SMART_POINTER_HPP_INCLUDED
#define SMART_POINTER_HPP_INCLUDED


class RefCount
{
public:
	RefCount():
		refs_(0)
	{
	}
	
	RefCount(const RefCount& that):
		refs_(0) // new object has its own counter
	{
	}
	
	virtual ~RefCount()
	{
	}
	
	RefCount& operator=(const RefCount& that)
	{
		// don't change self refs
	}

	unsigned long refs() const
	{
		return refs_;
	}
	
	void incRef()
	{
		refs_ += 1;
	}
	
	void decRef()
	{
		if(refs_ > 0)
		{
			--refs_;
		}
		
		if(refs_ == 0)
		{
			delete this;
		}
	}
	
private:
	unsigned long refs_;

};


template<typename T>
class SmartPointer
{
public:
	typedef T value_
	SmartPointer(T* pointer):
		pointer_(pointer)
	{
		if(pointer_)
		{
			pointer_->incRef();
		}
	}
	
	SmartPointer(const SmartPointer& that):
		pointer_(pointer)
	{
		if(pointer_)
		{
			pointer_->incRef();
		}
	}
	
	~SmartPointer()
	{
		if(pointer_)
		{
			pointer_->decRef();
		}
	}
	
	void reset(T* pointer)
	{
		if(pointer == pointer_)
		{
			return;
		}
		
		if(pointer_)
		{
			pointer_->decRef();
		}

		pointer_ = pointer;

		if(pointer_)
		{
			pointer_->incRef();
		}
	}
	
	SmartPointer& operator=(const SmartPointer& that)
	{
		reset(that.pointer_);
		return *this;
	}
	
	SmartPointer& operator=(const SmartPointer& that)
	{
		reset(that.pointer_);
		return *this;
	}
	
private:
	T* pointer_;

};


#endif
