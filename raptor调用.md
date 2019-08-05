
# 1. 拗口的话

brpc 一个端口注册多个逻辑Service


Service运行在brpc Server里面

将braft相关的Service加入到你的Server当照片那个

Service接口和Server接口要一致

server是服务器，service是服务


# 2. 几种参数
bloock_device: 即BD的ip,str型
BG_name:
BG_conf:所在BD的ip+port，list型（因为三副本，BG1' 、BG1''、BG'''可能分布在不同的BD上）
table_name:BG_Stat,BG_Conf等七种
BG_to_remove和BG_to_add

**在braft里**：
group_id:有时候和BG_name相同
peers:和Block_device类似,ip:port:0
leader:ip:port:0


# 3. 疑问

task ownership是什么玩意，什么作用



从哪里入手从哪里入手


直接调试mov table

# 4. braft chanage_peers函数调用过程
## 4.1. 方法入口

解析request
1. 获取对象指针scoped_refptr<NodeImpl> node;这个类（在node.h里）的方法：remove_peers add_peers,list peers等等
2. 判断节点状态，即BG的状态。
   ```
   butil::Status CliServiceImpl::get_node(scoped_refptr<NodeImpl>* node,
                                      const GroupId& group_id,
                                      const std::string& peer_id) ;
   入参：对象指针node
   grop_id：测试时是用的BG_name
   peer_id: 即new_peers,即BG_to_add,ip+port+0
   ```
3. list_peers,暂时推测是根据configuration文件得到,测试时结果只有一个old_peers:127.0.0.1:10001,这个是add_to_BG的ip+port
4. 溜出实例Configuration conf，将new_peers插入到一个set容器即_peer里
5. 调用方法node->change_peers(conf, change_peers_done)，传入俩对象
```c++
/***************************************************************************
 * server端的chanage接口
  message ChangePeersRequest {
    required string group_id = 1;
    required string leader_id = 2;
    repeated string new_peers = 3;
****************************************************************************/
void CliServiceImpl::change_peers(::google::protobuf::RpcController* controller,
                                  const ::braft::ChangePeersRequest* request,
                                  ::braft::ChangePeersResponse* response,
                                  ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;

    //判断node状态
    butil::Status st = get_node(&node, request->group_id(), request->leader_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }

    //整出来old_peers，干什么用涅
    std::vector<PeerId> old_peers;
    st = node->list_peers(&old_peers)//测试时。就得到一个old_peers:127.0.0.1:10001,这个是add_to_BG的ip+port。暂时推测是根据configuration文件得到
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }

    //一组peers时
    Configuration conf;//Configuration这个类有些方法比如add_peer change_peer等
    for (int i = 0; i < request->new_peers_size(); ++i) {
        PeerId peer;
        if (peer.parse(request->new_peers(i)) != 0) {
            cntl->SetFailed(EINVAL, "Fail to parse %s",
                                    request->new_peers(i).c_str());
            return;
        }
        conf.add_peer(peer);// Add a peer，像是要忘一个内存中的set容器即_peer中插入这条ip记录，至于什么作用，不清楚
    }

    //什么作用涅？ 释放内存？ 异步？
    Closure* change_peers_done = NewCallback(
            change_peers_returned, 
            cntl, request, response, old_peers, conf, node,
            done_guard.release());
    
    //configuration chanage，入参：实例conf，指针change_peers_done，这俩入参牛逼
    return node->change_peers(conf, change_peers_done);
}
```

### 4.1.1. 获取node状态函数

不细看，Return OK或者ERROR
1. 实例化一个类： NodeManager* const nm = NodeManager::GetInstance()，这个类有很多方法，可以获取node信息等等
2. 判断peer_ID（BD）是否存在，如存在，进入3，不存在则进入4
3. 判断此BG是不是在这个BD上，即：group_id是否在这个peer_id上。成功则进入5，失败则直接return
4. peer_ID不存在，return
5. 判断此cliserver能否访问此node，可以访问则return OK，否则返回EACCES

