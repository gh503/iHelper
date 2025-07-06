#ifndef PLATFORM_FACTORY_H
#define PLATFORM_FACTORY_H

#include <memory>
#include "ihelper/interfaces/iplatform_adapter.h"

class PlatformFactory {
public:
    static std::unique_ptr<IPlatformAdapter> create_adapter();
};

#endif // PLATFORM_FACTORY_H