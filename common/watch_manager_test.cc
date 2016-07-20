#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <pyxis/common/watch_manager.h>
#include <boost/bind.hpp>

class MockFoo
{
 public:
  MOCK_METHOD0(Foo, void());
};

TEST(Foo, test)
{
}
