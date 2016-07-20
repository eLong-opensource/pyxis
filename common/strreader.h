#ifndef PYXIS_COMMON_STRREADER_H
#define PYXIS_COMMON_STRREADER_H

#include <string>
#include <pyxis/common/status.h>

namespace pyxis {

class StrReader
{
public:
  StrReader(std::string& s) :s_(s), i_(0) {}

  ~StrReader(){}

  Status ReadN(void* s, int n) {
    if (i_ + n > s_.size()) {
      return Status::InvalidArgument("length not enougth");
    }
    memcpy(s, &s_[i_], n);
    i_ += n;
    return Status::OK();
  }

  int64_t ReadInt64() {
    int64_t n;
    Status st =  ReadN(&n, sizeof(n));
    CHECK(st.ok());
    return n;
  }

  uint16_t ReadInt16() {
    uint16_t n;
    Status st =  ReadN(&n, sizeof(n));
    CHECK(st.ok());
    return n;
  }

  uint8_t ReadByte() {
    uint8_t n;
    Status st = ReadN(&n, sizeof(n));
    CHECK(st.ok());
    return n;
  }

  Status Skip(int n) {
    if (i_ + n > s_.size()) {
      return Status::InvalidArgument("bad length");
    }
    i_ += n;
    return Status::OK();
  }

  // 返回从当前位置到delim的字符串，不包括delim，
  // 如果找不到delim则返回整个字符串
  std::string ReadString(char delim) {
    int pos = s_.find(delim, i_);
    if (pos == std::string::npos) {
      // return entire string
      const std::string& s = s_.substr(i_);
      i_ = s_.size();
      return s;
    }

    const std::string& s = s_.substr(i_, pos - i_);
    i_ = pos + 1;
    return s;
  }


private:
  std::string& s_;
  int i_;
};

}

#endif
