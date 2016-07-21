### pyxis介绍

pyxis是一个分布式的，提供以路径node为K值的KV存储的高可用服务。pyxis使用了raft一致性协议。

pyxis使用c++实现，提供go语言封装的客户端。

客户端能够直接通过包函数调用实现节点的增删改查。

### pyxis的编译安装

环境：centos6.4

安装准备：

    1.安装依赖 gcc,g++,zlib(zlib-devel),scons,boost-1.5.3(boost-devel)
    
    2.编译pyxis，需要下载安装第三方依赖库，这些依赖库需要和pyxis所处同一级目录，该目录作为整个项目编译安装的root目录，执行pyxis包内的build.sh脚本,会自动下载安装；
    
    下载的依赖库有:
    1.Blade构建系统
    2.thirdparty,toft
    3.raft,muduo,sofa-pbrpc
    
安装：
	  1.执行pyxis包内的build.sh
	 
	  2.编译过程需要花费一些时间，编译完成后执行 alt 即能切换至编译的结果存放目录，看到最终的二进制文件。如果alt命令找不到，二进制的目录为blade-bin/pyxis/cmd/pyxisd
	  
运行参数:

pyxis运行需要设置运行参数，通过--help查看参数说明。

```
Flags from pyxis/cmd/pyxisd/main.cc:
    --dbdir (leveldb dir) type: string default: "db"         
    --election_timeout (election timeout in ms) type: int32  //集群选举超时时间；
      default: 1000
    --force_flush (force flush in raft log) type: bool
      default: false
    --heartbeat_timeout (heartbeat timeout in ms) type: int32 //心跳间隔时间；
      default: 50
    --max_commit_size (max commit size in one loop) type: int32
      default: 200
    --max_timeout (max session timeout) type: int32 //设置回话过期时间的上限；
      default: 120000
    --min_timeout (min session timeout) type: int32 //设置回话过期时间的下限；
      default: 5000
    --myid (my id) type: int32 default: 0  //集群中当前机器的id, 0< myid <= len(pyxis_list)；
    --pprof (port of pprof) type: int32 default: 9982 //查看服务运行内部状态的接口；
    --pyxis_list (address list of pyxis   //pyxis集群的服务实例列表，格式是ip:port, 多个用逗号分割；
      servers(host1:port1,host2:port2)) type: string
      default: ""
    --pyxis_listen_addr (pyxis listen address) type: string
      default: "0.0.0.0:7000"
    --raft_list (address list of raft servers(host1:port1,host2:port2))
      type: string default: ""
    --raft_listen_addr (raft listen address) type: string
      default: "0.0.0.0:7001"
    --raftlog (raft log dir) type: string
      default: "raftlog"
    --rpc_channel_size (rpc channel size) type: int32
      default: 1024
    --session_interval (session check interval) type: int32
      default: 1000
    --snapshot_interval ( take snapshot interval in second.)
      type: uint64 default: 86400
    --snapshot_size (commit log size to take snapshot.)
      type: uint64 default: 1000000
    --snapshotdir (snapshot dir) type: string
      default: "snapshot"
      
 Flags from pyxis/server/store.cc:
    --cache_size (leveldb cache size, in MB) type: int32
      default: 100
    --max_node_size (node size, in KB) type: int32
      default: 1024

```

### pyxis的使用

pyxis提供go语言实现的客户端,封装了对pyxis节点的增删改查接口。

通过godoc查看包函数，以及操作示例。



