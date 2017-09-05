// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LazyInstance<Type, Traits> class manages a single instance of Type,
// which will be lazily created on the first time it's accessed.  This class is
// useful for places you would normally use a function-level static, but you
// need to have guaranteed thread-safety.  The Type constructor will only ever
// be called once, even if two threads are racing to create the object.  Get()
// and Pointer() will always return the same, completely initialized instance.
// When the instance is constructed it is registered with AtExitManager.  The
// destructor will be called on program exit.
//
// LazyInstance is completely thread safe, assuming that you create it safely.
// The class was designed to be POD initialized, so it shouldn't require a
// static constructor.  It really only makes sense to declare a LazyInstance as
// a global variable using the LAZY_INSTANCE_INITIALIZER initializer.
//
// LazyInstance is similar to Singleton, except it does not have the singleton
// property.  You can have multiple LazyInstance's of the same type, and each
// will manage a unique instance.  It also preallocates the space for Type, as
// to avoid allocating the Type instance on the heap.  This may help with the
// performance of creating the instance, and reducing heap fragmentation.  This
// requires that Type be a complete type so we can determine the size.
//
// Example usage:
//   static LazyInstance<MyClass>::Leaky inst = LAZY_INSTANCE_INITIALIZER;
//   void SomeMethod() {
//     inst.Get().SomeMethod();  // MyClass::SomeMethod()
//
//     MyClass* ptr = inst.Pointer();
//     ptr->DoDoDo();  // MyClass::DoDoDo
//   }

#ifndef BASE_LAZY_INSTANCE_H_
#define BASE_LAZY_INSTANCE_H_

#include <new>  // For placement new.

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

// LazyInstance uses its own struct initializer-list style static
// initialization, which does not require a constructor.
#define LAZY_INSTANCE_INITIALIZER {0}

namespace base {

template <typename Type>
struct LazyInstanceTraitsBase {
  static Type* New(void* instance) {
    DCHECK_EQ(reinterpret_cast<uintptr_t>(instance) & (alignof(Type) - 1), 0u);
    // Use placement new to initialize our instance in our preallocated space.
    // The parenthesis is very important here to force POD type initialization.
    return new (instance) Type();
  }

  static void CallDestructor(Type* instance) {
    // Explicitly call the destructor.
    instance->~Type();
  }
};

// We pull out some of the functionality into non-templated functions, so we
// can implement the more complicated pieces out of line in the .cc file.
namespace internal {

// This traits class causes destruction the contained Type at process exit via
// AtExitManager. This is probably generally not what you want. Instead, prefer
// Leaky below.
template <typename Type>
struct DestructorAtExitLazyInstanceTraits {
  static const bool kRegisterOnExit = true;
#if DCHECK_IS_ON()
  static const bool kAllowedToAccessOnNonjoinableThread = false;
#endif

  static Type* New(void* instance) {
    return LazyInstanceTraitsBase<Type>::New(instance);
  }

