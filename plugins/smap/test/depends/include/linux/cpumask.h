/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_CPUMASK_H
#define __LINUX_CPUMASK_H

#include <linux/threads.h>
#include <linux/bitops.h>

#define num_online_cpus()	2U

#define num_possible_cpus() 2U

#define for_each_online_cpu(cpu)	for ((cpu) = 0; (cpu) < 2; (cpu)++)

typedef struct cpumask { unsigned long bits[BITS_TO_LONGS(NR_CPUS)]; } cpumask_t;

typedef struct cpumask cpumask_var_t[1];

#endif /* __LINUX_CPUMASK_H */
