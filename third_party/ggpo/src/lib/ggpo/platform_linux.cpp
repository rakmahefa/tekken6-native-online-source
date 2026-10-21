/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net) - POSIX platform configuration.
 * ----------------------------------------------------------------------- */
#include "types.h"

#include <cctype>
#include <cstring>
#include <strings.h>

int Platform::GetConfigInt(const char* name)
{
   if (!name) return 0;
   const char* value = std::getenv(name);
   if (!value || !*value) return 0;
   char* end = nullptr;
   const long parsed = std::strtol(value, &end, 10);
   return end == value ? 0 : static_cast<int>(parsed);
}

bool Platform::GetConfigBool(const char* name)
{
   if (!name) return false;
   const char* value = std::getenv(name);
   if (!value || !*value) return false;
   return std::strtol(value, nullptr, 10) != 0 || strcasecmp(value, "true") == 0;
}
