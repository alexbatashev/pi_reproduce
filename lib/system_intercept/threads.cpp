#include <dlfcn.h>
#include <pthread.h>

#include <array>
#include <string>

using pthread_create_t = int (*)(pthread_t *, const pthread_attr_t *,
                                 void *(*)(void *), void *);
using pthread_self_t = pthread_t (*)();
using pthread_getname_np_t = int (*)(pthread_t, char *, size_t);
using pthread_setname_np_t = int (*)(pthread_t, const char *);

thread_local size_t threadCouner = 0;

extern "C" {
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *arg), void *arg) {
  static auto *real_pthread_create =
      reinterpret_cast<pthread_create_t>(dlsym(RTLD_NEXT, "pthread_create"));
  static auto *real_pthread_self =
      reinterpret_cast<pthread_self_t>(dlsym(RTLD_DEFAULT, "pthread_self"));
  static auto *real_pthread_getname_np = reinterpret_cast<pthread_getname_np_t>(
      dlsym(RTLD_DEFAULT, "pthread_getname_np"));
  static auto *real_pthread_setname_np = reinterpret_cast<pthread_setname_np_t>(
      dlsym(RTLD_DEFAULT, "pthread_setname_np"));

  int retValue = real_pthread_create(thread, attr, start_routine, arg);

  pthread_t self = real_pthread_self();
  std::array<char, 1024> buf;
  real_pthread_getname_np(self, buf.data(), buf.size());
  std::string newName{buf.data()};
  newName += "_" + std::to_string(threadCouner++);

  real_pthread_setname_np(*thread, newName.c_str());

  return retValue;
}
}
__attribute__((constructor)) static void setMainThreadName() {
  static auto *real_pthread_setname_np = reinterpret_cast<pthread_setname_np_t>(
      dlsym(RTLD_DEFAULT, "pthread_setname_np"));
  static auto *real_pthread_self =
      reinterpret_cast<pthread_self_t>(dlsym(RTLD_DEFAULT, "pthread_self"));

  if (real_pthread_setname_np) {
    real_pthread_setname_np(real_pthread_self(), "main");
  }
}
