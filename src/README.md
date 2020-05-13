# _ModChatRoom_

&#160; &#160; &#160; &#160; 一款支持多人在线的聊天室服务器/客户端。

## 功能

- **聊天(待实现)**: 正常发送消息即可被其他用户看见，参与多人聊天。

- **历史消息**: 使用命令 _`history`_ 来查看历史消息。

```
$ history
```

- **上传文件(待实现)**: 使用命令 _`push`_ 来上传文件。

```
$ push filepath
```

- **下载文件(待实现)**: 使用命令 _`pull`_ 来下载文件。

```
$ pull filename
```

- **查看用户信息(待实现)**: 使用命令 _`info`_ 来查看用户信息。

```
$ info [user_id]
```

&#160; &#160; &#160; &#160; 当命令不添加用户ID参数时，该功能为查询自己的用户信息。

## 目录

```txt
./
├── Makefile
├── README.md
├── client.cpp
├── command.cpp
├── include/
│   ├── command.hpp
│   └── server.hpp
├── old_client.cpp
└── server.cpp

1 directory, 8 files
```
