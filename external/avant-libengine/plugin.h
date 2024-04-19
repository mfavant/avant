// #pragma once
// #include <string>

// #include "engine/context.h"

// namespace avant
// {
//     namespace engine
//     {
//         class plugin
//         {
//         public:
//             plugin();
//             plugin(const std::string &name, bool flag);
//             virtual ~plugin();
//             void set_name(const std::string &name);
//             const std::string &get_name() const;
//             void set_switch(bool flag);
//             bool get_switch() const;
//             virtual bool run(context &ctx) = 0;

//         protected:
//             std::string m_name{};
//             bool m_switch{false};
//         };
//     }
// }