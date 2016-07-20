#include <pyxis/server/server.h>
#include <pyxis/rpc/server.h>
#include <pyxis/server/fake_raft.h>
#include <sofa/pbrpc/pbrpc.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/inspect/Inspector.h>

#include <raft/core/transporter/sofarpc.h>
#include <raft/core/server.h>

#include <toft/base/string/algorithm.h>
#include <toft/base/binary_version.h>

#include <boost/scoped_ptr.hpp>

DEFINE_string(pyxis_list, "", "address list of pyxis servers(host1:port1,host2:port2)");
DEFINE_string(pyxis_listen_addr, "0.0.0.0:7000", "pyxis listen address");
DEFINE_string(raft_listen_addr, "0.0.0.0:7001", "raft listen address");
DEFINE_int32(pprof, 9982, "port of pprof");
DEFINE_string(dbdir, "db", "leveldb dir");
DEFINE_int32(session_interval, 1000, "session check interval");
DEFINE_int32(max_timeout, 2 * 60 * 1000, "max session timeout");
DEFINE_int32(min_timeout, 5 * 1000, "max session timeout");
DEFINE_int32(rpc_channel_size, 1024, "rpc channel size");

// for raft
DEFINE_string(raft_list, "", "address list of raft servers(host1:port1,host2:port2)");
DEFINE_string(snapshotdir, "snapshot", "snapshot dir");
DEFINE_uint64(snapshot_size, 1000000, "commit log size to take snapshot.");
DEFINE_uint64(snapshot_interval, 3600 * 24, " take snapshot interval in second.");
DEFINE_int32(max_commit_size, 200, "max commit size in one loop");
DEFINE_string(raftlog, "raftlog", "raft log dir");
DEFINE_bool(force_flush, false, "force flush in raft log");
DEFINE_int32(election_timeout, 1000, "election timeout in ms");
DEFINE_int32(heartbeat_timeout, 50, "heartbeat timeout in ms");
DEFINE_int32(myid, 0, "my id");

int main(int argc, char* argv[])
{
  toft::SetupBinaryVersion();
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<std::string> raftList;
  std::vector<std::string> pyxisList;
  toft::SplitString(FLAGS_raft_list, ",", &raftList);
  toft::SplitString(FLAGS_pyxis_list, ",", &pyxisList);

  pyxis::Options options;
  options.DBDir = FLAGS_dbdir;
  options.SessionCheckInterval = FLAGS_session_interval;
  options.MaxSessionTimeout = FLAGS_max_timeout;
  options.MinSessionTimeout = FLAGS_min_timeout;
  options.SnapshotDir = FLAGS_snapshotdir;
  options.MyID = FLAGS_myid;
  options.AddrList = pyxisList;

  uint32_t myid = FLAGS_myid;
  uint32_t myindex = myid - 1;
  CHECK(myid > 0);
  CHECK(myid <= pyxisList.size());
  CHECK(myid <= raftList.size());

  // 监控线程
  boost::scoped_ptr<muduo::net::EventLoopThread> inspectThread;
  boost::scoped_ptr<muduo::net::Inspector> inspector;
  if (FLAGS_pprof != 0) {
    inspectThread.reset(new muduo::net::EventLoopThread);
    inspector.reset(new muduo::net::Inspector(inspectThread->startLoop(),
                                              muduo::net::InetAddress(FLAGS_pprof),
                                              "inspect"));
  }

  raft::sofarpc::Transporter trans;
  raft::Options raft_options;
  raft_options.MyID = FLAGS_myid;
  raft_options.RaftLogDir = FLAGS_raftlog;
  raft_options.SnapshotLogSize = FLAGS_snapshot_size;
  raft_options.SnapshotInterval = FLAGS_snapshot_interval;
  raft_options.ForceFlush = FLAGS_force_flush;
  raft_options.MaxCommitSize = FLAGS_max_commit_size;
  raft_options.HeartbeatTimeout = FLAGS_heartbeat_timeout;
  raft_options.ElectionTimeout = FLAGS_election_timeout;
  raft::Server raftServer(raft_options, &trans);
  raft::sofarpc::RpcServer raftRpcServer(FLAGS_raft_listen_addr, raftServer.RpcEventChannel());
  for (size_t i = 0; i < raftList.size(); i++) {
    if (i == myindex) {
      continue;
    }
    trans.AddPeer(raftList[i]);
    raftServer.AddPeer(raftList[i]);
  }

  // raft初始化
  pyxis::Server pyxisServer(options, &raftServer);

  muduo::net::EventLoop loop;
  pyxis::rpc::Server rpcServer(&loop, FLAGS_pyxis_listen_addr, &pyxisServer);

  rpcServer.Start();
  pyxisServer.Start();

  raftRpcServer.Start();
  raftServer.Start();

  loop.loop();
  return 0;
}
