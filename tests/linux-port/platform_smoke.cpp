#include "t6_platform.h"

int main()
{
    int value = 42;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(&value, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT)
        return 1;

    DWORD previous = PAGE_NOACCESS;
    if (!VirtualProtect(&value, sizeof(value), PAGE_READWRITE, &previous))
        return 2;
    if (previous == PAGE_NOACCESS)
        return 3;
    if (!VirtualProtect(&value, sizeof(value), previous, nullptr))
        return 4;

    return 0;
}
