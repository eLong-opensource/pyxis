#ifndef PYXIS_BASE_SELECTABLE_H
#define PYXIS_BASE_SELECTABLE_H

#include <string>
#include <boost/shared_ptr.hpp>

namespace pyxis {

// 可以被selector select的接口。设计为select少量接口，不适合大量的。
// 目前只支持可读事件
// 由于可读的时刻和最终读的时刻不一致，会发生竞争，不适合多个reader。
class Selectable {
 public:
  Selectable()
  {
  }
  virtual ~Selectable()
  {
  }
  virtual int Fd() const = 0;
  virtual bool IsReadable() = 0;
};

typedef boost::shared_ptr<Selectable> SelectablePtr;

} // end of namespace
#endif
