/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __DMI_H__
#define __DMI_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern u64 dmi_memdev_size(u16 handle);
extern u8 dmi_memdev_type(u16 handle);
extern u16 dmi_memdev_handle(int slot);

#ifdef __cplusplus
}
#endif

#define DMI_ENTRY_MEM_DEVICE 17

struct dmi_header {
	u8 type;
	u8 length;
	u16 handle;
};

extern int dmi_walk(void (*decode)(const struct dmi_header *, void *),
		    void *private_data);

#endif	/* __DMI_H__ */
