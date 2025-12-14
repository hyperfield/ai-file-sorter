#line 1 "/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-gcc-14.3.0/include/c++/14.3.0/x86_64-unknown-linux-gnu/bits/c++config.h"
// Predefined symbols and macros -*- C++ -*-

// Copyright (C) 1997-2024 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/c++config.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{version}
 */

#ifndef _GLIBCXX_CXX_CONFIG_H
#define _GLIBCXX_CXX_CONFIG_H 1

#pragma GCC system_header

// The major release number for the GCC release the C++ library belongs to.
#define _GLIBCXX_RELEASE 14

// The datestamp of the C++ library in compressed ISO date format.
#define __GLIBCXX__ 20250523

// Macros for various attributes.
//   _GLIBCXX_PURE
//   _GLIBCXX_CONST
//   _GLIBCXX_NORETURN
//   _GLIBCXX_NOTHROW
//   _GLIBCXX_VISIBILITY
#ifndef _GLIBCXX_PURE
# define _GLIBCXX_PURE __attribute__ ((__pure__))
#endif

#ifndef _GLIBCXX_CONST
# define _GLIBCXX_CONST __attribute__ ((__const__))
#endif

#ifndef _GLIBCXX_NORETURN
# define _GLIBCXX_NORETURN __attribute__ ((__noreturn__))
#endif

// See below for C++
#ifndef _GLIBCXX_NOTHROW
# ifndef __cplusplus
#  define _GLIBCXX_NOTHROW __attribute__((__nothrow__))
# endif
#endif

// Macros for visibility attributes.
//   _GLIBCXX_HAVE_ATTRIBUTE_VISIBILITY
//   _GLIBCXX_VISIBILITY
# define _GLIBCXX_HAVE_ATTRIBUTE_VISIBILITY 1

#if _GLIBCXX_HAVE_ATTRIBUTE_VISIBILITY
# define _GLIBCXX_VISIBILITY(V) __attribute__ ((__visibility__ (#V)))
#else
// If this is not supplied by the OS-specific or CPU-specific
// headers included below, it will be defined to an empty default.
# define _GLIBCXX_VISIBILITY(V) _GLIBCXX_PSEUDO_VISIBILITY(V)
#endif

// Macros for deprecated attributes.
//   _GLIBCXX_USE_DEPRECATED
//   _GLIBCXX_DEPRECATED
//   _GLIBCXX_DEPRECATED_SUGGEST( string-literal )
//   _GLIBCXX11_DEPRECATED
//   _GLIBCXX11_DEPRECATED_SUGGEST( string-literal )
//   _GLIBCXX14_DEPRECATED
//   _GLIBCXX14_DEPRECATED_SUGGEST( string-literal )
//   _GLIBCXX17_DEPRECATED
//   _GLIBCXX17_DEPRECATED_SUGGEST( string-literal )
//   _GLIBCXX20_DEPRECATED
//   _GLIBCXX20_DEPRECATED_SUGGEST( string-literal )
//   _GLIBCXX23_DEPRECATED
//   _GLIBCXX23_DEPRECATED_SUGGEST( string-literal )
#ifndef _GLIBCXX_USE_DEPRECATED
# define _GLIBCXX_USE_DEPRECATED 1
#endif

#if defined(__DEPRECATED)
# define _GLIBCXX_DEPRECATED __attribute__ ((__deprecated__))
# define _GLIBCXX_DEPRECATED_SUGGEST(ALT) \
  __attribute__ ((__deprecated__ ("use '" ALT "' instead")))
#else
# define _GLIBCXX_DEPRECATED
# define _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#endif

#if defined(__DEPRECATED) && (__cplusplus >= 201103L)
# define _GLIBCXX11_DEPRECATED _GLIBCXX_DEPRECATED
# define _GLIBCXX11_DEPRECATED_SUGGEST(ALT) _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#else
# define _GLIBCXX11_DEPRECATED
# define _GLIBCXX11_DEPRECATED_SUGGEST(ALT)
#endif

#if defined(__DEPRECATED) && (__cplusplus >= 201402L)
# define _GLIBCXX14_DEPRECATED _GLIBCXX_DEPRECATED
# define _GLIBCXX14_DEPRECATED_SUGGEST(ALT) _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#else
# define _GLIBCXX14_DEPRECATED
# define _GLIBCXX14_DEPRECATED_SUGGEST(ALT)
#endif

#if defined(__DEPRECATED) && (__cplusplus >= 201703L)
# define _GLIBCXX17_DEPRECATED [[__deprecated__]]
# define _GLIBCXX17_DEPRECATED_SUGGEST(ALT) _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#else
# define _GLIBCXX17_DEPRECATED
# define _GLIBCXX17_DEPRECATED_SUGGEST(ALT)
#endif

#if defined(__DEPRECATED) && (__cplusplus >= 202002L)
# define _GLIBCXX20_DEPRECATED [[__deprecated__]]
# define _GLIBCXX20_DEPRECATED_SUGGEST(ALT) _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#else
# define _GLIBCXX20_DEPRECATED
# define _GLIBCXX20_DEPRECATED_SUGGEST(ALT)
#endif

#if defined(__DEPRECATED) && (__cplusplus >= 202100L)
# define _GLIBCXX23_DEPRECATED [[__deprecated__]]
# define _GLIBCXX23_DEPRECATED_SUGGEST(ALT) _GLIBCXX_DEPRECATED_SUGGEST(ALT)
#else
# define _GLIBCXX23_DEPRECATED
# define _GLIBCXX23_DEPRECATED_SUGGEST(ALT)
#endif

// Macros for ABI tag attributes.
#ifndef _GLIBCXX_ABI_TAG_CXX11
# define _GLIBCXX_ABI_TAG_CXX11 __attribute ((__abi_tag__ ("cxx11")))
#endif

// Macro to warn about unused results.
#if __cplusplus >= 201703L
# define _GLIBCXX_NODISCARD [[__nodiscard__]]
#else
# define _GLIBCXX_NODISCARD
#endif



#if __cplusplus

// Macro for constexpr, to support in mixed 03/0x mode.
#ifndef _GLIBCXX_CONSTEXPR
# if __cplusplus >= 201103L
#  define _GLIBCXX_CONSTEXPR constexpr
#  define _GLIBCXX_USE_CONSTEXPR constexpr
# else
#  define _GLIBCXX_CONSTEXPR
#  define _GLIBCXX_USE_CONSTEXPR const
# endif
#endif

#ifndef _GLIBCXX14_CONSTEXPR
# if __cplusplus >= 201402L
#  define _GLIBCXX14_CONSTEXPR constexpr
# else
#  define _GLIBCXX14_CONSTEXPR
# endif
#endif

#ifndef _GLIBCXX17_CONSTEXPR
# if __cplusplus >= 201703L
#  define _GLIBCXX17_CONSTEXPR constexpr
# else
#  define _GLIBCXX17_CONSTEXPR
# endif
#endif

#ifndef _GLIBCXX20_CONSTEXPR
# if __cplusplus >= 202002L
#  define _GLIBCXX20_CONSTEXPR constexpr
# else
#  define _GLIBCXX20_CONSTEXPR
# endif
#endif

#ifndef _GLIBCXX23_CONSTEXPR
# if __cplusplus >= 202100L
#  define _GLIBCXX23_CONSTEXPR constexpr
# else
#  define _GLIBCXX23_CONSTEXPR
# endif
#endif

#ifndef _GLIBCXX17_INLINE
# if __cplusplus >= 201703L
#  define _GLIBCXX17_INLINE inline
# else
#  define _GLIBCXX17_INLINE
# endif
#endif

// Macro for noexcept, to support in mixed 03/0x mode.
#ifndef _GLIBCXX_NOEXCEPT
# if __cplusplus >= 201103L
#  define _GLIBCXX_NOEXCEPT noexcept
#  define _GLIBCXX_NOEXCEPT_IF(...) noexcept(__VA_ARGS__)
#  define _GLIBCXX_USE_NOEXCEPT noexcept
#  define _GLIBCXX_THROW(_EXC)
# else
#  define _GLIBCXX_NOEXCEPT
#  define _GLIBCXX_NOEXCEPT_IF(...)
#  define _GLIBCXX_USE_NOEXCEPT throw()
#  define _GLIBCXX_THROW(_EXC) throw(_EXC)
# endif
#endif

#ifndef _GLIBCXX_NOTHROW
# define _GLIBCXX_NOTHROW _GLIBCXX_USE_NOEXCEPT
#endif

#ifndef _GLIBCXX_THROW_OR_ABORT
# if __cpp_exceptions
#  define _GLIBCXX_THROW_OR_ABORT(_EXC) (throw (_EXC))
# else
#  define _GLIBCXX_THROW_OR_ABORT(_EXC) (__builtin_abort())
# endif
#endif

#if __cpp_noexcept_function_type
#define _GLIBCXX_NOEXCEPT_PARM , bool _NE
#define _GLIBCXX_NOEXCEPT_QUAL noexcept (_NE)
#else
#define _GLIBCXX_NOEXCEPT_PARM
#define _GLIBCXX_NOEXCEPT_QUAL
#endif

