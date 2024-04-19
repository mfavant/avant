// #include <string>
// #include <avant-log/logger.h>
// #include "engine/workflow.h"
// #include "utility/singleton.h"
// #include "engine/plugin.h"
// #include "engine/plugin_loader.h"
// #include "engine/context.h"

// using namespace avant::engine;
// using namespace avant::xml;
// using namespace avant::log;
// using namespace avant::utility;

// workflow::~workflow()
// {
//     // delete all work
//     for (auto it = m_works.begin(); it != m_works.end(); ++it)
//     {
//         delete it->second;
//         it->second = nullptr;
//     }
//     m_works.clear();
// }

// bool workflow::load(const std::string &path)
// {
//     document doc;
//     doc.load_file(path);
//     element root = doc.parse();
//     // workflow.xml form,foreach works
//     for (auto it = root.begin(); it != root.end(); ++it)
//     {
//         std::string &&name = std::move(it->attr("name"));
//         std::string &&flag = std::move(it->attr("switch"));
//         work *new_work = new work;
//         new_work->set_name(name);
//         if (flag == "on") // work on
//         {
//             new_work->set_switch(true);
//         }
//         else if (flag == "off") // work off
//         {
//             new_work->set_switch(false);
//         }
//         else
//         {
//             return false;
//         }
//         if (!load_plugin(*new_work, (*it)))
//         {
//             return false;
//         }
//         m_works[name] = new_work;
//     }
//     return true;
// }

// bool workflow::run(const std::string &work, const std::string &input, std::string &output)
// {
//     // find the work that named ${work}
//     auto it = m_works.find(work);
//     if (it == m_works.end()) // not found
//     {
//         return false;
//     }
//     if (!it->second->get_switch()) // the work'state is close
//     {
//         return false;
//     }
//     context ctx; // creating context
//     ctx.set<std::string>("input", input);
//     if (!it->second->run(ctx)) // execute plugins in the work
//     {
//         return false;
//     }
//     output = ctx.ref<std::string>("output"); // the data return to client
//     return true;
// }

// bool workflow::load_plugin(work &work, avant::xml::element &el)
// {
//     // foreach plugin in work
//     for (auto it = el.begin(); it != el.end(); it++)
//     {
//         if (it->name() != "plugin")
//         {
//             return false;
//         }
//         const std::string &&name = std::move(it->attr("name")); // plugin'name
//         // load function pointer
//         plugin *(*create)() = (plugin * (*)()) singleton<plugin_loader>::instance()->get(name, "create");
//         plugin *new_plugin = create(); // to create plugin from .so
//         new_plugin->set_name(name);
//         const std::string &&flag = std::move(it->attr("switch"));
//         if (flag == "on")
//         {
//             new_plugin->set_switch(true);
//         }
//         else if (flag == "off")
//         {
//             new_plugin->set_switch(false);
//         }
//         else
//         {
//             return false;
//         }
//         work.append(new_plugin); // adding plugin to work object
//     }
//     return true;
// }