#ifndef IUSERDB_H
#define IUSERDB_H

#include <string>
#include <vector>

class IUserDb {
public:
    virtual ~IUserDb() = default;

    virtual std::string getUserName(int uid) const = 0;
    virtual std::string getGroupName(int gid) const = 0;
    virtual std::vector<int> getGroupIds(int uid) const = 0;
};

#endif // IUSERDB_H
