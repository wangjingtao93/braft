//
// Created by root on 2019/8/22.
//

#include <iostream>
#include <sys/types.h>           // O_CREAT
#include <fcntl.h>               // open
#include <gflags/gflags.h>       // DEFINE_*
#include <butil/sys_byteorder.h> // butil::NetToHost32
#include <brpc/controller.h>     // brpc::Controller
#include <brpc/server.h>         // brpc::Server
#include <braft/raft.h>          // braft::Node braft::StateMachine
#include <braft/storage.h>       // braft::SnapshotWriter
#include <braft/util.h>          // braft::AsyncClosureGuard

#include "raftserver.h"
#include "serimpl.h"
#include "block.pb.h"

DEFINE_bool(check_term, true, "Check if the leader changed to another term");
DEFINE_bool(disable_cli, false, "Don't allow raft_cli access this node");
DEFINE_bool(log_applied_task, false, "Print notice log when a task is applied");
DEFINE_int32(election_timeout_ms, 5000,
             "Start election in such milliseconds if disconnect with the leader");
DEFINE_int32(port, 8200, "Listen port of this peer");
DEFINE_int32(snapshot_interval, 30, "Interval between each snapshot");
DEFINE_string(conf, "", "Initial configuration of the replication group");
DEFINE_string(data_path, "./data", "Path of data stored on");
DEFINE_string(group, "Block", "Id of the replication group");//似乎没用到
DEFINE_string(crash_on_fatal, "true", "Crash on fatal log");//wjt添加

int main(int argc, char *argv[])
{
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    butil::AtExitManager exit_manager;

    //一般只需要一个server
    brpc::Server server;
    example::Block block;
    example::BlockServiceImpl service(&block);

    //add service to RPC server
    if (server.AddService(&service,
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0)
    {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }
    //raft 可以共享一个RPC server。注意第二个参数，因为不允许将service加入到一个
    // 正在运行的server，并且在server starts之前获得server的监听地址是不可能的。所以必须
    // 指定server的address。
    if (braft::add_service(&server, FLAGS_port) != 0)
    {
        LOG(ERROR) << "Fail to add raft service";
        return -1;
    }
    // It's recommended to start the server before Block is started to avoid
    // the case that it becomes the leader while the service is unreacheable by
    // clients.
    // Notice that default options of server are used here. Check out details
    // from the doc of brpc if you would like change some options
    // 要求在block starts之前 start server，以避免他成为leader，然而service
    // 对client来说还是unreachable。
    // 这里server使用默认的options
    if (server.Start(FLAGS_port, NULL) != 0)
    {
        LOG(ERROR) << "Fail to start Server";
        return -1;
    }
    // It's ok to start Block
    if (block.start() != 0)
    {
        LOG(ERROR) << "Fail to start Block";
        return -1;
    }

    LOG(INFO) << "Block service is running on " << server.listen_address();
    // Wait until 'CTRL-C' is pressed. then Stop() and Join() the service
    while (!brpc::IsAskedToQuit())
    {
        sleep(1);
    }
    LOG(INFO) << "Block service is going to quit";

    // Stop block before server
    block.shutdown();
    server.Stop(0);

    // Wait until all the processing tasks are over.
    block.join();
    server.Join();
    return 0;
}