#include <stddef.h>

#ifdef NOSTDLIB
void *
memset(void *ptr, int val, size_t sz)
{
	size_t i;

	for (i=0; i < sz; i++)
		((char *)ptr)[i] = (char)val;

	return ptr;
}
#endif /* FREESTANDING */
