#pragma once

#include "classinet_client_cpp_api.h"

#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <vector>
#include <fstream>


struct OS {
    static const std::string home_dir() {
        const char *homedir;
        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }
        return homedir;
    }

    static const std::string classinet_config_dir() {
        return home_dir() + "/.classinet";
    }

    static const std::string classinet_config_file() {
        return classinet_config_dir() + "/configure";
    }

    static const std::vector<std::string> expand_wildcard(const std::string &pattern) {
        // We keep it simple, but safe
        std::string not_allowed{"><|&"};
        std::string p;
        for(auto c: pattern)
            if (std::string::npos == not_allowed.find(c))
                p.push_back(c);
        std::string tmp_filename{"/tmp/classinet_" + std::to_string(getpid())};
        std::string command{"realpath " + p + " >" + tmp_filename};
        std::system(command.c_str());
        std::ifstream stream(tmp_filename, std::ios::in | std::ios::binary);
        if (!stream.good())
            return {};
        std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        return classinet::tokenize(contents, "\n");
    }

};
