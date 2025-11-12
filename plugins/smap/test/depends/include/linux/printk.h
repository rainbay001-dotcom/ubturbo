/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KERNEL_PRINTK__
#define __KERNEL_PRINTK__

#include <stdio.h>

#define KERN_SOH	"\001"
#define KERN_DEFAULT	KERN_SOH "[INFO]\t"
#define KERN_DEBUG	KERN_SOH "[DEBUG]\t"
#define KERN_INFO	KERN_SOH "[INFO]\t"
#define KERN_NOTICE	KERN_SOH "[NOTICE]\t"
#define KERN_WARNING	KERN_SOH "[WARNING]\t"
#define KERN_ERR	KERN_SOH "[ERR]\t"
#define KERN_CRIT	KERN_SOH "[CRIT]\t"
#define KERN_ALERT	KERN_SOH "[ALERT]\t"
#define KERN_EMERG	KERN_SOH "[EMERG]\t"

#define KERN_CONT	KERN_SOH "c"

#define printk printf

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define pr_debug(fmt, ...)      printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_devel(fmt, ...)      printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)       printk(KERN_CONT fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)       printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)     printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)       printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...)        printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...)       printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)      printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...)      printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)

#define printk_once(fmt, ...)	printk(fmt, ##__VA_ARGS__)
#define printk_deferred_once(fmt, ...)	printk(fmt, ##__VA_ARGS__)

#define pr_emerg_once(fmt, ...)     printk_once(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert_once(fmt, ...)     printk_once(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit_once(fmt, ...)      printk_once(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err_once(fmt, ...)       printk_once(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn_once(fmt, ...)      printk_once(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice_once(fmt, ...)    printk_once(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info_once(fmt, ...)      printk_once(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#endif

