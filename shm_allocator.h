#ifndef __SHM_ALLOCATOR_H__
#define __SHM_ALLOCATOR_H__
#include "mem_pool.h"
#include <string>
#include <new>
#include <cstdlib> // for exit()
#include <climits> // for UNIX_MAX
#include <cstddef>



template<class T> class SHM_STL_Allocator
{
public:
  typedef T               value_type;
  typedef T*              pointer;
  typedef const T*        const_pointer;
  typedef T&              reference;
  typedef const T&        const_reference;
  typedef size_t          size_type;
  typedef ptrdiff_t       difference_type;


  SHM_STL_Allocator() {};
  ~SHM_STL_Allocator() {};
  template<class U> SHM_STL_Allocator(const SHM_STL_Allocator<U>& t) { };
  template<class U> struct rebind { typedef SHM_STL_Allocator<U> other; };

  pointer allocate(size_type n, const void* hint=0) {
//        fprintf(stderr, "allocate n=%u, hint= %p\n",n, hint);
    return((T*) (mm_malloc(n * sizeof(T))));
  }

  void deallocate(pointer p, size_type n) {
//        fprintf(stderr, "dealocate n=%u" ,n);
    mm_free((void*)p);
  }

  void construct(pointer p, const T& value) {
    ::new(p) T(value);
  }

  void construct(pointer p)
  {
    ::new(p) T();
  }

  void destroy(pointer p) {
    p->~T();
  }

  pointer address(reference x) {
    return (pointer)&x;
  }

  const_pointer address(const_reference x) {
    return (const_pointer)&x;
  }

  size_type max_size() const {
    return size_type(UINT_MAX/sizeof(T));
  }
};


class SHM_Allocator {
  public:
    static void *allocate (size_t size) {
      return mm_malloc(size);
      // return mem_pool_malloc(size);
    }

    static void deallocate (void *ptr) {
      return mm_free(ptr);
      // return mem_pool_free(ptr);
    }
};


class DM_Allocator {
  public:
    static void *allocate (size_t size) {
      return malloc(size);
    }

    static void deallocate (void *ptr) {
      return free(ptr);
    }
};


// template<class charT, class traits = char _traits<charT>,
// class Allocator = allocator<charT> >  




typedef std::basic_string<char, std::char_traits<char>, SHM_STL_Allocator<char> > SHMString;

#endif