  static void Delete(Type* instance) {
    LazyInstanceTraitsBase<Type>::CallDestructor(instance);
  }
};

// Use LazyInstance<T>::Leaky for a less-verbose call-site typedef; e.g.:
// base::LazyInstance<T>::Leaky my_leaky_lazy_instance;
// instead of:
// base::LazyInstance<T, base::internal::LeakyLazyInstanceTraits<T> >
// my_leaky_lazy_instance;
// (especially when T is MyLongTypeNameImplClientHolderFactory).
// Only use this internal::-qualified verbose form to extend this traits class
// (depending on its implementation details).
template <typename Type>
struct LeakyLazyInstanceTraits {
  static const bool kRegisterOnExit = false;
#if DCHECK_IS_ON()
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static Type* New(void* instance) {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    return LazyInstanceTraitsBase<Type>::New(instance);
  }
  static void Delete(Type* instance) {
  }
};

template <typename Type>
struct ErrorMustSelectLazyOrDestructorAtExitForLazyInstance {};

// Our AtomicWord doubles as a spinlock, where a value of
// kLazyInstanceStateCreating means the spinlock is being held for creation.
constexpr subtle::AtomicWord kLazyInstanceStateCreating = 1;

// Check if instance needs to be created. If so return true otherwise
// if another thread has beat us, wait for instance to be created and
// return false.
BASE_EXPORT bool NeedsLazyInstance(subtle::AtomicWord* state);

// After creating an instance, call this to register the dtor to be called
// at program exit and to update the atomic state to hold the |new_instance|
BASE_EXPORT void CompleteLazyInstance(subtle::AtomicWord* state,
                                      subtle::AtomicWord new_instance,
                                      void (*destructor)(void*),
                                      void* destructor_arg);

// If |state| is uninitialized, constructs a value using |creator_func|, stores
// it into |state| and registers |destructor| to be called with |destructor_arg|
// as argument when the current AtExitManager goes out of scope. Then, returns
// the value stored in |state|. It is safe to have concurrent calls to this
// function with the same |state|.
template <typename CreatorFunc>
void* GetOrCreateLazyPointer(subtle::AtomicWord* state,
                             const CreatorFunc& creator_func,
                             void (*destructor)(void*),
                             void* destructor_arg) {
  // If any bit in the created mask is true, the instance has already been
  // fully constructed.
  constexpr subtle::AtomicWord kLazyInstanceCreatedMask =
      ~internal::kLazyInstanceStateCreating;

  // We will hopefully have fast access when the instance is already created.
  // Since a thread sees |state| == 0 or kLazyInstanceStateCreating at most
  // once, the load is taken out of NeedsLazyInstance() as a fast-path. The load
  // has acquire memory ordering as a thread which sees |state| > creating needs
  // to acquire visibility over the associated data. Pairing Release_Store is in
  // CompleteLazyInstance().
  subtle::AtomicWord value = subtle::Acquire_Load(state);
  if (!(value & kLazyInstanceCreatedMask) && NeedsLazyInstance(state)) {
    // Create the instance in the space provided by |private_buf_|.
    value = reinterpret_cast<subtle::AtomicWord>(creator_func());
    CompleteLazyInstance(state, value, destructor, destructor_arg);
  }
  return reinterpret_cast<void*>(subtle::NoBarrier_Load(state));
}

}  // namespace internal

template <
    typename Type,
    typename Traits =
        internal::ErrorMustSelectLazyOrDestructorAtExitForLazyInstance<Type>>
class LazyInstance {
 public:
  // Do not define a destructor, as doing so makes LazyInstance a
  // non-POD-struct. We don't want that because then a static initializer will
  // be created to register the (empty) destructor with atexit() under MSVC, for
  // example. We handle destruction of the contained Type class explicitly via
  // the OnExit member function, where needed.
  // ~LazyInstance() {}

  // Convenience typedef to avoid having to repeat Type for leaky lazy
  // instances.
  typedef LazyInstance<Type, internal::LeakyLazyInstanceTraits<Type>> Leaky;
  typedef LazyInstance<Type, internal::DestructorAtExitLazyInstanceTraits<Type>>
      DestructorAtExit;

  Type& Get() {
    return *Pointer();
  }

  Type* Pointer() {
#if DCHECK_IS_ON()
    // Avoid making TLS lookup on release builds.
    if (!Traits::kAllowedToAccessOnNonjoinableThread)
      ThreadRestrictions::AssertSingletonAllowed();
#endif
    return static_cast<Type*>(internal::GetOrCreateLazyPointer(
        &private_instance_,
        [this]() { return Traits::New(private_buf_); },
        Traits::kRegisterOnExit ? OnExit : nullptr, this));
  }

  bool operator==(Type* p) {
    switch (subtle::NoBarrier_Load(&private_instance_)) {
      case 0:
        return p == NULL;
      case internal::kLazyInstanceStateCreating:
        return static_cast<void*>(p) == private_buf_;
      default:
        return p == instance();
    }
  }

  // MSVC gives a warning that the alignment expands the size of the
  // LazyInstance struct to make the size a multiple of the alignment. This
  // is expected in this case.
#if defined(OS_WIN)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

  // Effectively private: member data is only public to allow the linker to
  // statically initialize it and to maintain a POD class. DO NOT USE FROM
  // OUTSIDE THIS CLASS.
  subtle::AtomicWord private_instance_;

  // Preallocated space for the Type instance.
  alignas(Type) char private_buf_[sizeof(Type)];

#if defined(OS_WIN)
#pragma warning(pop)
#endif

 private:
  Type* instance() {
    return reinterpret_cast<Type*>(subtle::NoBarrier_Load(&private_instance_));
  }

  // Adapter function for use with AtExit.  This should be called single
  // threaded, so don't synchronize across threads.
  // Calling OnExit while the instance is in use by other threads is a mistake.
  static void OnExit(void* lazy_instance) {
    LazyInstance<Type, Traits>* me =
        reinterpret_cast<LazyInstance<Type, Traits>*>(lazy_instance);
    Traits::Delete(me->instance());
    subtle::NoBarrier_Store(&me->private_instance_, 0);
  }
};

}  // namespace base

#endif  // BASE_LAZY_INSTANCE_H_
