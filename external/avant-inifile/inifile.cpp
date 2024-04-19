#include <fstream>

#include "inifile.h"

namespace avant
{
    namespace inifile
    {
        inifile::inifile(const string &filename)
        {
            load(filename);
        }

        string inifile::trim(string s)
        {
            if (s.empty())
            {
                return s;
            }
            s.erase(0, s.find_first_not_of(" \r\n"));
            s.erase(s.find_last_not_of(" \r\n") + 1);
            return s;
        }

        bool inifile::load(const string &filename)
        {
            m_filename = filename;
            m_inifile.clear();
            string name;
            string line;
            // read ini file
            ifstream f(filename.c_str());
            if (f.fail())
            {
                cout << "loading file failed: " << m_filename << " is not found" << endl;
                return false;
            }
            cout << "loading file sucess: " << m_filename << endl;
            // parse content
            while (std::getline(f, line))
            {
                line = trim(line);
                if ('[' == line[0]) // section tag
                {
                    int pos = line.find_first_of(']');
                    if (-1 != pos)
                    {
                        name = trim(line.substr(1, pos - 1));
                        m_inifile[name];
                    }
                }
                else if ('#' == line[0]) // comment
                {
                    continue; // not parsing commment line
                }
                else // the line key=value
                {
                    // find =
                    int pos = line.find_first_of('=');
                    if (pos > 0)
                    {
                        string key = trim(line.substr(0, pos));
                        string value = trim(line.substr(pos + 1, line.size() - pos));
                        decltype(m_inifile)::iterator it = m_inifile.find(name);
                        if (it == m_inifile.end())
                        {
                            printf("parsing error: section=%s key=%s\n", name.c_str(), key.c_str());
                            return false;
                        }
                        m_inifile[name][key] = value;
                    }
                }
            }
            return true;
        }

        void inifile::save(const string &filename)
        {
            ofstream f(filename.c_str());
            std::map<string, std::map<string, value>>::iterator it;
            for (it = m_inifile.begin(); it != m_inifile.end(); ++it)
            {
                f << "[" << it->first << "]" << endl; // section tag
                for (std::map<string, value>::iterator iter = it->second.begin(); iter != it->second.end(); ++iter)
                {
                    // write key=value
                    f << iter->first << " = " << (string)iter->second << endl;
                }
                f << endl;
            }
        }

        ostream &inifile::operator<<(ostream &os)
        {
            decltype(m_inifile)::iterator it;
            for (it = m_inifile.begin(); it != m_inifile.end(); ++it)
            {
                os << "[" << it->first << "]" << endl; // section tag
                for (std::map<string, value>::iterator iter = it->second.begin(); iter != it->second.end(); ++iter)
                {
                    // write key=value
                    os << iter->first << " = " << (string)iter->second << endl;
                }
                os << endl;
            }
            return os;
        }

        void inifile::clear()
        {
            m_inifile.clear();
        }

        bool inifile::has(const string &section)
        {
            return (m_inifile.find(section) != m_inifile.end());
        }

        bool inifile::has(const string &section, const string &key)
        {
            decltype(m_inifile)::iterator it = m_inifile.find(section);
            if (it != m_inifile.end())
            {
                return (it->second.find(key) != it->second.end());
            }
            return false;
        }

        value &inifile::get(const string &section, const string &key)
        {
            return m_inifile[section][key];
        }

        void inifile::set(const string &section, const string &key, bool value)
        {
            m_inifile[section][key] = value;
        }

        void inifile::set(const string &section, const string &key, int value)
        {
            m_inifile[section][key] = value;
        }

        void inifile::set(const string &section, const string &key, double value)
        {
            m_inifile[section][key] = value;
        }

        void inifile::set(const string &section, const string &key, const string &value)
        {
            m_inifile[section][key] = value;
        }

        void inifile::remove(const string &section)
        {
            decltype(m_inifile)::iterator it = m_inifile.find(section);
            if (it != m_inifile.end())
                m_inifile.erase(it);
        }

        void inifile::remove(const string &section, const string &key)
        {
            decltype(m_inifile)::iterator it = m_inifile.find(section);
            if (it != m_inifile.end())
            {
                auto iter = it->second.find(key);
                if (iter != it->second.end())
                    it->second.erase(iter);
            }
        }
    }
}