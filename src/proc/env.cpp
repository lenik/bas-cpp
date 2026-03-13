#include "env.hpp"

namespace bas {

std::string getHomePath() {
    std::string homePath = getenv("HOME");
    if (homePath.empty()) {
#ifdef _WIN32
        homePath = getenv("USERPROFILE");
#else
        std::string user = getenv("USER");
        if (user.empty()) {
            homePath = "/";
        } else {
            homePath = "/home/" + user;
        }
#endif
    }
    return homePath;
}

} // namespace bas