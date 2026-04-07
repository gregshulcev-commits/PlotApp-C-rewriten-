#include "plotapp/BuildInfo.h"

#ifndef PLOTAPP_VERSION_STRING
#define PLOTAPP_VERSION_STRING "unknown"
#endif

namespace plotapp {

std::string plotappVersionString() {
    return PLOTAPP_VERSION_STRING;
}

} // namespace plotapp
