/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_PGTABLE_TYPES_H
#define __ASM_PGTABLE_TYPES_H

#include <asm/types.h>
#include <asm-generic/pgtable-nop4d.h>

typedef u64 pgdval_t;
typedef u64 p4dval_t;
typedef u64 pudval_t;
typedef u64 pmdval_t;
typedef u64 pteval_t;

typedef struct { pteval_t pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })

typedef struct { pgdval_t pgd; } pgd_t;
#define pgd_val(x)	((x).pgd)
#define __pgd(x)	((pgd_t) { (x) })

typedef struct { p4dval_t p4d; } p4d_t;
#define p4d_val(x)	((x).p4d)
#define __p4d(x)	((p4d_t) { (x) })

#if CONFIG_PGTABLE_LEVELS > 3
typedef struct { pudval_t pud; } pud_t;
#define pud_val(x)	((x).pud)
#define __pud(x)	((pud_t) { (x) })
#endif

#if CONFIG_PGTABLE_LEVELS > 2
typedef struct { pmdval_t pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) })
#endif

typedef struct { pteval_t pte; } pte_t;
#define pte_val(x)	((x).pte)
#define __pte(x)	((pte_t) { (x) })


#endif