```C++
butil::Status CliServiceImpl::get_node(scoped_refptr<NodeImpl>* node,
                                      const GroupId& group_id,
                                      const std::string& peer_id) {
    NodeManager* const nm = NodeManager::GetInstance();//这个类可以做很多事，获取node信息等等

    //一些前置判断，比如peer_id
    if (!peer_id.empty()) {
        *node = nm->get(group_id, peer_id);//判断此BG是不是在这个BD上，即：group_id是否在这个peer_id上
        if (!(*node)) {
            return butil::Status(ENOENT, "Fail to find node %s in group %s",
                                         peer_id.c_str(),
                                         group_id.c_str());
        }
    } else {
        std::vector<scoped_refptr<NodeImpl> > nodes;
        nm->get_nodes_by_group_id(group_id, &nodes);
        if (nodes.empty()) {
            return butil::Status(ENOENT, "Fail to find node in group %s",
                                         group_id.c_str());
        }
        if (nodes.size() > 1) {
            return butil::Status(EINVAL, "peer must be specified "
                                        "since there're %lu nodes in group %s",
                                         nodes.size(), group_id.c_str());
        }
        *node = nodes.front();
    }

    if ((*node)->disable_cli()) {
        return butil::Status(EACCES, "CliService is not allowed to access node "
                                    "%s", (*node)->node_id().to_string().c_str());
    }

    return butil::Status::OK();
}

```
### 4.1.2. Configuration::conf.add_peer

```C++
 // Add a peer.
    // Returns true if the peer is newly added.
    bool add_peer(const PeerId& peer) {
        return _peers.insert(peer).second;//像是往内存中的一个set容器中插入，至于这个_peer()容器中本身存储着什么、有什么作用，实在是不知道
    }

private:
    std::set<PeerId> _peers;

```


## 4.2. node->change_peers

1. 上把锁，调用 unsafe_register_conf_change执行configuration change，/两个入参的值是一样的，都是add_to_BGd的ip+port
2. 判断此bg是不是leader，不是的话进入步骤3，如果是leader，进入4
3. 写告警日志：此BG拒绝配置变化，因为the stat is leader。然后关闭线程之类，在return
4. check concurrent(并发) conf change，可能是如果资源被占用则拒绝修改，然后关闭线程之类，return，否则进入5
5. 判断news_peers是否new peers equals to 当前配置，是则立刻return，否则进入6
6. 
```C++
// @Node configuration change
void NodeImpl::change_peers(const Configuration& new_peers, Closure* done) {
    BAIDU_SCOPED_LOCK(_mutex);//加把锁？
    return unsafe_register_conf_change(_conf.conf, new_peers, done);//两个入参的值是一样的，都是add_to_BGd的ip+port
}

void NodeImpl::unsafe_register_conf_change(const Configuration& old_conf,
                                           const Configuration& new_conf,
                                           Closure* done) {
    //此BG不是leader的话Waring拒绝配置变化，关闭线程之类
    if (_state != STATE_LEADER) {
        LOG(WARNING) << "[" << node_id()//node_id即BG_name
                     << "] Refusing configuration changing because the state is "
                     << state2str(_state) ;
        if (done) {
            if (_state == STATE_TRANSFERRING) {
                done->status().set_error(EBUSY, "Is transferring leadership");
            } else {
                done->status().set_error(EPERM, "Not leader");
            }
            run_closure_in_bthread(done);
        }
        return;
    }

    // check concurrent(并发) conf change，可能是如果资源被占用则拒绝修改，然后关闭线程之类，return
    if (_conf_ctx.is_busy()) {
        //测试时并未进入
        LOG(WARNING) << "[" << node_id()
                     << " ] Refusing concurrent configuration changing";
        if (done) {
            done->status().set_error(EBUSY, "Doing another configuration change");
            run_closure_in_bthread(done);//干吗呢要
        }
        return;
    }

    //立刻return，当new peers equals to 当前配置
    // Return immediately when the new peers equals to current configuration
    if (_conf.conf.equals(new_conf)) {
        run_closure_in_bthread(done);
        return;
    }
    
    //这个竟然进不去
    return _conf_ctx.start(old_conf, new_conf, done);
}
```

### 4.2.1. Configuration::conf.equals
1. 先判断size是否ok
2. 遍历，看能不能找到这个peer
```C++
    bool equals(const Configuration& rhs) const {
        if (size() != rhs.size()) {
            return false;
        }
        // The cost of the following routine is O(nlogn), which is not the best
        // approach.
        // 下面这个方法代价大
        for (const_iterator iter = begin(); iter != end(); ++iter) {
            if (!rhs.contains(*iter)) {
                return false;
            }
        }
        return true;
    }
```

