/// \file fon9/framework/NamedIoManager.cpp
/// \author fonwinz@gmail.com
#include "fon9/framework/IoManagerTree.hpp"
#include "fon9/seed/SysEnv.hpp"
#include "fon9/seed/Plugins.hpp"

namespace fon9 {

static bool NamedIoManager_Start(seed::PluginsHolder& holder, StrView args) {
   IoManagerArgs iomArgs;
   StrView       tag, value;
   while (fon9::SbrFetchTagValue(args, tag, value)) {
      iomArgs.SetTagValue(holder, tag, value);
   }
   if (!iomArgs.Name_.empty() && IoManagerTree::Plant(*holder.Root_, iomArgs))
      return true;
   holder.SetPluginsSt(fon9::LogLevel::Error, "Name=", iomArgs.Name_, "|err='Name' is dup or empty");
   return false;
}

} // namespaces

extern "C" fon9_API fon9::seed::PluginsDesc f9p_NamedIoManager;
static fon9::seed::PluginsPark f9pAutoPluginsReg{"NamedIoManager", &f9p_NamedIoManager};

fon9::seed::PluginsDesc f9p_NamedIoManager{
   "",
   &fon9::NamedIoManager_Start,
   nullptr,
   nullptr,
};
