# 简介
Pyxis是一个类似Chubby和Zookeeper的高可用名字服务，使用raft一致性协议

# 新建客户端
Pyxis使用多个服务端实例来保证高可用，因此在连接的时候需要指定一个机器列表，同时需要提供一个默认的回调函数，这个回调函数用来接收特殊事件，如连接建立，连接断开之类的。
连接选项里面可以指定使用某个session id来进行注册，这个选项一般用在挂掉的服务重新连接同时恢复之前的会话。示例如下：
``` c++
#include <pyxis/client/client.h>

void defaultCallback(const pyxis::watch::Event& event)
{
}

pyxis::Client* cli;
pyxis::Options options;
std::string addr = "127.0.0.1:9981,127.0.0.1:9982,127.0.0.1:9983";
pyxis::Status st = pyxis::Client::Open(options, addr, defaultCallback, &cli);
CHECK(st.ok());

```

# Status

`pyxis::Status` 类型用来指示可能返回的错误。你可以通过检查它来确定某次调用是否成功，如果错误获取错误信息。
``` c++
pyxis::Status st = ...;
if (!st.ok()) {
    std::cerr << st.ToString() << std::endl;
}
```
# 关闭客户端
当你使用完一个客户端，直接删除那个客户端对象，这个操作会在服务端进行注销所有使用这个客户端创建的临时节点以及watcher。
``` c++
pyxis::Client* cli = ...
使用cli进行操作...
delete cli;
```

# 增删改查
客户端提供了 `Create`, `Delete`, `Write`, `Read`, `Stat`方法来和服务端通信，
