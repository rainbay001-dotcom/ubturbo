/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_UMH_H__
#define __LINUX_UMH_H__

#define UMH_NO_WAIT	0 // don't wait at all
#define UMH_WAIT_EXEC	1 // wait for the exec, but not the process
#define UMH_WAIT_PROC	2 // wait for the process to complete
#define UMH_KILLABLE	4 // wait for EXEC/PROC killable

#endif /* __LINUX_UMH_H__ */
