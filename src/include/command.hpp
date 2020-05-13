// Name   : command.hpp
// Author : Modnar
// Date   : 2020/05/13

#ifndef _COMMAND_HPP
#define _COMMAND_HPP

#include "server.hpp"

extern int parse_command(const char *cmdline);
extern void process_history_message();

#endif /* _COMMAND_HPP */
