/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_COMPILER_TYPES_H
#define __LINUX_COMPILER_TYPES_H

#ifndef __ASSEMBLY__

#if defined(CONFIG_DEBUG_INFO_BTF) && defined(CONFIG_PAHOLE_HAS_BTF_TAG) && \
	__has_attribute(btf_type_tag) && !defined(__BINDGEN__)
# define BTF_TYPE_TAG(value) __attribute__((btf_type_tag(#value)))
#else
# define BTF_TYPE_TAG(value)
#endif

#ifdef __CHECKER__

# define __rcu		__attribute__((noderef, address_space(__rcu)))
# define __percpu	__attribute__((noderef, address_space(__percpu)))
# define __iomem	__attribute__((noderef, address_space(__iomem)))
# define __user		__attribute__((noderef, address_space(__user)))
# define __kernel	__attribute__((address_space(0)))
static inline void __chk_io_ptr(const volatile void __iomem *ptr) { }
static inline void __chk_user_ptr(const volatile void __user *ptr) { }

# define __cond_lock(x, c)	((c) ? ({ __acquire(x); 1; }) : 0)
# define __release(x)	__context__(x, -1)
# define __acquire(x)	__context__(x, 1)
# define __releases(x)	__attribute__((context(x, 1, 0)))
# define __cond_acquires(x) __attribute__((context(x, 0, -1)))
# define __acquires(x)	__attribute__((context(x, 0, 1)))
# define __must_hold(x)	__attribute__((context(x, 1, 1)))
# define __private	__attribute__((noderef))
# define __safe		__attribute__((safe))
# define __nocast	__attribute__((nocast))
# define __force	__attribute__((force))
# define ACCESS_PRIVATE(ptr, member) (*((typeof((ptr)->member) __force *) &(ptr)->member))
#else
# define __kernel
# define __chk_io_ptr(x)	(void)0
# define __chk_user_ptr(x)	(void)0
# define __rcu
# define __percpu	BTF_TYPE_TAG(percpu)
# ifdef STRUCTLEAK_PLUGIN
#  define __user	__attribute__((user)) // __attribute__((user))
# else
#  define __user	BTF_TYPE_TAG(user) // BTF_TYPE_TAG(user)
# endif
# define __iomem
# define __cond_lock(x, c) (c)
# define __release(x)	(void)0
# define __acquire(x)	(void)0
# define __releases(x)
# define __cond_acquires(x)
# define __acquires(x)
# define __must_hold(x)
# define __private
# define __safe
# define __nocast
# define __force
# define ACCESS_PRIVATE(ptr, member) ((ptr)->member)
# define __builtin_warning(x, y...) (1)
#endif

#define ___PASTE(x, y) x##y
#define __PASTE(x, y) ___PASTE(x, y)

#ifdef __KERNEL__

#include <linux/compiler_attributes.h>

#ifndef __has_builtin
#define __has_builtin(x) (0)
#endif

#ifdef __clang__
#include <linux/compiler-clang.h>
#elif defined(__INTEL_COMPILER)
#include <linux/compiler-intel.h>
#elif defined(__GNUC__)
#include <linux/compiler-gcc.h>
#else
#error "Unknown compiler"
#endif

#ifdef CONFIG_HAVE_ARCH_COMPILER_H
#include <asm/compiler.h>
#endif

#define __naked			__attribute__((__naked__)) notrace

#define inline inline __gnu_inline __inline_maybe_unused notrace

#define __inline__ inline

#ifdef KBUILD_EXTRA_WARN1
#define __inline_maybe_unused
#else
#define __inline_maybe_unused __maybe_unused
#endif


#define noinline_for_stack noinline


#ifdef __SANITIZE_ADDRESS__

# define __no_kasan_or_inline __no_sanitize_address notrace __maybe_unused
# define __no_sanitize_or_inline __no_kasan_or_inline
#else
# define __no_kasan_or_inline __always_inline
#endif

#ifdef __SANITIZE_THREAD__

# define __no_kcsan __no_sanitize_thread __disable_sanitizer_instrumentation
# define __no_sanitize_or_inline __no_kcsan notrace __maybe_unused
#else
# define __no_kcsan
#endif

#ifndef __no_sanitize_or_inline
#define __no_sanitize_or_inline __always_inline
#endif

#define noinstr								\
	noinline notrace __attribute((__section__(".noinstr.text")))	\
	__no_kcsan __no_sanitize_address __no_profile __no_sanitize_coverage \
	__no_sanitize_memory

#endif

#endif

#ifndef __latent_entropy
# define __latent_entropy
#endif

#ifdef __alloc_size__
# define __alloc_size(x, ...)	__alloc_size__(x, ## __VA_ARGS__) __malloc
# define __realloc_size(x, ...)	__alloc_size__(x, ## __VA_ARGS__)
#else
# define __alloc_size(x, ...)	__malloc
# define __realloc_size(x, ...)
#endif

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#define __scalar_type_to_expr_cases(type)				\
		unsigned type:	(unsigned type)0,			\
		signed type:	(signed type)0

#define __native_word(type) \
	(sizeof(type) == sizeof(char) || sizeof(type) == sizeof(short) || \
	 sizeof(type) == sizeof(int) || sizeof(type) == sizeof(long))

#ifdef __OPTIMIZE__
# define __compiletime_assert(cond, msg, prefix, suffix)		\
	do {
		__noreturn extern void prefix ## suffix(void)		\
			__compiletime_error(msg);			\
		if (!(cond))					\
			prefix ## suffix();				\
	} while (0)
#else
# define __compiletime_assert(cond, msg, prefix, suffix) do { } while (0)
#endif

#define _compiletime_assert(cond, msg, prefix, suffix) \
	__compiletime_assert(cond, msg, prefix, suffix)

#define compiletime_assert(cond, msg) \
	_compiletime_assert(cond, msg, __compiletime_assert_, __COUNTER__)

#define compiletime_assert_atomic_type(t)				\
	compiletime_assert(__native_word(t), "Need native word sized stores/loads for atomicity.")

#ifndef __diag_ignore_all
#define __diag_ignore_all(option, comment)
#endif

#ifndef __diag_GCC
#define __diag_GCC(version, severity, string)
#endif

#ifndef __diag
#define __diag(string)
#endif

#define __diag_pop()	__diag(pop)
#define __diag_push()	__diag(push)

#define __diag_ignore(compiler, version, opt, comment)  __diag_ ## compiler(version, ignore, opt)
#define __diag_warn(compiler, version, opt, comment)  __diag_ ## compiler(version, warn, opt)
#define __diag_error(compiler, version, opt, comment) __diag_ ## compiler(version, error, opt)

#endif
