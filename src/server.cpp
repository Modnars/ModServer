// Name   : server.cpp
// Author : Modnar
// Date   : 2020/04/04

#include <iostream>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "server.hpp"

void ClientData::setCliFd(int fd) {
    _clifd = fd;
}

void ClientData::setUserName(const std::string &name) {
    _name = name;
}

void ClientData::setLoginTime(struct tm *currTime) {
    if (currTime) {
        _loginTime = *currTime;
    }
}

int main(int argc, char *argv[]) {
    return EXIT_SUCCESS;
}
