/*
 * selector.cc
 *
 *  Created on: Dec 16, 2013
 *      Author: fan
 */

#include <algorithm>
#include <ctime>
#include <cstdlib>

#include <poll.h>
#include <glog/logging.h>
#include "selector.h"

namespace pyxis {

Selector::Selector()
    : pollfdList_(),
      selectableMap_()
{
}

void Selector::Register(Selectable* s)
{
  if (s == NULL) {
    return;
  }

  struct pollfd pfd;
  pfd.fd = s->Fd();
  pfd.events = POLLIN;
  pfd.revents = 0;
  pollfdList_.push_back(pfd);
  selectableMap_[s->Fd()] = s;
}

int Selector::Select(SelectableList* l, int timeout)
{
  int n = 0;
  do {
    n = ::poll(&*pollfdList_.begin(), pollfdList_.size(), timeout);
  } while (n < 0 && errno == EINTR);

  PCHECK(n >= 0) << "poll error";

  if (n == 0) {
    return 0;
  }

  if (l == NULL) {
    return n;
  }

  for (PollfdList::iterator pfd = pollfdList_.begin();
       pfd != pollfdList_.end(); pfd++) {
    if (pfd->revents > 0) {
      l->push_back(selectableMap_[pfd->fd]);
    }
  }

  std::srand(unsigned(std::time(0)));
  std::random_shuffle(l->begin(), l->end());

  return n;
}

int Selector::Poll(Selectable* s, int timeout)
{
  if (s == NULL) {
    return 0;
  }

  struct pollfd pfd;
  pfd.fd = s->Fd();
  pfd.events = POLLIN;
  pfd.revents = 0;


  int n = 0;
  do {
    n = ::poll(&pfd, 1, timeout);
  } while (n < 0 && errno == EINTR);

  PCHECK(n >= 0) << "poll error";

  if (n == 0) {
    return 0;
  }

  CHECK(n == 1);
  return n;
}

Selectable* Selector::Wait()
{
  SelectableList list;
  if (Select(&list, -1) == 0) {
    return NULL;
  }
  return list[0];
}

void Selector::Reset()
{
  selectableMap_.clear();
  pollfdList_.clear();
}


} // end of namespace pyxis
