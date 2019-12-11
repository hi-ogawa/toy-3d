namespace toy {

namespace utils {

//
// Usage:
// for (auto i : range(10)) { ... }
//
// cf.
// - https://en.cppreference.com/w/cpp/language/range-for
// - https://en.cppreference.com/w/cpp/named_req/Iterator
// - https://github.com/xelatihy/yocto-gl/blob/master/yocto/yocto_common.h
//
struct RangeHelper {
  int start_, end_;

  struct Iterator {
    int i_;

    int operator*() { return i_; }
    Iterator& operator++() {
      i_++;
      return *this;
    }
    bool operator!=(const Iterator& other) {
      return i_ != other.i_;
    }
  };

  Iterator begin() { return Iterator{start_}; };
  Iterator end() { return Iterator{end_}; };
};

RangeHelper range(int stop) {
  return RangeHelper{0, stop};
}

}

} // namespace toy
