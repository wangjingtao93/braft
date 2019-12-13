//
// Created by root on 2019/8/22.
//

#ifndef RAPTOR_BLOCK_H
#define RAPTOR_BLOCK_H

#include <sys/types.h>           // O_CREAT
#include <fcntl.h>               // open
#include <gflags/gflags.h>       // DEFINE_*
#include <butil/sys_byteorder.h> // butil::NetToHost32
#include <brpc/controller.h>     // brpc::Controller
#include <brpc/server.h>         // brpc::Server
#include <braft/raft.h>          // braft::Node braft::StateMachine
#include <braft/storage.h>       // braft::SnapshotWriter
#include <braft/util.h>          // braft::AsyncClosureGuard
#include "block.pb.h"            // BlockService


namespace example
{

class BlockClosure;
// Implementation of example::Block as a braft::StateMachine.
class Block : public braft::StateMachine
{
public:
    Block();
    virtual ~Block();

    // Starts this node
    int start();
    //implements the methods
    void write(const BlockRequest *request, BlockResponse *response, butil::IOBuf *data, google::protobuf::Closure *done);
    void read(const BlockRequest *request, BlockResponse *response, butil::IOBuf *buf);

    // Shut this node down.
    void shutdown();

    // Blocking this thread until the node is eventually down.
    void join();

    bool is_leader() const;

    //不知道这是啥玩意儿
private:
    class SharedFD : public butil::RefCountedThreadSafe<SharedFD>
    {
    public:
        explicit SharedFD(int fd) : _fd(fd) {}
        int fd() const { return _fd; }

    private:
        friend class butil::RefCountedThreadSafe<SharedFD>;
        ~SharedFD()
        {
            if (_fd >= 0)
            {
                while (true)
                {
                    const int rc = ::close(_fd);
                    if (rc == 0 || errno != EINTR)
                    {
                        break;
                    }
                }
                _fd = -1;
            }
        }

        int _fd;
    };

    typedef scoped_refptr<SharedFD> scoped_fd;

    scoped_fd get_fd() const
    {
        BAIDU_SCOPED_LOCK(_fd_mutex);
        return _fd;
    }

    friend class BlockClosure;
    void redirect(BlockResponse *response);

    // @braft::StateMachine
    // 这个是必须要实现的
    // 会在一条或者多条日志被多数节点持久化之后调用。通知用户将这些日志所表示的操作应用到业务状态机中
    // 通过iter，可以从遍历所有未处理但是已经提交的日志，如果你的状态机支持批量更新，可以一次性获取
    // 多条日志,提高状态机的吞吐
    void on_apply(braft::Iterator &iter);

    static int link_overwrite(const char *old_path, const char *new_path);
    static void *save_snapshot(void *arg);
    void on_snapshot_save(braft::SnapshotWriter *writer, braft::Closure *done);
    int on_snapshot_load(braft::SnapshotReader *reader);

    void on_leader_start(int64_t term);
    void on_leader_stop(const butil::Status &status);
    void on_shutdown();
    void on_error(const ::braft::Error &e);
    void on_configuration_committed(const ::braft::Configuration &conf);
    void on_start_following(const ::braft::LeaderChangeContext &ctx);

    struct SnapshotArg
    {
        scoped_fd fd;
        braft::SnapshotWriter *writer;
        braft::Closure *done;
    };

private:
    mutable butil::Mutex _fd_mutex;
    braft::Node *volatile _node;
    butil::atomic<int64_t> _leader_term;
    scoped_fd _fd;
};
// end of @braft::StateMachine

// Implements Closure（关闭，使终止） which enclosed（围绕，装入，放入封套） RPC stuff（东西，塞满，填塞）
// 实现包含RPC内容的关闭
class BlockClosure : public braft::Closure
{
public:
    BlockClosure(Block *block,
                 const BlockRequest *request,
                 BlockResponse *response,
                 butil::IOBuf *data,
                 google::protobuf::Closure *done)
        : _block(block), _request(request), _response(response), _data(data), _done(done) {}
    ~BlockClosure() {}

    const BlockRequest *request() const { return _request; }
    BlockResponse *response() const { return _response; }
    void Run();
    butil::IOBuf *data() const { return _data; }

private:
    // Disable explicitly delete
    Block *_block;
    const BlockRequest *_request;
    BlockResponse *_response;
    butil::IOBuf *_data;
    google::protobuf::Closure *_done;
};
} // namespace example
#endif //RAPTOR_SERVER_H
