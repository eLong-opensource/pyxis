#include <google/protobuf/stubs/common.h>
#include <pyxis/rpc/event.h>

namespace pyxis {
namespace rpc {

void Event::Done()
{
  if (done_) {
    done_->Run();
  }
}

Event::~Event()
{
  Done();
}

}
}
