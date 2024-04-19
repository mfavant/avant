// #pragma once
// #include <string>
// #include <vector>

// #include "engine/context.h"
// #include "engine/plugin.h"

// namespace avant
// {
//     namespace engine
//     {
//         class work
//         {
//         public:
//             work();
//             work(const std::string &name, bool flag);
//             ~work();
//             void append(plugin *plugin);
//             void set_name(const std::string &name);
//             void set_switch(bool flag);
//             bool get_switch() const;
//             bool run(context &ctx);

//         protected:
//             std::string m_name{};
//             bool m_switch{false};
//             std::vector<plugin *> m_plugins{};
//         };
//     }
// }