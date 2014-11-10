/*
 * Vector.hpp
 *
 * Created: 09/11/2014 09:50:41
 *  Author: David
 */ 


#ifndef VECTOR_H_
#define VECTOR_H_

// Bounded vector class
template<class T, size_t N> class Vector
{
public:
	Vector() : filled(0) { }
		
	bool full() const { return filled == N; }
		
	size_t capacity() const { return N; }
		
	size_t size() const { return filled; }
		
	const T& operator[](size_t index) const pre(index < N) { return storage[index]; }

	T& operator[](size_t index) pre(index < N) { return storage[index]; }
		
	void add(const T& x) pre(filled < N) { storage[filled++] = x; }
		
	void clear() { filled = 0; }

protected:
	T storage[N];
	size_t filled;	
};

// Compare a vector of char with a bull-terminated string
template<size_t N> int compare(Vector<char, N> v, const char * array s)
{
	for (size_t i = 0; i < v.size(); ++i)
	{
		char c1 = v[i];
		char c2 = *s++;
		if (c1 != c2)
		{
			return (c1 > c2) ? 1 : -1;
		}
	}
	return (*s == '\0') ? 0 : -1;
}

template<size_t N> class String : public Vector<char, N>
{
public:
	// Redefine 'full' so as to make room for a null terminator
	bool full() const { return this->filled >= N - 1; }
		
	const char* array c_str() { this->storage[this->filled] = 0; return this->storage; }
};

#endif /* VECTOR_H_ */