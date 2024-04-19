// #pragma once
// #include <string>
// #include <map>
// #include <avant-xml/element.h>
// #include <avant-xml/document.h>

// #include "engine/work.h"

// namespace avant
// {
//     namespace engine
//     {
//         class workflow
//         {
//         public:
//             workflow() = default;
//             ~workflow();
//             /**
//              * @brief loading workflow.xml file
//              *
//              * @param path xml file path
//              * @return true
//              * @return false
//              */
//             bool load(const std::string &path);
//             /**
//              * @brief processing request form client
//              *
//              * @param work should be running
//              * @param input data from client
//              * @param output back output data for client
//              * @return true
//              * @return false
//              */
//             bool run(const std::string &work, const std::string &input, std::string &output);

//         private:
//             /**
//              * @brief load plugin
//              *
//              * @param work load plugin then to config work
//              * @param el
//              * @return true
//              * @return false
//              */
//             bool load_plugin(work &work, avant::xml::element &el);

//         private:
//             std::map<std::string, work *> m_works{};
//         };
//     }
// }