// Name   : server.hpp
// Author : Modnar
// Date   : 2020/04/12

#ifndef _SERVER_HPP
#define _SERVER_HPP

#include <string>

class ClientData {
public:
    ClientData() : _clifd(-1) { }
    void setCliFd(int fd);
    void setUserName(const std::string &name);
    void setLoginTime(struct tm *currTime);

private:
    int _clifd;
    std::string _name;
    struct tm _loginTime;
};

#endif /* _SERVER_HPP */
