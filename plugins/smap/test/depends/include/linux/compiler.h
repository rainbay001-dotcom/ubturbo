/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_DEPENDS_COMPILER_H
#define __LINUX_DEPENDS_COMPILER_H

#include <linux/compiler_types.h>

#ifndef __ASSEMBLY__

#ifdef __KERNEL__

#define unlikely_notrace(x)	__builtin_expect(!!(x), 0)
#define likely_notrace(x)	__builtin_expect(!!(x), 1)

#if defined(CONFIG_TRACE_BRANCH_PROFILING) && !defined(DISABLE_BRANCH_PROFILING) && !defined(__CHECKER__)
void ftrace_likely_update(struct ftrace_likely_data *f, int val, int expect, int is_constant);

#define __branch_check__(val, expectation, is_constant) ({			\
			long ______depends_r;					\
			static struct ftrace_likely_data		\
				__aligned(4)				\
				__section("_ftrace_annotated_branch")	\
				______depends_f = {				\
                .data.line = __LINE__,			\
                .data.file = __FILE__,			\
				.data.func = __func__,			\
			};						\
			______depends_r = __builtin_expect(!!(val), expectation);	\
			ftrace_likely_update(&______depends_f, ______depends_r,		\
					     expectation, is_constant);	\
			______depends_r;					\
		})

# ifndef likely
#  define likely(cond)	(__branch_check__(cond, 1, __builtin_constant_p(cond)))
# endif
# ifndef unlikely
#  define unlikely(cond)	(__branch_check__(cond, 0, __builtin_constant_p(cond)))
# endif

#else
# define unlikely(x)	__builtin_expect(!!(x), 0)
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely_notrace(x)	unlikely(x)
# define likely_notrace(x)	likely(x)
#endif

#ifndef barrier_before_unreachable
# define barrier_before_unreachable() do { } while (0)
#endif

#ifndef barrier_data
# define barrier_data(p) __asm__ __volatile__("": :"r"(p) :"memory")
#endif

#ifndef barrier
# define barrier() __asm__ __volatile__("": : :"memory")
#endif


#ifdef CONFIG_OBJTOOL
#define __stringify_label(x) #x

#define __annotate_unreachable(a) ({					\
	asm volatile(__stringify_label(a) ":\n\t"			\
		     ".pushsection .discard.unreachable\n\t"		\
		     ".long " __stringify_label(a) "b - .\n\t"		\
		     ".popsection\n\t" : : "i" (a));			\
})
#define annotate_unreachable() __annotate_unreachable(__COUNTER__)

#define __annotate_jump_table __section(".rodata..c_jump_table")

#else
#define annotate_unreachable()
#define __annotate_jump_table
#endif

#ifndef unreachable /* depends */
# define unreachable() do {	 \
	annotate_unreachable();	 \
	__builtin_unreachable(); \
} while (0)
#endif

#ifndef KENTRY
# define KENTRY(sym)						\
	extern typeof(sym) sym;					\
	static const unsigned long __kentry_##sym		\
	__used __attribute__((__section__("___kentry+" #sym)))		\
	= (unsigned long)&sym
#endif

#ifndef RELOC_HIDE
# define RELOC_HIDE(p, offset)					\
  ({ unsigned long __ptr;					\
     __ptr = (unsigned long) (p);				\
    (typeof(p)) (__ptr + (offset)); })
#endif

#define absolute_pointer(val)	RELOC_HIDE((void *)(val), 0)

#ifndef __UNIQUE_ID
# define __UNIQUE_ID(prefix_str) __PASTE(__PASTE(__UNIQUE_ID_, prefix_str), __LINE__)
#endif

#ifndef OPTIMIZER_HIDE_VAR
#define OPTIMIZER_HIDE_VAR(var)	__asm__ ("" : "=r" (var) : "0" (var))
#endif

#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define ___ADDRESSABLE(sym, __attrs) \
	static void * __used __attrs \
		__UNIQUE_ID(__PASTE(__addressable_, (sym))) = (void *)&(sym)
#define __ADDRESSABLE(sym) \
	___ADDRESSABLE((sym), __section(".discard.addressable"))

static inline void *offset_to_ptr(const int *offset)
{
	return (void *)((unsigned long)offset + *offset);
}

#endif

#define __must_be_array(array)	BUILD_BUG_ON_ZERO(__same_type((array), &(array)[0]))
#define is_signed_type(type) (((type)(-1)) < (__force type)1)
#define prevent_tail_call_optimization()	mb()

#define READ_ONCE(x) (x)
#define WRITE_ONCE(x, val) ((x) = (val))

#endif
