#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

#include "inifile.h"

namespace avant
{
    namespace inifile
    {
        inifile::inifile(const std::string &filename)
        {
            load(filename);
        }

        std::string inifile::trim(std::string_view s)
        {
            const std::size_t first = s.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return {};
            }
            const std::size_t last = s.find_last_not_of(" \t\r\n");
            return std::string(s.substr(first, last - first + 1));
        }

        bool inifile::load(const std::string &filename)
        {
            m_filename = filename;
            m_inifile.clear();
            m_missing_section.clear();
            std::string name;
            std::string line;
            // read ini file
            std::ifstream f(filename);
            if (f.fail())
            {
                std::cerr << "loading file failed: " << filename << " is not found" << std::endl;
                return false;
            }
            std::cout << "loading file success: " << filename << std::endl;
            // parse content (lenient by design: unparseable lines are skipped
            // with a warning instead of failing the whole load; validating the
            // parsed values is the caller's job, e.g. config_mgr::init)
            while (std::getline(f, line))
            {
                line = trim(line);
                if (line.empty())
                {
                    continue;
                }
                if (line[0] == '[') // section tag
                {
                    const std::size_t pos = line.find_first_of(']');
                    if (pos != std::string::npos)
                    {
                        const std::string section_name = trim(line.substr(1, pos - 1));
                        if (section_name.empty())
                        {
                            std::cerr << "parsing warning: skipping empty section tag in " << filename << std::endl;
                            // keep the previous section: keys after an empty
                            // tag keep landing where they were
                            continue;
                        }
                        name = section_name;
                        m_inifile[name];
                    }
                    else
                    {
                        std::cerr << "parsing warning: skipping section tag without closing bracket: " << line << std::endl;
                    }
                }
                else if (line[0] == '#') // comment
                {
                    continue; // not parsing comment line
                }
                else // the line key=value
                {
                    // find =
                    const std::size_t pos = line.find_first_of('=');
                    if (pos == std::string::npos || pos == 0)
                    {
                        continue;
                    }
                    std::string key = trim(line.substr(0, pos));
                    if (key.empty())
                    {
                        continue;
                    }
                    const std::string value = trim(line.substr(pos + 1));
                    auto it = m_inifile.find(name);
                    if (it == m_inifile.end())
                    {
                        std::cerr << "parsing warning: skipping key outside any section: " << line << std::endl;
                        continue;
                    }
                    it->second[key] = value;
                }
            }
            return true;
        }

        bool inifile::save(const std::string &filename)
        {
            std::ofstream f(filename);
            if (f.fail())
            {
                std::cerr << "saving file failed: " << filename << " is not writable" << std::endl;
                return false;
            }
            *this << f;
            f.flush();
            if (!f.good())
            {
                std::cerr << "saving file failed: " << filename << std::endl;
                return false;
            }
            return true;
        }

        std::ostream &inifile::operator<<(std::ostream &os)
        {
            for (auto &section : m_inifile)
            {
                os << "[" << section.first << "]" << std::endl; // section tag
                for (auto &entry : section.second)
                {
                    // write key=value
                    os << entry.first << " = " << static_cast<std::string>(entry.second) << std::endl;
                }
                os << std::endl;
            }
            return os;
        }

        void inifile::clear()
        {
            m_inifile.clear();
            m_missing_section.clear();
        }

        bool inifile::has(const std::string &section)
        {
            return (m_inifile.find(section) != m_inifile.end());
        }

        bool inifile::has(const std::string &section, const std::string &key)
        {
            auto it = m_inifile.find(section);
            if (it != m_inifile.end())
            {
                return (it->second.find(key) != it->second.end());
            }
            return false;
        }

        const value &inifile::get(const std::string &section, const std::string &key)
        {
            auto it = m_inifile.find(section);
            if (it == m_inifile.end())
            {
                return m_default_value;
            }
            auto key_it = it->second.find(key);
            if (key_it == it->second.end())
            {
                return m_default_value;
            }
            return key_it->second;
        }

        void inifile::set(const std::string &section, const std::string &key, const std::string &value)
        {
            m_inifile[section][key] = value;
        }

        void inifile::remove(const std::string &section)
        {
            auto it = m_inifile.find(section);
            if (it != m_inifile.end())
            {
                m_inifile.erase(it);
            }
        }

        void inifile::remove(const std::string &section, const std::string &key)
        {
            auto it = m_inifile.find(section);
            if (it != m_inifile.end())
            {
                auto iter = it->second.find(key);
                if (iter != it->second.end())
                {
                    it->second.erase(iter);
                }
            }
        }

        std::map<std::string, value> &inifile::operator[](const std::string &key)
        {
            auto it = m_inifile.find(key);
            if (it == m_inifile.end())
            {
                return m_missing_section;
            }
            return it->second;
        }
    }
}
