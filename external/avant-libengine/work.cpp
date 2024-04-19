// #include "engine/work.h"
// #include "engine/plugin.h"
// #include "engine/plugin_loader.h"
// #include "utility/singleton.h"

// using namespace avant::engine;
// using namespace avant::utility;

// work::work(const std::string &name, bool flag) : m_name(name), m_switch(flag)
// {
// }

// work::work() : m_name(""), m_switch(false)
// {
// }

// work::~work()
// {
//     for (auto it = m_plugins.begin(); it != m_plugins.end(); it++)
//     {
//         void (*destory)(plugin *) = (void (*)(plugin *))singleton<plugin_loader>::instance()->get((*it)->get_name(), "destory");
//         destory(dynamic_cast<plugin *>(*it));
//     }
// }

// void work::append(plugin *plugin)
// {
//     m_plugins.push_back(plugin);
// }

// void work::set_name(const std::string &name)
// {
//     m_name = name;
// }

// void work::set_switch(bool flag)
// {
//     m_switch = flag;
// }

// bool work::get_switch() const
// {
//     return m_switch;
// }

// bool work::run(context &ctx)
// {
//     // run plugin pipline
//     for (auto it = m_plugins.begin(); it != m_plugins.end(); it++)
//     {
//         if ((*it)->get_switch()) // work'state is on
//         {
//             if (!((*it)->run(ctx)))
//             {
//                 break;
//             }
//         }
//     }
//     return true;
// }