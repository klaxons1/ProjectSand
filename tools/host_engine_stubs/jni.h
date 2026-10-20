#pragma once
#include <cstdint>
using jobject = void*;
using jclass = void*;
using jmethodID = void*;
struct JNIEnv {
  void CallStaticVoidMethod(jclass, jmethodID, ...);
};
#ifndef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))
#endif
#ifndef JNICALL
#define JNICALL
#endif
