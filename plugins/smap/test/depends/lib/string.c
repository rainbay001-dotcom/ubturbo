/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>

size_t strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

int strncmp(const char *ns, const char *nt, size_t cnt)
{
	unsigned char n1, n2;

    // Loop until 'cnt' characters are compared or a null character is encountered
	while (cnt) {
		n1 = *ns++;
		n2 = *nt++;
		if (n1 != n2)
			return n1 < n2 ? -1 : 1;
		if (!n1)
			break;
		cnt--;
	}
    // If all characters are equal, return 0
	return 0;
}

char *strim(char *s)
{
    return NULL;
}