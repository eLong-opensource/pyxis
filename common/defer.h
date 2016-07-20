#ifndef PYXIS_COMMON_DEFER_H
#define PYXIS_COMMON_DEFER_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

namespace pyxis {

typedef boost::function<void()> DeferFunc;

class Defer : boost::noncopyable
{
public:
  Defer(const DeferFunc& func) : func_(func) {}
  ~Defer() { func_(); }

private:
  DeferFunc func_;
};

}

// Prevent misuse like:
// Defer(func);
// A tempory object doesn't hold the func for long!
#define Defer(x) error "Missing defer object name"

#endif