# 5. Symphony
## 5.1. 外围接口 
简单来讲包括：
1. 判断JOBLIST文件是否被其他线程使用
2. 打开JOBLIST文件，readlines文件内容.
3. pool出线程，<font color='red'>BG_utils.do_tasks（task_group）</font>,处理文件内容
4. 将没处理的task再写回JOBLIST
```python
def do_move_tasks():
    if lock_utils.is_file_occupied(JOB_LIST_FILE):
        print '{JOB_LIST_FILE} is used by other process.'.format(JOB_LIST_FILE=JOB_LIST_FILE)
        return

    if not os.path.exists(JOB_LIST_FILE):
        return

    with lock_utils.get_file_lock(JOB_LIST_FILE):
        with open(JOB_LIST_FILE, 'r') as f:
            tasks = f.readlines()#readlines返回的list，每个元素是文件的每一行

        p = Pool()
        unfinished_tasks = []
        res = []
        for i in range(0, len(tasks), 100):
            task_group = tasks[i: i+100]#切片
            res.append(p.apply_async(BG_utils.do_tasks, args=(task_group,)))#进程池，子进程，入参task_group
        p.close()
        p.join()

        #从返回值res判断迁移任务是否完成
        for i in res:
            unfinished_tasks.extend(i.get())#extend，往list追加list
        logger.info('Finish to do move tasks.')

        with open(JOB_LIST_FILE, 'w') as f:
            f.writelines(unfinished_tasks)#未完成的再写回去
        logger.info('Finish to update JOB_LIST.')
```
## 5.2. 第二步
**BG_utils.do_tasks执行**：
1. 根据逗号分隔将task信息取出来，如group_id, peer_tp_remove(ip+port)等
2. 判断job类型：move_BG还是move_table,根据类型调用函数，重点分析do_BG_task
3. 需要将正在doing和其他类型的job放入unfinished_tasks = []并返回
```python
def do_tasks(tasks):

    unfinished_tasks = []

    for task in tasks:
        job, group_id, peer_to_remove, peer_to_add, ts = task.strip().split(',')#根据逗号分割出来

        #判断job类型
        if job not in ('move_BG', 'move_table'):
            unfinished_tasks.append(task)
            continue

        if job == 'move_BG':
            res = do_BG_task(group_id, peer_to_remove, peer_to_add)#重点分析
        else:
            res = do_table_task(group_id, peer_to_remove, peer_to_add)

        if res == DOING:
            unfinished_tasks.append(task)

    return unfinished_tasks#需要将没有完成的unfinished_task返回，写回JOBLIST中
```

## 5.3. 第三步
BG_utils.do_BG_task()，需要干得或有两个，将BG move到BD_to_add，同时清理BD_to_remove的该BG，执行如下：
1. 一些前置判断，比如判断两个BD_to_add和BD_to_remove是否是同一个，是否为空。判断BG是否在BD_to_remove上，判断BG是否在BD_to_add
2. 判断通过后才能调用 block_client.move
3. move完成后需要调用 _remove_BGs(BD_to_remove, [BG]) 从remove_from的BD删除该BG（底层调用的函数就是remove_BG，暂时先不研究）

关于这些前置判断，可以这么理解：
pass

