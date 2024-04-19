// #include <dlfcn.h>
// #include <assert.h>
// #include <string>
// #include <cstdio>
// #include <cstring>

// #include "plugin_loader.h"
// #include "system/system.h"
// #include "utility/singleton.h"

// using std::map;
// using std::string;
// using namespace avant::engine;
// using namespace avant::system;
// using namespace avant::utility;

// plugin_loader::plugin_loader()
// {
// }

// plugin_loader::~plugin_loader()
// {
// }

// void plugin_loader::load(const std::string &plugin)
// {
//     assert(!plugin.empty());
//     assert(m_plugins.find(plugin) == m_plugins.end()); // plugin is not exist
//     string filename = singleton<system::system>::instance()->get_root_path() + "/plugin/" + plugin;
//     // loading .so lib
//     void *handle = dlopen(filename.c_str(), RTLD_GLOBAL | RTLD_LAZY);
//     printf("load plugin %s %s\n", filename.c_str(), dlerror());
//     assert(handle != nullptr);
//     m_plugins[plugin] = handle; // cache plugin
// }

// void plugin_loader::unload(const std::string &plugin)
// {
//     assert(!plugin.empty());
//     auto it = m_plugins.find(plugin);
//     assert(it != m_plugins.end());
//     dlclose(m_plugins[plugin]); // close handler
//     m_plugins.erase(it);        // delete from cache
// }

// void *plugin_loader::get(const string &plugin, const string &symbol)
// {
//     auto it = m_plugins.find(plugin);
//     if (it == m_plugins.end()) // not found
//     {
//         load(plugin);
//         // retry
//         it = m_plugins.find(plugin);
//         assert(it != m_plugins.end());
//     }
//     void *func = dlsym(it->second, symbol.c_str()); // get funtion fron plugin
//     assert(func != nullptr);
//     return func;
// }
