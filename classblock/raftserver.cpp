//
// Created by root on 2019/8/22.
//

#include "raftserver.h"

DECLARE_bool(check_term);
DECLARE_bool(disable_cli);
DECLARE_bool(log_applied_task);
DECLARE_int32(port);
DECLARE_int32(election_timeout_ms);
DECLARE_int32(snapshot_interval);
DECLARE_string(conf);
DECLARE_string(data_path);
DECLARE_string(group);


namespace example
{

Block::Block() : _node(NULL), _leader_term(-1), _fd(NULL){}
Block::~Block()
{
    delete _node;
}



/********************************************************************
*Function:       
*Description:   
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
int Block::start()
{
    if (!butil::CreateDirectory(butil::FilePath(FLAGS_data_path)))
    {
        LOG(ERROR) << "Fail to create directory " << FLAGS_data_path;
        return -1;
    }
    std::string data_path = FLAGS_data_path + "/data";
    int fd = ::open(data_path.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd < 0)
    {
        PLOG(ERROR) << "Fail to open " << data_path;
        return -1;
    }
    _fd = new SharedFD(fd);
    butil::EndPoint addr(butil::my_ip(), FLAGS_port);
    braft::NodeOptions node_options;
    if (node_options.initial_conf.parse_from(FLAGS_conf) != 0)
    {
        LOG(ERROR) << "Fail to parse configuration `" << FLAGS_conf << '\'';
        return -1;
    }
    node_options.election_timeout_ms = FLAGS_election_timeout_ms;
    node_options.fsm = this;
    node_options.node_owns_fsm = false;
    node_options.snapshot_interval_s = FLAGS_snapshot_interval;
    std::string prefix = "local://" + FLAGS_data_path;
    node_options.log_uri = prefix + "/log";
    node_options.raft_meta_uri = prefix + "/raft_meta";
    node_options.snapshot_uri = prefix + "/snapshot";
    node_options.disable_cli = FLAGS_disable_cli;
    braft::Node *node = new braft::Node(FLAGS_group, braft::PeerId(addr));
    if (node->init(node_options) != 0)
    {
        LOG(ERROR) << "Fail to init raft node";
        delete node;
        return -1;
    }
    _node = node;
    return 0;
}
/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::write(const BlockRequest *request,
                  BlockResponse *response,
                  butil::IOBuf *data,
                  google::protobuf::Closure *done)
{
    brpc::ClosureGuard done_guard(done);
    // Serialize request to the replicated write-ahead-log so that all the
    // peers in the group receive this request as well.
    // Notice that _value can't be modified in this routine otherwise it
    // will be inconsistent with others in this group.

    // Serialize request to IOBuf
    const int64_t term = _leader_term.load(butil::memory_order_relaxed);
    if (term < 0)
    {
        return redirect(response);
    }
    butil::IOBuf log;
    const uint32_t meta_size_raw = butil::HostToNet32(request->ByteSize());
    log.append(&meta_size_raw, sizeof(uint32_t));
    butil::IOBufAsZeroCopyOutputStream wrapper(&log);
    if (!request->SerializeToZeroCopyStream(&wrapper))
    {
        LOG(ERROR) << "Fail to serialize request";
        response->set_success(false);
        return;
    }
    log.append(*data);
    // Apply this log as a braft::Task
    braft::Task task;
    task.data = &log;
    // This callback would be iovoked(触发) when the task actually excuted or
    // fail
    task.done = new BlockClosure(this, request, response,
                                 data, done_guard.release());
    if (FLAGS_check_term)
    {
        // ABA problem can be avoid if expected_term is set
        task.expected_term = term;
    }
    // Now the task is applied to the group, waiting for the result.
    return _node->apply(task);
}
/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::read(const BlockRequest *request, BlockResponse *response,
                 butil::IOBuf *buf)
{
    // In consideration of consistency. GetRequest to follower should be
    // rejected.
    if (!is_leader())
    {
        // This node is a follower or it's not up-to-date. Redirect to
        // the leader if possible.
        return redirect(response);
    }

    if (request->offset() < 0)
    {
        response->set_success(false);
        return;
    }

    // This is the leader and is up-to-date. It's safe to respond client
    scoped_fd fd = get_fd();
    butil::IOPortal portal;
    const ssize_t nr = braft::file_pread(
        &portal, fd->fd(), request->offset(), request->size());
    if (nr < 0)
    {
        // Some disk error occurred, shutdown this node and another leader
        // will be elected
        PLOG(ERROR) << "Fail to read from fd=" << fd->fd();
        _node->shutdown(NULL);
        response->set_success(false);
        return;
    }
    buf->swap(portal);
    if (buf->length() < (size_t)request->size())
    {
        buf->resize(request->size());
    }
    response->set_success(true);
}
/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::shutdown()
{
    if (_node)
    {
        _node->shutdown(NULL);
    }
}

/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::join()
{
    if (_node)
    {
        _node->join();
    }
}

/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
bool Block::is_leader() const
{
    return _leader_term.load(butil::memory_order_acquire) > 0;
}

/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::on_apply(braft::Iterator &iter)
{
    // A batch（批） of tasks are committed, which must be processed through
    // |iter|
    for (; iter.valid(); iter.next())
    {
        BlockResponse *response = NULL;
        // This guard helps invoke（调用） iter.done()->Run() asynchronously to
        // avoid that callback blocks the StateMachine
        braft::AsyncClosureGuard closure_guard(iter.done());
        butil::IOBuf data;
        off_t offset = 0;

        //判断是从request中解析还是从log中解析
        if (iter.done())
        {
            // This task is applied by this node, get value from this
            // closure to avoid additional parsing.
            BlockClosure *c = dynamic_cast<BlockClosure *>(iter.done());
            offset = c->request()->offset();
            data.swap(*(c->data()));
            response = c->response();
        }
        else
        {
            // Have to parse BlockRequest from this log.
            uint32_t meta_size = 0;
            butil::IOBuf saved_log = iter.data();
            saved_log.cutn(&meta_size, sizeof(uint32_t));
            // Remember that meta_size is in network order which hould be
            // covert to host order网络序 和主机序列
            meta_size = butil::NetToHost32(meta_size);
            butil::IOBuf meta;
            saved_log.cutn(&meta, meta_size);
            butil::IOBufAsZeroCopyInputStream wrapper(meta);
            BlockRequest request;
            CHECK(request.ParseFromZeroCopyStream(&wrapper));
            data.swap(saved_log);
            offset = request.offset();
        }

        //这是具体操作的实现？？可以在这封装个函数？
        const ssize_t nw = braft::file_pwrite(data, _fd->fd(), offset);
        if (nw < 0)
        {
            PLOG(ERROR) << "Fail to write to fd=" << _fd->fd();
            if (response)
            {
                response->set_success(false);
            }
            // Let raft run this closure.
            closure_guard.release();
            // Some disk error occurred, notify raft and never apply any data
            // ever after
            iter.set_error_and_rollback();
            return;
        }

        if (response)
        {
            response->set_success(true);
        }

        // The purpose of following logs is to help you understand the way
        // this StateMachine works.
        // Remove these logs in performance-sensitive servers.
        LOG_IF(INFO, FLAGS_log_applied_task)
            << "Write " << data.size() << " bytes"
            << " from offset=" << offset
            << " at log_index=" << iter.index();
    }
}
/********************************************************************
*Function:       
*Description:    
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
int Block::link_overwrite(const char *old_path, const char *new_path)
{
    if (::unlink(new_path) < 0 && errno != ENOENT)
    {
        PLOG(ERROR) << "Fail to unlink " << new_path;
        return -1;
    }
    return ::link(old_path, new_path);
}
/********************************************************************
*Function:       
*Description:    
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void *Block::save_snapshot(void *arg)
{
    SnapshotArg *sa = (SnapshotArg *)arg;
    std::unique_ptr<SnapshotArg> arg_guard(sa);
    // Serialize StateMachine to the snapshot
    brpc::ClosureGuard done_guard(sa->done);
    std::string snapshot_path = sa->writer->get_path() + "/data";
    // Sync buffered data before
    int rc = 0;
    LOG(INFO) << "Saving snapshot to " << snapshot_path;
    for (; (rc = ::fdatasync(sa->fd->fd())) < 0 && errno == EINTR;)
    {
    }
    if (rc < 0)
    {
        sa->done->status().set_error(EIO, "Fail to sync fd=%d : %m",
                                     sa->fd->fd());
        return NULL;
    }
    std::string data_path = FLAGS_data_path + "/data";
    if (link_overwrite(data_path.c_str(), snapshot_path.c_str()) != 0)
    {
        sa->done->status().set_error(EIO, "Fail to link data : %m");
        return NULL;
    }

    // Snapshot is a set of files in raft. Add the only file into the
    // writer here.
    if (sa->writer->add_file("data") != 0)
    {
        sa->done->status().set_error(EIO, "Fail to add file to writer");
        return NULL;
    }
    return NULL;
}
/********************************************************************
*Function:       
*Description:    
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
void Block::on_snapshot_save(braft::SnapshotWriter *writer, braft::Closure *done)
{
    // Save current StateMachine in memory and starts a new bthread to avoid
    // blocking StateMachine since it's a bit slow to write data to disk
    // file.
    SnapshotArg *arg = new SnapshotArg;
    arg->fd = _fd;
    arg->writer = writer;
    arg->done = done;
    bthread_t tid;
    bthread_start_urgent(&tid, NULL, save_snapshot, arg);
}
/********************************************************************
*Function:       
*Description:    接收rpc,解析完req之后调用addtable
*Calls:          
*Table Accessed: 
*Table Updated: 
*Input:                 
*Output:         
*Return:         
*Others:        
*********************************************************************/
int Block::on_snapshot_load(braft::SnapshotReader *reader)
{
    // Load snasphot from reader, replacing the running StateMachine
    CHECK(!is_leader()) << "Leader is not supposed to load snapshot";
    if (reader->get_file_meta("data", NULL) != 0)
    {
        LOG(ERROR) << "Fail to find `data' on " << reader->get_path();
        return -1;
    }
    // reset fd
    _fd = NULL;
    std::string snapshot_path = reader->get_path() + "/data";
    std::string data_path = FLAGS_data_path + "/data";
    if (link_overwrite(snapshot_path.c_str(), data_path.c_str()) != 0)
    {
        PLOG(ERROR) << "Fail to link data";
        return -1;
    }
    // Reopen this file
    int fd = ::open(data_path.c_str(), O_RDWR, 0644);
    if (fd < 0)
    {
        PLOG(ERROR) << "Fail to open " << data_path;
        return -1;
    }
    _fd = new SharedFD(fd);
    return 0;
}

void Block::on_leader_start(int64_t term)
{
    _leader_term.store(term, butil::memory_order_release);
    LOG(INFO) << "Node becomes leader";
}
void Block::on_leader_stop(const butil::Status &status)
{
    _leader_term.store(-1, butil::memory_order_release);
    LOG(INFO) << "Node stepped down : " << status;
}
void Block::on_shutdown()
{
    LOG(INFO) << "This node is down";
}
void Block::on_error(const ::braft::Error &e)
{
    LOG(ERROR) << "Met raft error " << e;
}
void Block::on_configuration_committed(const ::braft::Configuration &conf)
{
    LOG(INFO) << "Configuration of this group is " << conf;
}
void Block::on_start_following(const ::braft::LeaderChangeContext &ctx)
{
    LOG(INFO) << "Node start following " << ctx;
}

void Block::redirect(BlockResponse *response)
{
    response->set_success(false);
    if (_node)
    {
        braft::PeerId leader = _node->leader_id();
        if (!leader.is_empty())
        {
            response->set_redirect(leader.to_string());
        }
    }
}

void BlockClosure::Run()
{
    // Auto delete this after Run()
    std::unique_ptr<BlockClosure> self_guard(this);
    // Repsond this RPC.
    brpc::ClosureGuard done_guard(_done);
    if (status().ok())
    {
        return;
    }
    // Try redirect if this request failed.
    _block->redirect(_response);
}
} // namespace example
