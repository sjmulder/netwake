#include <string.h>

#ifdef NOSTDLIB
size_t
strlen(const char *s)
{
	size_t len;

	for (len=0; s[len]; len++)
		;

	return len;
}
#endif