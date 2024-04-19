// #pragma once

// #include <string>
// #include <map>

// namespace avant
// {
//     namespace engine
//     {
//         class plugin_loader
//         {
//         public:
//             plugin_loader();
//             ~plugin_loader();
//             /**
//              * @brief loading plugin
//              *
//              * @param plugin
//              */
//             void load(const std::string &plugin);
//             /**
//              * @brief close plugin
//              *
//              * @param plugin
//              */
//             void unload(const std::string &plugin);
//             /**
//              * @brief get handler from plugin
//              *
//              * @param plugin
//              * @param symbol
//              * @return void*
//              */
//             void *get(const std::string &plugin, const std::string &symbol);

//         private:
//             std::map<std::string, void *> m_plugins{};
//         };
//     }
// }