```python
def do_BG_task(BG, BD_to_remove, BD_to_add):
    '''
    do move BG tasks and clean removed_BG task
    包括move BG 和 clean remove_BG
    '''

    #判断BG 需要add to的BD和remove from的BD是否是相同的
    if BD_to_add == BD_to_remove:
        BD_to_remove = ''#ip+port置空，有什么意义呢？，为雷下一步可以return finish

    #都是空的花，直接return finish
    if BD_to_add == '' and BD_to_remove == '':
        return FINISH

    need_change_peer = False #加flag
    #要判断remove from的BD是否存在，并且是否存在这个BG，存在的flag要置true
    if BD_to_remove != '':
        res = block_client.get_raft_node_state(BD_to_remove)# get node即当前这个BD状态。（通过发rpc）
        if res is not None:
            raft_nodes, _, _ = _get_BD_raft_node(res.raft_node_infos)
            if BG in raft_nodes:
                #测试时为什么没进入此分支
                need_change_peer = True

    #判断需要add_to的BD是否存在。不存在直接return CANCEL，存在则...
    if BD_to_add != '':
        res = block_client.get_raft_node_state(BD_to_add)
        if res is None:
            return CANCEL

        #需要add_to的BD存在
        node_infos, _, _ = _get_BD_raft_node(res.raft_node_infos)
        if BG in node_infos:
            #若BG已经在该BD上了，该逻辑暂不细看
            info = node_infos[BG]
            logger.info('raft node stat in added BD: {info}'.format(info=info))
            if info.snapshot_status in ('LOADING', 'DOWNLOADING'):
                return DOING
            elif not need_change_peer:
                logger.info('Successfully move BG: {BG} from {BD_to_remove} to {BD_to_add}.'.format(
                    BG=BG, BD_to_remove=BD_to_remove, BD_to_add=BD_to_add))
                _remove_BGs(BD_to_remove, [BG])
                return FINISH

    try:
        success = block_client.move(BG, BD_to_remove, BD_to_add)#重点关注
    except client_utils.ClientTimeoutError:
        #超时，doing
        logger.warning('Timeout when move BG: {BG} from {BD_to_remove} to {BD_to_add}.'.format(
            BG=BG, BD_to_remove=BD_to_remove, BD_to_add=BD_to_add))
        return DOING

    #move成功了之后，需要打日志，且需要从remove_from的BD删除该BG
    if success:
        logger.info('Successfully move BG: {BG} from {BD_to_remove} to {BD_to_add}.'.format(
            BG=BG, BD_to_remove=BD_to_remove, BD_to_add=BD_to_add))

        _remove_BGs(BD_to_remove, [BG])#需要从remove_from的BD删除该BG
        return FINISH

    logger.error('Failed to move BG: {BG} from {BD_to_remove} to {BD_to_add}.'.format(
        BG=BG, BD_to_remove=BD_to_remove, BD_to_add=BD_to_add))
    return CANCEL

```


## 5.4. 第四步
 block_client.move得调用
 1. 格式化old_BD new_BD等格式，如u型变str型等，根据BG_Name获得所在的old_BDs,也需要获得new_BDs
 2. 调 create_one_block_group完成BG在新的BD上的创建
 3. 调用client_utils.change_block_group_conf，修改BG_conf
```python
def move(BG_name, BD_to_remove, BD_to_add):
    
    old_BDs = client_utils.get_block_group_conf(BG_name)#和BD_to_remove一样
    new_BDs = client_utils.concat_new_peers(old_BDs, BD_to_remove, BD_to_add)/#和BD_to_add相同
    if BD_to_add != '':
        BD_to_add = client_utils.get_peer_removed_index(BD_to_add)#u型变成str型，ip+port
        success = create_one_block_group(BD_to_add, BG_name, new_BDs)#这个地方也发了rpc,在新的BDcreate BG
        if not success:
            return False

    return client_utils.change_block_group_conf(BG_name, new_BDs)#改BG_conf，发rpc

```

## 5.5. 第五步

发rpc，在新的BD上create BG
1. 其实BG_creat是分两种：批量（batch）creat，one_creat,此处调用的是one_creat
2. 先获取BD，只有ip
3. 

```python
def create_one_block_group(block_device, BG_name, BG_conf):
    '''
    创建一个BG
    :param block_device: BD的ip+port如：127.0.0.1+port，str型
    :param BG_name: 00000000000000000A
    :param BG_conf: BG所在的BD ip+port,（会有三个BD的ip+port，因为是三个BG副本）
    :return: create_BGs_on_BD
    '''
    return create_BGs_on_BD(block_device, [BG_name, ], {BG_name: BG_conf}) #在次调用创建BG on BD

def create_BGs_on_BD(BD, BGs, BG_confs):

    BD = client_utils.get_peer_removed_index(BD)#只有ip，str型

    logger.debug('Start to create BGs on BD {BD}: {BGs}'.format(BD=BD, BGs=BGs))
    res = True
    batch_size = 100

    try:
        for i in range(0, len(BGs), batch_size):
            with grpc.insecure_channel(BD) as stub:
                stub = block_device_pb2_grpc.BlockDeviceServiceStub(stub)
                BG_batch = BGs[i: i+batch_size]

                while True:
                    if len(BG_batch) == 0:
                        break

                    req = block_device_pb2.CreateBGBatchRequest()
                    for BG in BG_batch:
                        element = req.batch_request.add()
                        element.bg_name = BG
                        element.raft_conf = client_utils.add_index(BG_confs[BG])

                    #response几种情况判断：1.BG在此BD上已经存在 2.创建成功 3.创建失败
                    resp = stub.CreateBGBatch(req)#发rpc
                    if resp.status == 0:
                        logger.debug('Create BG sucessfully. BD: {BD}, BGs {BGs}'.format(
                            BD=BD, BGs=req))
                        break
                    elif resp.status == 40019:
                        logger.info('Create BGs on BD {BD}, these BGs already exists: {BGs}'.format(
                            BD=BD, BGs=resp.failed_bg_name))
                        BG_batch = set(BGs[i: i+batch_size]) - set(resp.failed_bg_name)
                    else:
                        logger.error('Create BGs error on BD {BD} from BG {BG}, error: {error}, and these BGs are not created: {BGs}'.format(
                            BD=BD, BG=resp.failed_bg_name[0], error=resp.status, BGs=resp.failed_bg_name))
                        return False

    except grpc._channel._Rendezvous as e:
        #rpc执行失败
        res = False
        logger.error('Error accur while Create BGs on BD {BD}. {code}: {details}'.format(
            BD=BD, code=e.code(), details=e.details()))

    #返回rpc发送是否成功
    return res
```

