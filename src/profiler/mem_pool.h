#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <list>

template <class T>
class mempool_t {
public:
  mempool_t() {
    T* entry = new T;
    free_.push_back(entry);
    cur_size_ = 1;
  }

  T* acquire() {
    if (free_.empty()) {
      expand();
    }
    T* f = free_.front();
    free_.pop_front();
    return f;
  }

  void release(T* entry) {
    free_.push_front(entry);
  }

  void expand() {
    for (int i = 0; i < cur_size_; i++) {
      T* entry = new T;
      free_.push_back(entry);
    }
    cur_size_ <<= 1;
  }

private:
  int cur_size_;
  std::list<T*> free_;
};

#endif // __MEMPOOL_H__