// Macro for extern template, ie controlling template linkage via use
// of extern keyword on template declaration. As documented in the g++
// manual, it inhibits all implicit instantiations and is used
// throughout the library to avoid multiple weak definitions for
// required types that are already explicitly instantiated in the
// library binary. This substantially reduces the binary size of
// resulting executables.
// Special case: _GLIBCXX_EXTERN_TEMPLATE == -1 disallows extern
// templates only in basic_string, thus activating its debug-mode
// checks even at -O0.
# define _GLIBCXX_EXTERN_TEMPLATE 1

/*
  Outline of libstdc++ namespaces.

  namespace std
  {
    namespace __debug { }
    namespace __parallel { }
    namespace __cxx1998 { }

    namespace __detail {
      namespace __variant { }				// C++17
    }

    namespace rel_ops { }

    namespace tr1
    {
      namespace placeholders { }
      namespace regex_constants { }
      namespace __detail { }
    }

    namespace tr2 { }
    
    namespace decimal { }

    namespace chrono { }				// C++11
    namespace placeholders { }				// C++11
    namespace regex_constants { }			// C++11
    namespace this_thread { }				// C++11
    inline namespace literals {				// C++14
      inline namespace chrono_literals { }		// C++14
      inline namespace complex_literals { }		// C++14
      inline namespace string_literals { }		// C++14
      inline namespace string_view_literals { }		// C++17
    }
  }

  namespace abi { }

  namespace __gnu_cxx
  {
    namespace __detail { }
  }

  For full details see:
  http://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/namespaces.html
*/
namespace std
{
  typedef __SIZE_TYPE__ 	size_t;
  typedef __PTRDIFF_TYPE__	ptrdiff_t;

#if __cplusplus >= 201103L
  typedef decltype(nullptr)	nullptr_t;
#endif

#pragma GCC visibility push(default)
  // This allows the library to terminate without including all of <exception>
  // and without making the declaration of std::terminate visible to users.
  extern "C++" __attribute__ ((__noreturn__, __always_inline__))
  inline void __terminate() _GLIBCXX_USE_NOEXCEPT
  {
    void terminate() _GLIBCXX_USE_NOEXCEPT __attribute__ ((__noreturn__,__cold__));
    terminate();
  }
#pragma GCC visibility pop
}

# define _GLIBCXX_USE_DUAL_ABI 1

#if ! _GLIBCXX_USE_DUAL_ABI
// Ignore any pre-defined value of _GLIBCXX_USE_CXX11_ABI
# undef _GLIBCXX_USE_CXX11_ABI
#endif

#ifndef _GLIBCXX_USE_CXX11_ABI
# define _GLIBCXX_USE_CXX11_ABI 1
#endif

#if _GLIBCXX_USE_CXX11_ABI
namespace std
{
  inline namespace __cxx11 __attribute__((__abi_tag__ ("cxx11"))) { }
}
namespace __gnu_cxx
{
  inline namespace __cxx11 __attribute__((__abi_tag__ ("cxx11"))) { }
}
# define _GLIBCXX_NAMESPACE_CXX11 __cxx11::
# define _GLIBCXX_BEGIN_NAMESPACE_CXX11 namespace __cxx11 {
# define _GLIBCXX_END_NAMESPACE_CXX11 }
# define _GLIBCXX_DEFAULT_ABI_TAG _GLIBCXX_ABI_TAG_CXX11
#else
# define _GLIBCXX_NAMESPACE_CXX11
# define _GLIBCXX_BEGIN_NAMESPACE_CXX11
# define _GLIBCXX_END_NAMESPACE_CXX11
# define _GLIBCXX_DEFAULT_ABI_TAG
#endif

// Non-zero if inline namespaces are used for versioning the entire library.
# define _GLIBCXX_INLINE_VERSION 0 

#if _GLIBCXX_INLINE_VERSION
// Inline namespace for symbol versioning of (nearly) everything in std.
# define _GLIBCXX_BEGIN_NAMESPACE_VERSION namespace __8 {
# define _GLIBCXX_END_NAMESPACE_VERSION }
// Unused when everything in std is versioned anyway.
# define _GLIBCXX_BEGIN_INLINE_ABI_NAMESPACE(X)
# define _GLIBCXX_END_INLINE_ABI_NAMESPACE(X)

namespace std
{
inline _GLIBCXX_BEGIN_NAMESPACE_VERSION
#if __cplusplus >= 201402L
  inline namespace literals {
    inline namespace chrono_literals { }
    inline namespace complex_literals { }
    inline namespace string_literals { }
#if __cplusplus > 201402L
    inline namespace string_view_literals { }
#endif // C++17
  }
#endif // C++14
_GLIBCXX_END_NAMESPACE_VERSION
}

namespace __gnu_cxx
{
inline _GLIBCXX_BEGIN_NAMESPACE_VERSION
_GLIBCXX_END_NAMESPACE_VERSION
}

#else
// Unused.
# define _GLIBCXX_BEGIN_NAMESPACE_VERSION
# define _GLIBCXX_END_NAMESPACE_VERSION
// Used to version individual components, e.g. std::_V2::error_category.
# define _GLIBCXX_BEGIN_INLINE_ABI_NAMESPACE(X) inline namespace X {
# define _GLIBCXX_END_INLINE_ABI_NAMESPACE(X)   } // inline namespace X
#endif

// In the case that we don't have a hosted environment, we can't provide the
// debugging mode.  Instead, we do our best and downgrade to assertions.
#if defined(_GLIBCXX_DEBUG) && !__STDC_HOSTED__
#undef _GLIBCXX_DEBUG
#define _GLIBCXX_ASSERTIONS 1
#endif

// Inline namespaces for special modes: debug, parallel.
#if defined(_GLIBCXX_DEBUG) || defined(_GLIBCXX_PARALLEL)
namespace std
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  // Non-inline namespace for components replaced by alternates in active mode.
  namespace __cxx1998
  {
# if _GLIBCXX_USE_CXX11_ABI
  inline namespace __cxx11 __attribute__((__abi_tag__ ("cxx11"))) { }
# endif
  }

_GLIBCXX_END_NAMESPACE_VERSION

  // Inline namespace for debug mode.
# ifdef _GLIBCXX_DEBUG
  inline namespace __debug { }
# endif

  // Inline namespaces for parallel mode.
# ifdef _GLIBCXX_PARALLEL
  inline namespace __parallel { }
# endif
}

// Check for invalid usage and unsupported mixed-mode use.
# if defined(_GLIBCXX_DEBUG) && defined(_GLIBCXX_PARALLEL)
#  error illegal use of multiple inlined namespaces
# endif

// Check for invalid use due to lack for weak symbols.
# if __NO_INLINE__ && !__GXX_WEAK__
#  warning currently using inlined namespace mode which may fail \
   without inlining due to lack of weak symbols
# endif
#endif

