// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// <PUIC-PATCH>

#ifndef BASE_MEMORY_REF_COUNTED_H_
#define BASE_MEMORY_REF_COUNTED_H_

#include <stddef.h>

#include <utility>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace base {
namespace subtle {

class BASE_EXPORT RefCountedBase {
 public:
  bool HasOneRef() const { return ref_count_ == 1; }

 protected:
  explicit RefCountedBase(StartRefCountFromZeroTag) {
  }

  explicit RefCountedBase(StartRefCountFromOneTag) : ref_count_(1) {
  }

  ~RefCountedBase() {
  }

  void AddRef() const {
    AddRefImpl();
  }

  // Returns true if the object should self-delete.
  bool Release() const {
    --ref_count_;
    return ref_count_ == 0;
  }

 private:
  template <typename U>
  friend scoped_refptr<U> base::AdoptRef(U*);

  void Adopted() const {
  }

  void AddRefImpl() const { ++ref_count_; }

  mutable int ref_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

}  // namespace subtle

//
// A base class for reference counted classes.  Otherwise, known as a cheap
// knock-off of WebKit's RefCounted<T> class.  To use this, just extend your
// class from it like so:
//
//   class MyFoo : public base::RefCounted<MyFoo> {
//    ...
//    private:
//     friend class base::RefCounted<MyFoo>;
//     ~MyFoo();
//   };
//
// You should always make your destructor non-public, to avoid any code deleting
// the object accidently while there are references to it.
//
//
// The ref count manipulation to RefCounted is NOT thread safe and has DCHECKs
// to trap unsafe cross thread usage. A subclass instance of RefCounted can be
// passed to another execution sequence only when its ref count is 1. If the ref
// count is more than 1, the RefCounted class verifies the ref updates are made
// on the same execution sequence as the previous ones. The subclass can also
// manually call IsOnValidSequence to trap other non-thread-safe accesses; see
// the documentation for that method.
//
//
// The reference count starts from zero by default, and we intended to migrate
// to start-from-one ref count. Put REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE() to
// the ref counted class to opt-in.
//
// If an object has start-from-one ref count, the first scoped_refptr need to be
// created by base::AdoptRef() or base::MakeRefCounted(). We can use
// base::MakeRefCounted() to create create both type of ref counted object.
//
// The motivations to use start-from-one ref count are:
//  - Start-from-one ref count doesn't need the ref count increment for the
//    first reference.
//  - It can detect an invalid object acquisition for a being-deleted object
//    that has zero ref count. That tends to happen on custom deleter that
//    delays the deletion.
//    TODO(tzik): Implement invalid acquisition detection.
//  - Behavior parity to Blink's WTF::RefCounted, whose count starts from one.
//    And start-from-one ref count is a step to merge WTF::RefCounted into
//    base::RefCounted.
//
#define REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE()             \
  static constexpr ::base::subtle::StartRefCountFromOneTag \
      kRefCountPreference = ::base::subtle::kStartRefCountFromOneTag

template <class T, typename Traits>
class RefCounted;

template <typename T>
struct DefaultRefCountedTraits {
  static void Destruct(const T* x) {
    RefCounted<T, DefaultRefCountedTraits>::DeleteInternal(x);
  }
};

template <class T, typename Traits = DefaultRefCountedTraits<T>>
class RefCounted : public subtle::RefCountedBase {
 public:
  static constexpr subtle::StartRefCountFromZeroTag kRefCountPreference =
      subtle::kStartRefCountFromZeroTag;

  RefCounted() : subtle::RefCountedBase(T::kRefCountPreference) {}

  void AddRef() const {
    subtle::RefCountedBase::AddRef();
  }

  void Release() const {
    if (subtle::RefCountedBase::Release()) {
      // Prune the code paths which the static analyzer may take to simulate
      // object destruction. Use-after-free errors aren't possible given the
      // lifetime guarantees of the refcounting system.
      // ANALYZER_SKIP_THIS_PATH();

      Traits::Destruct(static_cast<const T*>(this));
    }
  }

 protected:
  ~RefCounted() = default;

 private:
  friend struct DefaultRefCountedTraits<T>;
  template <typename U>
  static void DeleteInternal(const U* x) {
    delete x;
  }

  DISALLOW_COPY_AND_ASSIGN(RefCounted);
};

}  // namespace base

#endif  // BASE_MEMORY_REF_COUNTED_H_
