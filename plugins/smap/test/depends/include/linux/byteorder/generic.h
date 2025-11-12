/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_BYTEORDER_GENERIC_H
#define _LINUX_BYTEORDER_GENERIC_H

#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) (x)
#define __le16_to_cpu(x) (x)
#define le16_to_cpu __le16_to_cpu
#endif /* _LINUX_BYTEORDER_GENERIC_H */
