#define LEN(a) (sizeof(a)/sizeof(*(a)))

void __declspec(noreturn) err(int code, const char *msg);