// Macros for namespace scope. Either namespace std:: or the name
// of some nested namespace within it corresponding to the active mode.
// _GLIBCXX_STD_A
// _GLIBCXX_STD_C
//
// Macros for opening/closing conditional namespaces.
// _GLIBCXX_BEGIN_NAMESPACE_ALGO
// _GLIBCXX_END_NAMESPACE_ALGO
// _GLIBCXX_BEGIN_NAMESPACE_CONTAINER
// _GLIBCXX_END_NAMESPACE_CONTAINER
#if defined(_GLIBCXX_DEBUG)
# define _GLIBCXX_STD_C __cxx1998
# define _GLIBCXX_BEGIN_NAMESPACE_CONTAINER \
	 namespace _GLIBCXX_STD_C {
# define _GLIBCXX_END_NAMESPACE_CONTAINER }
#else
# define _GLIBCXX_STD_C std
# define _GLIBCXX_BEGIN_NAMESPACE_CONTAINER
# define _GLIBCXX_END_NAMESPACE_CONTAINER
#endif

#ifdef _GLIBCXX_PARALLEL
# define _GLIBCXX_STD_A __cxx1998
# define _GLIBCXX_BEGIN_NAMESPACE_ALGO \
	 namespace _GLIBCXX_STD_A {
# define _GLIBCXX_END_NAMESPACE_ALGO }
#else
# define _GLIBCXX_STD_A std
# define _GLIBCXX_BEGIN_NAMESPACE_ALGO
# define _GLIBCXX_END_NAMESPACE_ALGO
#endif

// GLIBCXX_ABI Deprecated
// Define if compatibility should be provided for -mlong-double-64.
#undef _GLIBCXX_LONG_DOUBLE_COMPAT

// Define if compatibility should be provided for alternative 128-bit long
// double formats. Not possible for Clang until __ibm128 is supported.
#ifndef __clang__
#undef _GLIBCXX_LONG_DOUBLE_ALT128_COMPAT
#endif

// Inline namespaces for long double 128 modes.
#if defined _GLIBCXX_LONG_DOUBLE_ALT128_COMPAT \
  && defined __LONG_DOUBLE_IEEE128__
namespace std
{
  // Namespaces for 128-bit IEEE long double format on 64-bit POWER LE.
  inline namespace __gnu_cxx_ieee128 { }
  inline namespace __gnu_cxx11_ieee128 { }
}
# define _GLIBCXX_NAMESPACE_LDBL __gnu_cxx_ieee128::
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL namespace __gnu_cxx_ieee128 {
# define _GLIBCXX_END_NAMESPACE_LDBL }
# define _GLIBCXX_NAMESPACE_LDBL_OR_CXX11 __gnu_cxx11_ieee128::
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL_OR_CXX11 namespace __gnu_cxx11_ieee128 {
# define _GLIBCXX_END_NAMESPACE_LDBL_OR_CXX11 }

#else // _GLIBCXX_LONG_DOUBLE_ALT128_COMPAT && IEEE128

#if defined _GLIBCXX_LONG_DOUBLE_COMPAT && defined __LONG_DOUBLE_128__
namespace std
{
  inline namespace __gnu_cxx_ldbl128 { }
}
# define _GLIBCXX_NAMESPACE_LDBL __gnu_cxx_ldbl128::
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL namespace __gnu_cxx_ldbl128 {
# define _GLIBCXX_END_NAMESPACE_LDBL }
#else
# define _GLIBCXX_NAMESPACE_LDBL
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL
# define _GLIBCXX_END_NAMESPACE_LDBL
#endif

#if _GLIBCXX_USE_CXX11_ABI
# define _GLIBCXX_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_NAMESPACE_CXX11
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_BEGIN_NAMESPACE_CXX11
# define _GLIBCXX_END_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_END_NAMESPACE_CXX11
#else
# define _GLIBCXX_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_NAMESPACE_LDBL
# define _GLIBCXX_BEGIN_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_BEGIN_NAMESPACE_LDBL
# define _GLIBCXX_END_NAMESPACE_LDBL_OR_CXX11 _GLIBCXX_END_NAMESPACE_LDBL
#endif

#endif // _GLIBCXX_LONG_DOUBLE_ALT128_COMPAT && IEEE128

namespace std
{
#pragma GCC visibility push(default)
  // Internal version of std::is_constant_evaluated().
  // This can be used without checking if the compiler supports the feature.
  // The macro _GLIBCXX_HAVE_IS_CONSTANT_EVALUATED can be used to check if
  // the compiler support is present to make this function work as expected.
  __attribute__((__always_inline__))
  _GLIBCXX_CONSTEXPR inline bool
  __is_constant_evaluated() _GLIBCXX_NOEXCEPT
  {
#if __cpp_if_consteval >= 202106L
# define _GLIBCXX_HAVE_IS_CONSTANT_EVALUATED 1
    if consteval { return true; } else { return false; }
#elif __cplusplus >= 201103L && __has_builtin(__builtin_is_constant_evaluated)
# define _GLIBCXX_HAVE_IS_CONSTANT_EVALUATED 1
    return __builtin_is_constant_evaluated();
#else
    return false;
#endif
  }
#pragma GCC visibility pop
}

// Debug Mode implies checking assertions.
#if defined(_GLIBCXX_DEBUG) && !defined(_GLIBCXX_ASSERTIONS)
# define _GLIBCXX_ASSERTIONS 1
#endif

// Disable std::string explicit instantiation declarations in order to assert.
#ifdef _GLIBCXX_ASSERTIONS
# undef _GLIBCXX_EXTERN_TEMPLATE
# define _GLIBCXX_EXTERN_TEMPLATE -1
#endif

#define _GLIBCXX_VERBOSE_ASSERT 1

// Assert.
#ifdef _GLIBCXX_VERBOSE_ASSERT
namespace std
{
#pragma GCC visibility push(default)
  // Don't use <cassert> because this should be unaffected by NDEBUG.
  extern "C++" _GLIBCXX_NORETURN
  void
  __glibcxx_assert_fail /* Called when a precondition violation is detected. */
    (const char* __file, int __line, const char* __function,
     const char* __condition)
  _GLIBCXX_NOEXCEPT;
#pragma GCC visibility pop
}
# define _GLIBCXX_ASSERT_FAIL(_Condition)				\
  std::__glibcxx_assert_fail(__FILE__, __LINE__, __PRETTY_FUNCTION__,	\
			     #_Condition)
#else // ! VERBOSE_ASSERT
# define _GLIBCXX_ASSERT_FAIL(_Condition) __builtin_abort()
#endif

#if defined(_GLIBCXX_ASSERTIONS)
// When _GLIBCXX_ASSERTIONS is defined we enable runtime assertion checks.
// These checks will also be done during constant evaluation.
# define __glibcxx_assert(cond)						\
  do {									\
    if (__builtin_expect(!bool(cond), false))				\
      _GLIBCXX_ASSERT_FAIL(cond);					\
  } while (false)
#elif _GLIBCXX_HAVE_IS_CONSTANT_EVALUATED
// _GLIBCXX_ASSERTIONS is not defined, so assertions checks are only enabled
// during constant evaluation. This ensures we diagnose undefined behaviour
// in constant expressions.
namespace std
{
  __attribute__((__always_inline__,__visibility__("default")))
  inline void
  __glibcxx_assert_fail()
  { }
}
# define __glibcxx_assert(cond)						\
  do {									\
    if (std::__is_constant_evaluated() && !bool(cond))			\
      std::__glibcxx_assert_fail();					\
  } while (false)
#else
// _GLIBCXX_ASSERTIONS is not defined and __is_constant_evaluated() doesn't
// work so don't check any assertions.
# define __glibcxx_assert(cond)
#endif

// Macro indicating that TSAN is in use.
#if __SANITIZE_THREAD__
#  define _GLIBCXX_TSAN 1
#elif defined __has_feature
# if __has_feature(thread_sanitizer)
#  define _GLIBCXX_TSAN 1
# endif
#endif

// Macros for race detectors.
// _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A) and
// _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A) should be used to explain
// atomic (lock-free) synchronization to race detectors:
// the race detector will infer a happens-before arc from the former to the
// latter when they share the same argument pointer.
//
// The most frequent use case for these macros (and the only case in the
// current implementation of the library) is atomic reference counting:
//   void _M_remove_reference()
//   {
//     _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(&this->_M_refcount);
//     if (__gnu_cxx::__exchange_and_add_dispatch(&this->_M_refcount, -1) <= 0)
//       {
//         _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(&this->_M_refcount);
//         _M_destroy(__a);
//       }
//   }
// The annotations in this example tell the race detector that all memory
// accesses occurred when the refcount was positive do not race with
// memory accesses which occurred after the refcount became zero.
#ifndef _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE
# define  _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A)
#endif
#ifndef _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER
# define  _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A)
#endif

// Macros for C linkage: define extern "C" linkage only when using C++.
# define _GLIBCXX_BEGIN_EXTERN_C extern "C" {
# define _GLIBCXX_END_EXTERN_C }

# define _GLIBCXX_USE_ALLOCATOR_NEW 1

#ifdef __SIZEOF_INT128__
#if ! defined __GLIBCXX_TYPE_INT_N_0 && ! defined __STRICT_ANSI__
// If __int128 is supported, we expect __GLIBCXX_TYPE_INT_N_0 to be defined
// unless the compiler is in strict mode. If it's not defined and the strict
// macro is not defined, something is wrong.
#warning "__STRICT_ANSI__ seems to have been undefined; this is not supported"
#endif
#endif

#else // !__cplusplus
# define _GLIBCXX_BEGIN_EXTERN_C
# define _GLIBCXX_END_EXTERN_C
#endif


// First includes.

// Pick up any OS-specific definitions.
#include <bits/os_defines.h>

// Pick up any CPU-specific definitions.
#include <bits/cpu_defines.h>

// If platform uses neither visibility nor psuedo-visibility,
// specify empty default for namespace annotation macros.
#ifndef _GLIBCXX_PSEUDO_VISIBILITY
# define _GLIBCXX_PSEUDO_VISIBILITY(V)
#endif

// Certain function definitions that are meant to be overridable from
// user code are decorated with this macro.  For some targets, this
// macro causes these definitions to be weak.
#ifndef _GLIBCXX_WEAK_DEFINITION
# define _GLIBCXX_WEAK_DEFINITION
#endif

// By default, we assume that __GXX_WEAK__ also means that there is support
// for declaring functions as weak while not defining such functions.  This
// allows for referring to functions provided by other libraries (e.g.,
// libitm) without depending on them if the respective features are not used.
#ifndef _GLIBCXX_USE_WEAK_REF
# define _GLIBCXX_USE_WEAK_REF __GXX_WEAK__
#endif

// Conditionally enable annotations for the Transactional Memory TS on C++11.
// Most of the following conditions are due to limitations in the current
// implementation.
#if __cplusplus >= 201103L && _GLIBCXX_USE_CXX11_ABI			\
  && _GLIBCXX_USE_DUAL_ABI && __cpp_transactional_memory >= 201500L	\
  &&  !_GLIBCXX_FULLY_DYNAMIC_STRING && _GLIBCXX_USE_WEAK_REF		\
  && _GLIBCXX_USE_ALLOCATOR_NEW
#define _GLIBCXX_TXN_SAFE transaction_safe
#define _GLIBCXX_TXN_SAFE_DYN transaction_safe_dynamic
#else
#define _GLIBCXX_TXN_SAFE
#define _GLIBCXX_TXN_SAFE_DYN
#endif

#if __cplusplus > 201402L
// In C++17 mathematical special functions are in namespace std.
# define _GLIBCXX_USE_STD_SPEC_FUNCS 1
#elif __cplusplus >= 201103L && __STDCPP_WANT_MATH_SPEC_FUNCS__ != 0
// For C++11 and C++14 they are in namespace std when requested.
# define _GLIBCXX_USE_STD_SPEC_FUNCS 1
#endif

// The remainder of the prewritten config is automatic; all the
// user hooks are listed above.

// Create a boolean flag to be used to determine if --fast-math is set.
#ifdef __FAST_MATH__
# define _GLIBCXX_FAST_MATH 1
#else
# define _GLIBCXX_FAST_MATH 0
#endif

// This marks string literals in header files to be extracted for eventual
// translation.  It is primarily used for messages in thrown exceptions; see
// src/functexcept.cc.  We use __N because the more traditional _N is used
// for something else under certain OSes (see BADNAMES).
#define __N(msgid)     (msgid)

// For example, <windows.h> is known to #define min and max as macros...
#undef min
#undef max

// N.B. these _GLIBCXX_USE_C99_XXX macros are defined unconditionally
// so they should be tested with #if not with #ifdef.
#if __cplusplus >= 201103L
# ifndef _GLIBCXX_USE_C99_MATH
#  define _GLIBCXX_USE_C99_MATH _GLIBCXX11_USE_C99_MATH
# endif
# ifndef _GLIBCXX_USE_C99_COMPLEX
# define _GLIBCXX_USE_C99_COMPLEX _GLIBCXX11_USE_C99_COMPLEX
# endif
# ifndef _GLIBCXX_USE_C99_STDIO
# define _GLIBCXX_USE_C99_STDIO _GLIBCXX11_USE_C99_STDIO
# endif
# ifndef _GLIBCXX_USE_C99_STDLIB
# define _GLIBCXX_USE_C99_STDLIB _GLIBCXX11_USE_C99_STDLIB
# endif
# ifndef _GLIBCXX_USE_C99_WCHAR
# define _GLIBCXX_USE_C99_WCHAR _GLIBCXX11_USE_C99_WCHAR
# endif
#else
# ifndef _GLIBCXX_USE_C99_MATH
#  define _GLIBCXX_USE_C99_MATH _GLIBCXX98_USE_C99_MATH
# endif
# ifndef _GLIBCXX_USE_C99_COMPLEX
# define _GLIBCXX_USE_C99_COMPLEX _GLIBCXX98_USE_C99_COMPLEX
# endif
# ifndef _GLIBCXX_USE_C99_STDIO
# define _GLIBCXX_USE_C99_STDIO _GLIBCXX98_USE_C99_STDIO
# endif
# ifndef _GLIBCXX_USE_C99_STDLIB
# define _GLIBCXX_USE_C99_STDLIB _GLIBCXX98_USE_C99_STDLIB
# endif
# ifndef _GLIBCXX_USE_C99_WCHAR
# define _GLIBCXX_USE_C99_WCHAR _GLIBCXX98_USE_C99_WCHAR
# endif
#endif

// Unless explicitly specified, enable char8_t extensions only if the core
// language char8_t feature macro is defined.
#ifndef _GLIBCXX_USE_CHAR8_T
# ifdef __cpp_char8_t
#  define _GLIBCXX_USE_CHAR8_T 1
# endif
#endif
#ifdef _GLIBCXX_USE_CHAR8_T
# define __cpp_lib_char8_t 201907L
#endif

/* Define if __float128 is supported on this host.  */
#if defined(__FLOAT128__) || defined(__SIZEOF_FLOAT128__)
/* For powerpc64 don't use __float128 when it's the same type as long double. */
# if !(defined(_GLIBCXX_LONG_DOUBLE_ALT128_COMPAT) && defined(__LONG_DOUBLE_IEEE128__))
#  define _GLIBCXX_USE_FLOAT128 1
# endif
#endif

// Define if float has the IEEE binary32 format.
#if __FLT_MANT_DIG__ == 24 \
  && __FLT_MIN_EXP__ == -125 \
  && __FLT_MAX_EXP__ == 128
# define _GLIBCXX_FLOAT_IS_IEEE_BINARY32 1
#endif

// Define if double has the IEEE binary64 format.
#if __DBL_MANT_DIG__ == 53 \
  && __DBL_MIN_EXP__ == -1021 \
  && __DBL_MAX_EXP__ == 1024
# define _GLIBCXX_DOUBLE_IS_IEEE_BINARY64 1
#endif

// Define if long double has the IEEE binary128 format.
#if __LDBL_MANT_DIG__ == 113 \
  && __LDBL_MIN_EXP__ == -16381 \
  && __LDBL_MAX_EXP__ == 16384
# define _GLIBCXX_LDOUBLE_IS_IEEE_BINARY128 1
#endif

#if defined __cplusplus && defined __BFLT16_DIG__
namespace __gnu_cxx
{
  typedef __decltype(0.0bf16) __bfloat16_t;
}
#endif

#ifdef __has_builtin
# ifdef __is_identifier
// Intel and older Clang require !__is_identifier for some built-ins:
#  define _GLIBCXX_HAS_BUILTIN(B) __has_builtin(B) || ! __is_identifier(B)
# else
#  define _GLIBCXX_HAS_BUILTIN(B) __has_builtin(B)
# endif
#endif

#if _GLIBCXX_HAS_BUILTIN(__has_unique_object_representations)
# define _GLIBCXX_HAVE_BUILTIN_HAS_UNIQ_OBJ_REP 1
#endif

#if _GLIBCXX_HAS_BUILTIN(__is_aggregate)
# define _GLIBCXX_HAVE_BUILTIN_IS_AGGREGATE 1
#endif

#if _GLIBCXX_HAS_BUILTIN(__builtin_launder)
# define _GLIBCXX_HAVE_BUILTIN_LAUNDER 1
#endif

// Returns 1 if _GLIBCXX_DO_NOT_USE_BUILTIN_TRAITS is not defined and the
// compiler has a corresponding built-in type trait, 0 otherwise.
// _GLIBCXX_DO_NOT_USE_BUILTIN_TRAITS can be defined to disable the use of
// built-in traits.
#ifndef _GLIBCXX_DO_NOT_USE_BUILTIN_TRAITS
# define _GLIBCXX_USE_BUILTIN_TRAIT(BT) _GLIBCXX_HAS_BUILTIN(BT)
#else
# define _GLIBCXX_USE_BUILTIN_TRAIT(BT) 0
#endif

// Mark code that should be ignored by the compiler, but seen by Doxygen.
#define _GLIBCXX_DOXYGEN_ONLY(X)

// PSTL configuration

#if __cplusplus >= 201703L
// This header is not installed for freestanding:
#if __has_include(<pstl/pstl_config.h>)
// Preserved here so we have some idea which version of upstream we've pulled in
// #define PSTL_VERSION 9000

// For now this defaults to being based on the presence of Thread Building Blocks
# ifndef _GLIBCXX_USE_TBB_PAR_BACKEND
#  define _GLIBCXX_USE_TBB_PAR_BACKEND __has_include(<tbb/tbb.h>)
# endif
// This section will need some rework when a new (default) backend type is added
# if _GLIBCXX_USE_TBB_PAR_BACKEND
#  define _PSTL_PAR_BACKEND_TBB
# else
#  define _PSTL_PAR_BACKEND_SERIAL
# endif

# define _PSTL_ASSERT(_Condition) __glibcxx_assert(_Condition)
# define _PSTL_ASSERT_MSG(_Condition, _Message) __glibcxx_assert(_Condition)

#include <pstl/pstl_config.h>
#endif // __has_include
#endif // C++17

// End of prewritten config; the settings discovered at configure time follow.
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the `acosf' function. */
#define _GLIBCXX_HAVE_ACOSF 1

/* Define to 1 if you have the `acosl' function. */
#define _GLIBCXX_HAVE_ACOSL 1

/* Define to 1 if you have the `aligned_alloc' function. */
#define _GLIBCXX_HAVE_ALIGNED_ALLOC 1

/* Define if arc4random is available in <stdlib.h>. */
#define _GLIBCXX_HAVE_ARC4RANDOM 1

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define _GLIBCXX_HAVE_ARPA_INET_H 1

/* Define to 1 if you have the `asinf' function. */
#define _GLIBCXX_HAVE_ASINF 1

/* Define to 1 if you have the `asinl' function. */
#define _GLIBCXX_HAVE_ASINL 1

/* Define to 1 if the target assembler supports .symver directive. */
#define _GLIBCXX_HAVE_AS_SYMVER_DIRECTIVE 1

/* Define to 1 if you have the `atan2f' function. */
#define _GLIBCXX_HAVE_ATAN2F 1

/* Define to 1 if you have the `atan2l' function. */
#define _GLIBCXX_HAVE_ATAN2L 1

/* Define to 1 if you have the `atanf' function. */
#define _GLIBCXX_HAVE_ATANF 1

/* Define to 1 if you have the `atanl' function. */
#define _GLIBCXX_HAVE_ATANL 1

/* Defined if shared_ptr reference counting should use atomic operations. */
#define _GLIBCXX_HAVE_ATOMIC_LOCK_POLICY 1

/* Define to 1 if you have the `at_quick_exit' function. */
#define _GLIBCXX_HAVE_AT_QUICK_EXIT 1

/* Define if C99 float_t and double_t in <math.h> should be imported in
   <cmath> in namespace std for C++11. */
#define _GLIBCXX_HAVE_C99_FLT_EVAL_TYPES 1

/* Define to 1 if the target assembler supports thread-local storage. */
/* #undef _GLIBCXX_HAVE_CC_TLS */

/* Define to 1 if you have the `ceilf' function. */
#define _GLIBCXX_HAVE_CEILF 1

/* Define to 1 if you have the `ceill' function. */
#define _GLIBCXX_HAVE_CEILL 1

/* Define to 1 if you have the <complex.h> header file. */
#define _GLIBCXX_HAVE_COMPLEX_H 1

/* Define to 1 if you have the `cosf' function. */
#define _GLIBCXX_HAVE_COSF 1

/* Define to 1 if you have the `coshf' function. */
#define _GLIBCXX_HAVE_COSHF 1

/* Define to 1 if you have the `coshl' function. */
#define _GLIBCXX_HAVE_COSHL 1

/* Define to 1 if you have the `cosl' function. */
#define _GLIBCXX_HAVE_COSL 1

/* Define to 1 if you have the declaration of `strnlen', and to 0 if you
   don't. */
#define _GLIBCXX_HAVE_DECL_STRNLEN 1

/* Define to 1 if you have the <dirent.h> header file. */
#define _GLIBCXX_HAVE_DIRENT_H 1

/* Define if dirfd is available in <dirent.h>. */
#define _GLIBCXX_HAVE_DIRFD 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define _GLIBCXX_HAVE_DLFCN_H 1

/* Define to 1 if you have the <endian.h> header file. */
#define _GLIBCXX_HAVE_ENDIAN_H 1

/* Define to 1 if GCC 4.6 supported std::exception_ptr for the target */
#define _GLIBCXX_HAVE_EXCEPTION_PTR_SINCE_GCC46 1

/* Define to 1 if you have the <execinfo.h> header file. */
#define _GLIBCXX_HAVE_EXECINFO_H 1

/* Define to 1 if you have the `expf' function. */
#define _GLIBCXX_HAVE_EXPF 1

/* Define to 1 if you have the `expl' function. */
#define _GLIBCXX_HAVE_EXPL 1

/* Define to 1 if you have the `fabsf' function. */
#define _GLIBCXX_HAVE_FABSF 1

/* Define to 1 if you have the `fabsl' function. */
#define _GLIBCXX_HAVE_FABSL 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define _GLIBCXX_HAVE_FCNTL_H 1

/* Define if fdopendir is available in <dirent.h>. */
#define _GLIBCXX_HAVE_FDOPENDIR 1

/* Define to 1 if you have the <fenv.h> header file. */
#define _GLIBCXX_HAVE_FENV_H 1

/* Define to 1 if you have the `finite' function. */
#define _GLIBCXX_HAVE_FINITE 1

/* Define to 1 if you have the `finitef' function. */
#define _GLIBCXX_HAVE_FINITEF 1

/* Define to 1 if you have the `finitel' function. */
#define _GLIBCXX_HAVE_FINITEL 1

/* Define to 1 if you have the <float.h> header file. */
#define _GLIBCXX_HAVE_FLOAT_H 1

/* Define to 1 if you have the `floorf' function. */
#define _GLIBCXX_HAVE_FLOORF 1

/* Define to 1 if you have the `floorl' function. */
#define _GLIBCXX_HAVE_FLOORL 1

/* Define to 1 if you have the `fmodf' function. */
#define _GLIBCXX_HAVE_FMODF 1

/* Define to 1 if you have the `fmodl' function. */
#define _GLIBCXX_HAVE_FMODL 1

/* Define to 1 if you have the `fpclass' function. */
/* #undef _GLIBCXX_HAVE_FPCLASS */

/* Define to 1 if you have the <fp.h> header file. */
/* #undef _GLIBCXX_HAVE_FP_H */

/* Define to 1 if you have the `frexpf' function. */
#define _GLIBCXX_HAVE_FREXPF 1

/* Define to 1 if you have the `frexpl' function. */
#define _GLIBCXX_HAVE_FREXPL 1

/* Define if getentropy is available in <unistd.h>. */
#define _GLIBCXX_HAVE_GETENTROPY 1

/* Define if _Unwind_GetIPInfo is available. */
#define _GLIBCXX_HAVE_GETIPINFO 1

/* Define if gets is available in <stdio.h> before C++14. */
#define _GLIBCXX_HAVE_GETS 1

/* Define to 1 if you have the `hypot' function. */
#define _GLIBCXX_HAVE_HYPOT 1

/* Define to 1 if you have the `hypotf' function. */
#define _GLIBCXX_HAVE_HYPOTF 1

/* Define to 1 if you have the `hypotl' function. */
#define _GLIBCXX_HAVE_HYPOTL 1

/* Define if you have the iconv() function and it works. */
#define _GLIBCXX_HAVE_ICONV 1

/* Define to 1 if you have the <ieeefp.h> header file. */
/* #undef _GLIBCXX_HAVE_IEEEFP_H */

/* Define to 1 if you have the <inttypes.h> header file. */
#define _GLIBCXX_HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isinf' function. */
/* #undef _GLIBCXX_HAVE_ISINF */

/* Define to 1 if you have the `isinff' function. */
#define _GLIBCXX_HAVE_ISINFF 1

/* Define to 1 if you have the `isinfl' function. */
#define _GLIBCXX_HAVE_ISINFL 1

/* Define to 1 if you have the `isnan' function. */
/* #undef _GLIBCXX_HAVE_ISNAN */

/* Define to 1 if you have the `isnanf' function. */
#define _GLIBCXX_HAVE_ISNANF 1

/* Define to 1 if you have the `isnanl' function. */
#define _GLIBCXX_HAVE_ISNANL 1

/* Defined if iswblank exists. */
#define _GLIBCXX_HAVE_ISWBLANK 1

/* Define if LC_MESSAGES is available in <locale.h>. */
#define _GLIBCXX_HAVE_LC_MESSAGES 1

/* Define to 1 if you have the `ldexpf' function. */
#define _GLIBCXX_HAVE_LDEXPF 1

/* Define to 1 if you have the `ldexpl' function. */
#define _GLIBCXX_HAVE_LDEXPL 1

/* Define to 1 if you have the <libintl.h> header file. */
#define _GLIBCXX_HAVE_LIBINTL_H 1

/* Only used in build directory testsuite_hooks.h. */
#define _GLIBCXX_HAVE_LIMIT_AS 1

/* Only used in build directory testsuite_hooks.h. */
#define _GLIBCXX_HAVE_LIMIT_DATA 1

/* Only used in build directory testsuite_hooks.h. */
#define _GLIBCXX_HAVE_LIMIT_FSIZE 1

/* Only used in build directory testsuite_hooks.h. */
#define _GLIBCXX_HAVE_LIMIT_RSS 1

/* Only used in build directory testsuite_hooks.h. */
#define _GLIBCXX_HAVE_LIMIT_VMEM 0

/* Define if link is available in <unistd.h>. */
#define _GLIBCXX_HAVE_LINK 1

/* Define to 1 if you have the <link.h> header file. */
#define _GLIBCXX_HAVE_LINK_H 1

/* Define if futex syscall is available. */
#define _GLIBCXX_HAVE_LINUX_FUTEX 1

/* Define to 1 if you have the <linux/random.h> header file. */
#define _GLIBCXX_HAVE_LINUX_RANDOM_H 1

/* Define to 1 if you have the <linux/types.h> header file. */
#define _GLIBCXX_HAVE_LINUX_TYPES_H 1

/* Define to 1 if you have the <locale.h> header file. */
#define _GLIBCXX_HAVE_LOCALE_H 1

/* Define to 1 if you have the `log10f' function. */
#define _GLIBCXX_HAVE_LOG10F 1

/* Define to 1 if you have the `log10l' function. */
#define _GLIBCXX_HAVE_LOG10L 1

/* Define to 1 if you have the `logf' function. */
#define _GLIBCXX_HAVE_LOGF 1

/* Define to 1 if you have the `logl' function. */
#define _GLIBCXX_HAVE_LOGL 1

/* Define if lseek is available in <unistd.h>. */
#define _GLIBCXX_HAVE_LSEEK 1

/* Define to 1 if you have the <machine/endian.h> header file. */
/* #undef _GLIBCXX_HAVE_MACHINE_ENDIAN_H */

/* Define to 1 if you have the <machine/param.h> header file. */
/* #undef _GLIBCXX_HAVE_MACHINE_PARAM_H */

/* Define if mbstate_t exists in wchar.h. */
#define _GLIBCXX_HAVE_MBSTATE_T 1

/* Define to 1 if you have the `memalign' function. */
#define _GLIBCXX_HAVE_MEMALIGN 1

/* Define to 1 if you have the <memory.h> header file. */
#define _GLIBCXX_HAVE_MEMORY_H 1

/* Define to 1 if you have the `modf' function. */
#define _GLIBCXX_HAVE_MODF 1

/* Define to 1 if you have the `modff' function. */
#define _GLIBCXX_HAVE_MODFF 1

/* Define to 1 if you have the `modfl' function. */
#define _GLIBCXX_HAVE_MODFL 1

/* Define to 1 if you have the <nan.h> header file. */
/* #undef _GLIBCXX_HAVE_NAN_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define _GLIBCXX_HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define _GLIBCXX_HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define _GLIBCXX_HAVE_NETINET_TCP_H 1

/* Define if <math.h> defines obsolete isinf function. */
/* #undef _GLIBCXX_HAVE_OBSOLETE_ISINF */

/* Define if <math.h> defines obsolete isnan function. */
/* #undef _GLIBCXX_HAVE_OBSOLETE_ISNAN */

/* Define if openat is available in <fcntl.h>. */
#define _GLIBCXX_HAVE_OPENAT 1

/* Define if O_NONBLOCK is defined in <fcntl.h> */
#define _GLIBCXX_HAVE_O_NONBLOCK 1

/* Define if poll is available in <poll.h>. */
#define _GLIBCXX_HAVE_POLL 1

/* Define to 1 if you have the <poll.h> header file. */
#define _GLIBCXX_HAVE_POLL_H 1

/* Define to 1 if you have the `posix_memalign' function. */
#define _GLIBCXX_HAVE_POSIX_MEMALIGN 1

/* Define to 1 if POSIX Semaphores with sem_timedwait are available in
   <semaphore.h>. */
#define _GLIBCXX_HAVE_POSIX_SEMAPHORE 1

/* Define to 1 if you have the `powf' function. */
#define _GLIBCXX_HAVE_POWF 1

/* Define to 1 if you have the `powl' function. */
#define _GLIBCXX_HAVE_POWL 1

/* Define to 1 if you have the `qfpclass' function. */
/* #undef _GLIBCXX_HAVE_QFPCLASS */

/* Define to 1 if you have the `quick_exit' function. */
#define _GLIBCXX_HAVE_QUICK_EXIT 1

/* Define if readlink is available in <unistd.h>. */
#define _GLIBCXX_HAVE_READLINK 1

/* Define to 1 if you have the `secure_getenv' function. */
#define _GLIBCXX_HAVE_SECURE_GETENV 1

/* Define to 1 if you have the `setenv' function. */
#define _GLIBCXX_HAVE_SETENV 1

/* Define to 1 if you have the `sincos' function. */
#define _GLIBCXX_HAVE_SINCOS 1

/* Define to 1 if you have the `sincosf' function. */
#define _GLIBCXX_HAVE_SINCOSF 1

/* Define to 1 if you have the `sincosl' function. */
#define _GLIBCXX_HAVE_SINCOSL 1

/* Define to 1 if you have the `sinf' function. */
#define _GLIBCXX_HAVE_SINF 1

/* Define to 1 if you have the `sinhf' function. */
#define _GLIBCXX_HAVE_SINHF 1

/* Define to 1 if you have the `sinhl' function. */
#define _GLIBCXX_HAVE_SINHL 1

/* Define to 1 if you have the `sinl' function. */
#define _GLIBCXX_HAVE_SINL 1

/* Defined if sleep exists. */
/* #undef _GLIBCXX_HAVE_SLEEP */

/* Define to 1 if you have the `sockatmark' function. */
#define _GLIBCXX_HAVE_SOCKATMARK 1

/* Define to 1 if you have the `sqrtf' function. */
#define _GLIBCXX_HAVE_SQRTF 1

/* Define to 1 if you have the `sqrtl' function. */
#define _GLIBCXX_HAVE_SQRTL 1

/* Define if the <stacktrace> header is supported. */
#define _GLIBCXX_HAVE_STACKTRACE 1

/* Define to 1 if you have the <stdalign.h> header file. */
#define _GLIBCXX_HAVE_STDALIGN_H 1

/* Define to 1 if you have the <stdbool.h> header file. */
#define _GLIBCXX_HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define _GLIBCXX_HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define _GLIBCXX_HAVE_STDLIB_H 1

/* Define if strerror_l is available in <string.h>. */
#define _GLIBCXX_HAVE_STRERROR_L 1

/* Define if strerror_r is available in <string.h>. */
#define _GLIBCXX_HAVE_STRERROR_R 1

/* Define to 1 if you have the <strings.h> header file. */
#define _GLIBCXX_HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define _GLIBCXX_HAVE_STRING_H 1

/* Define to 1 if you have the `strtof' function. */
#define _GLIBCXX_HAVE_STRTOF 1

/* Define to 1 if you have the `strtold' function. */
#define _GLIBCXX_HAVE_STRTOLD 1

/* Define to 1 if `d_type' is a member of `struct dirent'. */
#define _GLIBCXX_HAVE_STRUCT_DIRENT_D_TYPE 1

/* Define if strxfrm_l is available in <string.h>. */
#define _GLIBCXX_HAVE_STRXFRM_L 1

/* Define if symlink is available in <unistd.h>. */
#define _GLIBCXX_HAVE_SYMLINK 1

/* Define to 1 if the target runtime linker supports binding the same symbol
   to different versions. */
#define _GLIBCXX_HAVE_SYMVER_SYMBOL_RENAMING_RUNTIME_SUPPORT 1

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef _GLIBCXX_HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define _GLIBCXX_HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ipc.h> header file. */
#define _GLIBCXX_HAVE_SYS_IPC_H 1

/* Define to 1 if you have the <sys/isa_defs.h> header file. */
/* #undef _GLIBCXX_HAVE_SYS_ISA_DEFS_H */

/* Define to 1 if you have the <sys/machine.h> header file. */
/* #undef _GLIBCXX_HAVE_SYS_MACHINE_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define _GLIBCXX_HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define _GLIBCXX_HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define _GLIBCXX_HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have a suitable <sys/sdt.h> header file */
/* #undef _GLIBCXX_HAVE_SYS_SDT_H */

/* Define to 1 if you have the <sys/sem.h> header file. */
#define _GLIBCXX_HAVE_SYS_SEM_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define _GLIBCXX_HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define _GLIBCXX_HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define _GLIBCXX_HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
#define _GLIBCXX_HAVE_SYS_SYSINFO_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define _GLIBCXX_HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define _GLIBCXX_HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
#define _GLIBCXX_HAVE_SYS_UIO_H 1

/* Define if S_IFREG is available in <sys/stat.h>. */
/* #undef _GLIBCXX_HAVE_S_IFREG */

/* Define if S_ISREG is available in <sys/stat.h>. */
#define _GLIBCXX_HAVE_S_ISREG 1

/* Define to 1 if you have the `tanf' function. */
#define _GLIBCXX_HAVE_TANF 1

/* Define to 1 if you have the `tanhf' function. */
#define _GLIBCXX_HAVE_TANHF 1

/* Define to 1 if you have the `tanhl' function. */
#define _GLIBCXX_HAVE_TANHL 1

/* Define to 1 if you have the `tanl' function. */
#define _GLIBCXX_HAVE_TANL 1

/* Define to 1 if you have the <tgmath.h> header file. */
#define _GLIBCXX_HAVE_TGMATH_H 1

/* Define to 1 if you have the `timespec_get' function. */
#define _GLIBCXX_HAVE_TIMESPEC_GET 1

/* Define to 1 if the target supports thread-local storage. */
#define _GLIBCXX_HAVE_TLS 1

/* Define if truncate is available in <unistd.h>. */
#define _GLIBCXX_HAVE_TRUNCATE 1

/* Define to 1 if you have the <uchar.h> header file. */
#define _GLIBCXX_HAVE_UCHAR_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define _GLIBCXX_HAVE_UNISTD_H 1

/* Define if unlinkat is available in <fcntl.h>. */
#define _GLIBCXX_HAVE_UNLINKAT 1

/* Define to 1 if you have the `uselocale' function. */
#define _GLIBCXX_HAVE_USELOCALE 1

/* Defined if usleep exists. */
/* #undef _GLIBCXX_HAVE_USLEEP */

/* Define to 1 if you have the <utime.h> header file. */
#define _GLIBCXX_HAVE_UTIME_H 1

/* Defined if vfwscanf exists. */
#define _GLIBCXX_HAVE_VFWSCANF 1

/* Defined if vswscanf exists. */
#define _GLIBCXX_HAVE_VSWSCANF 1

/* Defined if vwscanf exists. */
#define _GLIBCXX_HAVE_VWSCANF 1

/* Define to 1 if you have the <wchar.h> header file. */
#define _GLIBCXX_HAVE_WCHAR_H 1

/* Defined if wcstof exists. */
#define _GLIBCXX_HAVE_WCSTOF 1

/* Define to 1 if you have the <wctype.h> header file. */
#define _GLIBCXX_HAVE_WCTYPE_H 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef _GLIBCXX_HAVE_WINDOWS_H */

/* Define if writev is available in <sys/uio.h>. */
#define _GLIBCXX_HAVE_WRITEV 1

/* Define to 1 if you have the <xlocale.h> header file. */
/* #undef _GLIBCXX_HAVE_XLOCALE_H */

/* Define to 1 if you have the `_aligned_malloc' function. */
/* #undef _GLIBCXX_HAVE__ALIGNED_MALLOC */

/* Define to 1 if you have the `_wfopen' function. */
/* #undef _GLIBCXX_HAVE__WFOPEN */

/* Define to 1 if you have the `__cxa_thread_atexit' function. */
/* #undef _GLIBCXX_HAVE___CXA_THREAD_ATEXIT */

/* Define to 1 if you have the `__cxa_thread_atexit_impl' function. */
#define _GLIBCXX_HAVE___CXA_THREAD_ATEXIT_IMPL 1

/* Define as const if the declaration of iconv() needs const. */
#define _GLIBCXX_ICONV_CONST 

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define _GLIBCXX_LT_OBJDIR ".libs/"

/* Name of package */
/* #undef _GLIBCXX_PACKAGE */

/* Define to the address where bug reports for this package should be sent. */
#define _GLIBCXX_PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define _GLIBCXX_PACKAGE_NAME "package-unused"

/* Define to the full name and version of this package. */
#define _GLIBCXX_PACKAGE_STRING "package-unused version-unused"

/* Define to the one symbol short name of this package. */
#define _GLIBCXX_PACKAGE_TARNAME "libstdc++"

/* Define to the home page for this package. */
#define _GLIBCXX_PACKAGE_URL ""

/* Define to the version of this package. */
#define _GLIBCXX_PACKAGE__GLIBCXX_VERSION "version-unused"

/* Define to 1 if you have the ANSI C header files. */
#define _GLIBCXX_STDC_HEADERS 1

/* Version number of package */
/* #undef _GLIBCXX_VERSION */

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _GLIBCXX_DARWIN_USE_64_BIT_INODE
# define _GLIBCXX_DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _GLIBCXX_FILE_OFFSET_BITS */

/* Define if C99 functions in <complex.h> should be used in <complex> for
   C++11. Using compiler builtins for these functions requires corresponding
   C99 library functions to be present. */
#define _GLIBCXX11_USE_C99_COMPLEX 1

/* Define if C99 generic macros in <math.h> should be imported in <cmath> in
   namespace std for C++11. */
#define _GLIBCXX11_USE_C99_MATH 1

/* Define if C99 functions or macros in <stdio.h> should be imported in
   <cstdio> in namespace std for C++11. */
#define _GLIBCXX11_USE_C99_STDIO 1

/* Define if C99 functions or macros in <stdlib.h> should be imported in
   <cstdlib> in namespace std for C++11. */
#define _GLIBCXX11_USE_C99_STDLIB 1

/* Define if C99 functions or macros in <wchar.h> should be imported in
   <cwchar> in namespace std for C++11. */
#define _GLIBCXX11_USE_C99_WCHAR 1

/* Define if C99 functions in <complex.h> should be used in <complex> for
   C++98. Using compiler builtins for these functions requires corresponding
   C99 library functions to be present. */
#define _GLIBCXX98_USE_C99_COMPLEX 1

/* Define if C99 functions or macros in <math.h> should be imported in <cmath>
   in namespace std for C++98. */
#define _GLIBCXX98_USE_C99_MATH 1

/* Define if C99 functions or macros in <stdio.h> should be imported in
   <cstdio> in namespace std for C++98. */
#define _GLIBCXX98_USE_C99_STDIO 1

/* Define if C99 functions or macros in <stdlib.h> should be imported in
   <cstdlib> in namespace std for C++98. */
#define _GLIBCXX98_USE_C99_STDLIB 1

/* Define if C99 functions or macros in <wchar.h> should be imported in
   <cwchar> in namespace std for C++98. */
#define _GLIBCXX98_USE_C99_WCHAR 1

/* Define if the compiler supports C++11 atomics. */
#define _GLIBCXX_ATOMIC_BUILTINS 1

/* Define if global objects can be aligned to
   std::hardware_destructive_interference_size. */
#define _GLIBCXX_CAN_ALIGNAS_DESTRUCTIVE_SIZE 1

/* Define to use concept checking code from the boost libraries. */
/* #undef _GLIBCXX_CONCEPT_CHECKS */

/* Define to 1 if a fully dynamic basic_string is wanted, 0 to disable,
   undefined for platform defaults */
#define _GLIBCXX_FULLY_DYNAMIC_STRING 0

/* Define if gthreads library is available. */
#define _GLIBCXX_HAS_GTHREADS 1

/* Define to 1 if a full hosted library is built, or 0 if freestanding. */
#define _GLIBCXX_HOSTED __STDC_HOSTED__

/* Define if compatibility should be provided for alternative 128-bit long
   double formats. */

/* Define if compatibility should be provided for -mlong-double-64. */

/* Define to the letter to which size_t is mangled. */
#define _GLIBCXX_MANGLE_SIZE_T m

/* Define if C99 llrint and llround functions are missing from <math.h>. */
/* #undef _GLIBCXX_NO_C99_ROUNDING_FUNCS */

/* Defined if no way to sleep is available. */
/* #undef _GLIBCXX_NO_SLEEP */

/* Define if ptrdiff_t is int. */
/* #undef _GLIBCXX_PTRDIFF_T_IS_INT */

/* Define if using setrlimit to set resource limits during "make check" */
#define _GLIBCXX_RES_LIMITS 1

/* Define if size_t is unsigned int. */
/* #undef _GLIBCXX_SIZE_T_IS_UINT */

/* Define if static tzdata should be compiled into the library. */
#define _GLIBCXX_STATIC_TZDATA 1

/* Define to the value of the EOF integer constant. */
#define _GLIBCXX_STDIO_EOF -1

/* Define to the value of the SEEK_CUR integer constant. */
#define _GLIBCXX_STDIO_SEEK_CUR 1

/* Define to the value of the SEEK_END integer constant. */
#define _GLIBCXX_STDIO_SEEK_END 2

/* Define to use symbol versioning in the shared library. */
#define _GLIBCXX_SYMVER 1

/* Define to use darwin versioning in the shared library. */
/* #undef _GLIBCXX_SYMVER_DARWIN */

/* Define to use GNU versioning in the shared library. */
#define _GLIBCXX_SYMVER_GNU 1

/* Define to use GNU namespace versioning in the shared library. */
/* #undef _GLIBCXX_SYMVER_GNU_NAMESPACE */

/* Define to use Sun versioning in the shared library. */
/* #undef _GLIBCXX_SYMVER_SUN */

/* Define if C11 functions in <uchar.h> should be imported into namespace std
   in <cuchar>. */
#define _GLIBCXX_USE_C11_UCHAR_CXX11 1

/* Define if C99 functions or macros from <wchar.h>, <math.h>, <complex.h>,
   <stdio.h>, and <stdlib.h> can be used or exposed. */
#define _GLIBCXX_USE_C99 1

/* Define if C99 inverse trig functions in <complex.h> should be used in
   <complex>. Using compiler builtins for these functions requires
   corresponding C99 library functions to be present. */
#define _GLIBCXX_USE_C99_COMPLEX_ARC 1

/* Define if C99 functions in <complex.h> should be used in <tr1/complex>.
   Using compiler builtins for these functions requires corresponding C99
   library functions to be present. */
#define _GLIBCXX_USE_C99_COMPLEX_TR1 1

/* Define if C99 functions in <ctype.h> should be imported in <cctype> in
   namespace std for C++11. */
#define _GLIBCXX_USE_C99_CTYPE 1

/* Define if C99 functions in <ctype.h> should be imported in <tr1/cctype> in
   namespace std::tr1. */
#define _GLIBCXX_USE_C99_CTYPE_TR1 1

/* Define if C99 functions in <fenv.h> should be imported in <cfenv> in
   namespace std for C++11. */
#define _GLIBCXX_USE_C99_FENV 1

/* Define if C99 functions in <fenv.h> should be imported in <tr1/cfenv> in
   namespace std::tr1. */
#define _GLIBCXX_USE_C99_FENV_TR1 1

/* Define if C99 functions in <inttypes.h> should be imported in <cinttypes>
   in namespace std in C++11. */
#define _GLIBCXX_USE_C99_INTTYPES 1

/* Define if C99 functions in <inttypes.h> should be imported in
   <tr1/cinttypes> in namespace std::tr1. */
#define _GLIBCXX_USE_C99_INTTYPES_TR1 1

/* Define if wchar_t C99 functions in <inttypes.h> should be imported in
   <cinttypes> in namespace std in C++11. */
#define _GLIBCXX_USE_C99_INTTYPES_WCHAR_T 1

/* Define if wchar_t C99 functions in <inttypes.h> should be imported in
   <tr1/cinttypes> in namespace std::tr1. */
#define _GLIBCXX_USE_C99_INTTYPES_WCHAR_T_TR1 1

/* Define if C99 functions in <math.h> should be imported in <cmath> in
   namespace std for C++11. */
#define _GLIBCXX_USE_C99_MATH_FUNCS 1

/* Define if C99 functions or macros in <math.h> should be imported in
   <tr1/cmath> in namespace std::tr1. */
#define _GLIBCXX_USE_C99_MATH_TR1 1

/* Define if C99 types in <stdint.h> should be imported in <cstdint> in
   namespace std for C++11. */
#define _GLIBCXX_USE_C99_STDINT 1

/* Define if C99 types in <stdint.h> should be imported in <tr1/cstdint> in
   namespace std::tr1. */
#define _GLIBCXX_USE_C99_STDINT_TR1 1

/* Define if usable chdir is available in <unistd.h>. */
#define _GLIBCXX_USE_CHDIR 1

/* Define if usable chmod is available in <sys/stat.h>. */
#define _GLIBCXX_USE_CHMOD 1

/* Defined if clock_gettime syscall has monotonic and realtime clock support.
   */
/* #undef _GLIBCXX_USE_CLOCK_GETTIME_SYSCALL */

/* Defined if clock_gettime has monotonic clock support. */
#define _GLIBCXX_USE_CLOCK_MONOTONIC 1

/* Defined if clock_gettime has realtime clock support. */
#define _GLIBCXX_USE_CLOCK_REALTIME 1

/* Define if copy_file_range is available in <unistd.h>. */
/* #undef _GLIBCXX_USE_COPY_FILE_RANGE */

/* Define if ISO/IEC TR 24733 decimal floating point types are supported on
   this host. */
#define _GLIBCXX_USE_DECIMAL_FLOAT 1

/* Define if /dev/random and /dev/urandom are available for
   std::random_device. */
#define _GLIBCXX_USE_DEV_RANDOM 1

/* Define if fchmod is available in <sys/stat.h>. */
#define _GLIBCXX_USE_FCHMOD 1

/* Define if fchmodat is available in <sys/stat.h>. */
#define _GLIBCXX_USE_FCHMODAT 1

/* Define if fseeko and ftello are available. */
#define _GLIBCXX_USE_FSEEKO_FTELLO 1

/* Define if usable getcwd is available in <unistd.h>. */
#define _GLIBCXX_USE_GETCWD 1

/* Defined if gettimeofday is available. */
#define _GLIBCXX_USE_GETTIMEOFDAY 1

/* Define if get_nprocs is available in <sys/sysinfo.h>. */
#define _GLIBCXX_USE_GET_NPROCS 1

/* Define if init_priority should be used for iostream initialization. */
#define _GLIBCXX_USE_INIT_PRIORITY_ATTRIBUTE 1

/* Define if LFS support is available. */
#define _GLIBCXX_USE_LFS 1

/* Define if code specialized for long long should be used. */
#define _GLIBCXX_USE_LONG_LONG 1

/* Define if lstat is available in <sys/stat.h>. */
#define _GLIBCXX_USE_LSTAT 1

/* Define if usable mkdir is available in <sys/stat.h>. */
#define _GLIBCXX_USE_MKDIR 1

/* Defined if nanosleep is available. */
#define _GLIBCXX_USE_NANOSLEEP 1

/* Define if NLS translations are to be used. */
#define _GLIBCXX_USE_NLS 1

/* Define if nl_langinfo_l should be used for std::text_encoding. */
#define _GLIBCXX_USE_NL_LANGINFO_L 1

/* Define if pthreads_num_processors_np is available in <pthread.h>. */
/* #undef _GLIBCXX_USE_PTHREADS_NUM_PROCESSORS_NP */

/* Define if pthread_cond_clockwait is available in <pthread.h>. */
#define _GLIBCXX_USE_PTHREAD_COND_CLOCKWAIT 1

/* Define if pthread_mutex_clocklock is available in <pthread.h>. */
#define _GLIBCXX_USE_PTHREAD_MUTEX_CLOCKLOCK (_GLIBCXX_TSAN==0)

/* Define if pthread_rwlock_clockrdlock and pthread_rwlock_clockwrlock are
   available in <pthread.h>. */
#define _GLIBCXX_USE_PTHREAD_RWLOCK_CLOCKLOCK 1

/* Define if POSIX read/write locks are available in <gthr.h>. */
#define _GLIBCXX_USE_PTHREAD_RWLOCK_T 1

/* Define if /dev/random and /dev/urandom are available for the random_device
   of TR1 (Chapter 5.1). */
#define _GLIBCXX_USE_RANDOM_TR1 1

/* Define if usable realpath is available in <stdlib.h>. */
#define _GLIBCXX_USE_REALPATH 1

/* Defined if sched_yield is available. */
#define _GLIBCXX_USE_SCHED_YIELD 1

/* Define if _SC_NPROCESSORS_ONLN is available in <unistd.h>. */
#define _GLIBCXX_USE_SC_NPROCESSORS_ONLN 1

/* Define if _SC_NPROC_ONLN is available in <unistd.h>. */
/* #undef _GLIBCXX_USE_SC_NPROC_ONLN */

/* Define if sendfile is available in <sys/sendfile.h>. */
#define _GLIBCXX_USE_SENDFILE 1

/* Define to restrict std::__basic_file<> to stdio APIs. */
/* #undef _GLIBCXX_USE_STDIO_PURE */

/* Define if struct stat has timespec members. */
#define _GLIBCXX_USE_ST_MTIM 1

/* Define if sysctl(), CTL_HW and HW_NCPU are available in <sys/sysctl.h>. */
/* #undef _GLIBCXX_USE_SYSCTL_HW_NCPU */

/* Define if obsolescent tmpnam is available in <stdio.h>. */
#define _GLIBCXX_USE_TMPNAM 1

/* Define if c8rtomb and mbrtoc8 functions in <uchar.h> should be imported
   into namespace std in <cuchar> for C++20. */
#define _GLIBCXX_USE_UCHAR_C8RTOMB_MBRTOC8_CXX20 1

/* Define if c8rtomb and mbrtoc8 functions in <uchar.h> should be imported
   into namespace std in <cuchar> for -fchar8_t. */
#define _GLIBCXX_USE_UCHAR_C8RTOMB_MBRTOC8_FCHAR8_T 1

/* Define if utime is available in <utime.h>. */
#define _GLIBCXX_USE_UTIME 1

/* Define if utimensat and UTIME_OMIT are available in <sys/stat.h> and
   AT_FDCWD in <fcntl.h>. */
#define _GLIBCXX_USE_UTIMENSAT 1

/* Define if code specialized for wchar_t should be used. */
#define _GLIBCXX_USE_WCHAR_T 1

/* Defined if Sleep exists. */
/* #undef _GLIBCXX_USE_WIN32_SLEEP */

/* Define if _get_osfhandle should be used for filebuf::native_handle(). */
/* #undef _GLIBCXX_USE__GET_OSFHANDLE */

/* Define to 1 if a verbose library is built, or 0 otherwise. */
#define _GLIBCXX_VERBOSE 1

/* Defined if as can handle rdrand. */
#define _GLIBCXX_X86_RDRAND 1

/* Defined if as can handle rdseed. */
#define _GLIBCXX_X86_RDSEED 1

/* Define if a directory should be searched for tzdata files. */
#define _GLIBCXX_ZONEINFO_DIR "/usr/share/zoneinfo"

/* Define to 1 if mutex_timedlock is available. */
#define _GTHREAD_USE_MUTEX_TIMEDLOCK 1

/* Define for large files, on AIX-style hosts. */
/* #undef _GLIBCXX_LARGE_FILES */

/* Define if all C++11 floating point overloads are available in <math.h>.  */
#if __cplusplus >= 201103L
/* #undef __CORRECT_ISO_CPP11_MATH_H_PROTO_FP */
#endif

/* Define if all C++11 integral type overloads are available in <math.h>.  */
#if __cplusplus >= 201103L
/* #undef __CORRECT_ISO_CPP11_MATH_H_PROTO_INT */
#endif

#endif // _GLIBCXX_CXX_CONFIG_H