proto协议中定义的Creat_BG_bath的request和reponse格式
```
#request
message CreateBGBatchRequest {
  repeated CreateBGRequest batch_request = 1;

message CreateBGRequest {
	required string bg_name = 1;
	required string raft_conf = 2;
}

#reponse
message CreateBGBatchResponse {
  required int32 status = 1;
  repeated string failed_bg_name = 2;
}

message CreateBGResponse {
	required int32 status = 1;
}

}
```

## 5.6. 第六步
在第四步的最后调用client_utils.change_block_group_conf，修改BG_conf
1. 先根据BG_name找到BG的leader,ip+port
2. 调_change_peers个braft发change_peer_request 
```python
def change_block_group_conf(BG_name, new_peers):
    leader = get_block_group_leader(BG_name)
    return _change_peers(BG_name, leader, new_peers)

def _change_peers(group_id, leader, new_peers, timeout=1):

    new_peers = add_index(new_peers)#好像是加了个:0
    leader = get_peer_removed_index(leader)

    #发rpc给braft
    try:
        with grpc.insecure_channel(leader) as channel:
            stub = cli_pb2_grpc.CliServiceStub(channel)
            req = cli_pb2.ChangePeersRequest()
            req.group_id = group_id
            req.leader_id = add_index(leader)#加个:0
            req.new_peers.extend(split_raft_conf(new_peers))

            resp = stub.change_peers(req, timeout=timeout)
            logger.debug('Group {group_id} change peers from {old_peers} to {new_peers}'.format(
                group_id=group_id, old_peers=resp.old_peers, new_peers=resp.new_peers))

            return True
    except grpc._channel._Rendezvous as e:
        if e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
            raise ClientTimeoutError(group_id)

        logger.error('Error accur while change peers on group {group_id}, new_peers: {new_peers}. {code}: {details}'.format(
            group_id=group_id, new_peers=new_peers, code=e.code(), details=e.details()))
        return False
```

发给braft的change_peer的request和reponse格式
```
message ChangePeersRequest {
    required string group_id = 1;
    required string leader_id = 2;
    repeated string new_peers = 3;
}

message ChangePeersResponse {
    repeated string old_peers = 1;
    repeated string new_peers = 2;
}
```

## 5.7. 其他

rpc发的req得到的reponse的内容。get_raft_node_stat_reposed,内容有group_id, peer_id, raft_conf, stat等等
```C
message GetRaftNodeStateResponse {
  required int32 status = 1;
  repeated RaftNodeInfo raft_node_infos = 2;
}

message RaftNodeInfo {
  required string   group_id = 1;
  required string   peer_id = 2;
  required string   raft_conf = 3;
  required string   state = 4;
  required int64    term  = 5;
  required string   leader = 6;
  required string   fsm_state = 7;
  required int64    disk_id = 8;
  required int64    known_applied_id = 9;
  required int64    last_committed_id = 11;
  required int64    last_snapshot_id = 12;
  required int64    last_snapshot_term = 13;
  required string   snapshot_status = 14;
  optional string   raw_raft_stat = 15;
}

RaftNodeInfo* A 

GetRaftNodeStateResponse b
A = b.add_A();

b.A_sinze()
RaftNodeInfo c = B.a()
```