#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SYSCALL(n, name, ...) j6_status_t _syscall_ ## name (__VA_ARGS__);
#include "syscalls.inc"
#undef SYSCALL

#ifdef __cplusplus
}
#endif
