# braft学习笔记

## 1. 概述

[入门理解](http://thesecretlivesofdata.com/raft/)

解决分布式一致性问题（distributed consensus）

C++，基于brpc的[RAFT 一致算法和 replicated state machine。应用于分布式系统

高可用：
- 存储系统： key-vlue,对象、文件
- SQL数据库、
- 元数据sevice:


<font color='red'>RAFT节点状态</font>：
- <font color='red'>Leader</font>：接收Client的请求，并进行复制，任何时刻只有一个Leader
- <font color='red'>Follower</font>：被动接收各种RPC请求
- <font color='red'>Candidate</font>：用于选举出一个新的Leader

Follower--->Candidate::: Follower长时间没收到心跳

Follower--->Leader :::Follower收到多数**投票应答**

Leader、Candidate--->Follower::: Leader、Candidatek接收到更高版本的消息

过程：leader election， Log Replication

## 2. compile and install
1. 装brpc
   - 安装通用开源库
   ```
   yum install -y glog-devel gflags-devel protobuf-devel protobuf-compiler leveldb-devel gperftools-devel gtest-devel snappy-devel openssl-devel libaio-devel libstdc++-devel
   ```
2. cmake,make

## 3. raft

raft作为复制状态机，是分布式系统中<font color ='red'>最核心最基础</font>的组件，<font color ='red'>提供命令</font>在多个节点<font color ='red'>有序复制和执行</font>

## 4. braft功能
braft并不能解决available问题
保证一致性和分区容忍性

基本功能：
- Leader Election
- Log Replication
- Membership Change
- Log Compaction
- Replication and recovery.
- Snapshot and log compaction.
- Membership management.
- Fully concurrent replication.
- Fault tolerance.
- Asymmetric network partition tolerance.
- Workaround when quorate peers are dead.

通过RAFT提供的一致性状态机，可以解决复制、修复、节点管理等问题
大的简化当前分布式系统的设计与实现，让开发者只关注于业务逻辑，将其抽象实现成对应的状态机即可。基于这套框架，可以构建很多分布式应用：

- 分布式锁服务，比如Zookeeper
- 分布式存储系统，比如分布式消息队列、分布式块系统、分布式文件系统、分布式表格系统等
- 高可靠元信息管理，比如各类Master模块的HA

## 5. 分布式一致性复制协议

Paxos Zab Viewstamped Replication

Paxos是根本

Raft最好，简单易用

|                       | **Multi-Paxos** | **RAFT** | **Zab** | **Viewstamped Replication** |
| --------------------- | --------------- | -------- | ------- | --------------------------- |
| Leader Election       | Yes             | Yes      | Yes     | Yes                         |
| Log Replication       | Yes             | Yes      | Yes     | Yes                         |
| Log Recovery          | Yes?            | Yes      | Yes     | Yes                         |
| Log Compaction        | Yes?            | Yes      | Yes     | Yes                         |
| Membership Management | Yes?            | Yes      | No      | Yes                         |
| Understandable        | Hard            | Easy     | Medium  | Medium                      |
| Protocol Details      | No              | Yes      | Yes     | Yes                         |
| Implements            | Few             | Mass     | Many    | Few                         |


<font color='red'>RAFT节点状态</font>：
- <font color='red'>Leader</font>：接收Client的请求，并进行复制，任何时刻只有一个Leader
- <font color='red'>Follower</font>：被动接收各种RPC请求
- <font color='red'>Candidate</font>：用于选举出一个新的Leader

Follower--->Candidate::: Follower长时间没收到心跳

Follower--->Leader :::Follower收到多数**投票应答**

Leader、Candidate--->Follower::: Leader、Candidatek接收到更高版本的消息


## 6. Leader Election

RAFT将时间划分到Term,用于选举

选举过程中，每个Candidate节点先将本地的Current Term加一，然后向其他节点发送RequestVote请求。
具体的判断规则：


## 7. Symmetric network partitioning

对称网络划分：可能是因为节点故障导致。

新的节点上线，属于PeerSet中的节点可以加入，不属于PeerSet中的节点，leader会永远忽略。

属于PeerSet中的新节点上线，会导致leader降级（stepDown）。会打断lease(租约)，导致复制组不可用

## 8. Asymmetric network partitioning

原来的RAFT论文中对非对称的网络划分做的不好

## 9. StepDown

原始的RAFT协议是Leader收到任何term高于Current的请求都会进行stepDown,实际过程中应该在一下几个时刻进行StepDown:

## 10. Log Replication

leader负责接收客户端的请求，每个请求包含一个<font color = 'red'>要被复制状态机执行</font>的<font color = 'red'>指令</font>。

*新的entry*:leader将这个<u>指令（command）追加到log</u>形成一个新的entry。然后通过AppendEntries RPCs将entry<u>分发给其他的server</u>

其他sever如果没发现entr有问题，复制成功后会给leader一个<font color = 'red'>表示成功的ACK</font>。leade收到大多数的ACK后应用该日志（Log Entry）。

Log Entry的内容：Command 其他server从leader收到该entry的term号(判断一些log之间不一致的状态) index(索引，指明Entry在log中的位置)

<font color = 'red'>leader需要决定什么时候将日志（log Entry）应用给状态机是安全的</font>

<font color = 'red'>committed</font>:可以被状态机应用的Entry叫committed。RAFT<font color = 'red'>保证committed entries持久化</font>，<u>并且最终被其他状态机应用</u>

*节点重启过程*：先加载上一个Snapshot，再加入RAFT复制组，选主或者是接收更新。。因为Snapshot中的数据一定是Applied，那么肯定是Committed的，加载是安全的。但是Log中的数据，不一定是Committed的，因为我们没有持久化CommittedIndex，所以不确定Log是否是Committed，不能进行加载。样先加载Snapshot虽然延迟了新节点加入集群的时间，但是<u>能够保证一旦一个节点变为Leader之后能够比较快的加载完全数据</u>，并提供服务。同理，Follower接收到InstallSnapshot之后，接收并加载完Snapshot之后再回复Leader。

### 关于Replication

概述:
对上面几种复制模型做一下简单对比，可以根据业务模型做出对应的选择。

- 分发复制只有在单写者的情况下比较有优势，使用范围较为有限。
- 链式复制有较高的吞吐，但延迟较高且无法规避慢节点。
- 树形复制，是吞吐和延迟的一个比较好的折中。

|      | 吞吐         | 延迟   | 慢节点       | 多写   | 实时读取                    |
| ---- | ---------- | ---- | --------- | ---- | ----------------------- |
| 链式复制 | 1 * 网卡带宽   | 高    | 全部节点      | 支持   | Head节点或Tail节点           |
| 树形复制 | 1/2 * 网卡带宽 | 中    | Primary节点 | 支持   | Primary节点               |
| 分发复制 | 1/3 * 网卡带宽 | 低    | 无         | 不支持  | 读取全部节点选择多数，或由Writer节点决定 |
#### 链式复制
使用最广泛。

将数据复制到全部节点之后，在想client应答成功
高吞吐，高延时
#### 树形复制
client ---> primary --->两个Secondary。

只有Primary节点IO慢才会导致复制卡顿

#### 分发式复制
<u>分发复制中节点都是对等的</u>，Client直接向各个节点直接进行分发写入，节点之间并不进行通信复制，只要写入多数节点成功，就判为写入成功。

延时低
- 不能同时有两个Writer，避免多个写入同时发生，保证数据的一致性
- 另外一个问题就是<u>如何读到最新的数据</u>。因为节点都是对等的，且没有互相通信。Reader只有读取全部节点，根据节点返回的version或者是index之类的才能判断出哪些节点返回的数据是有效的，<u>效率较低</u>
## 11. Log Recovery

*目的*：保证已经committed的数据不会丢失,未Committed的数据转变为Committed，但不会因为修复过程中断又重启而影响节点之间一致性。

分为current Term修复和prev Term修复

*current Term修复*：主要是解决某些Follower节点重启加入集群，或者是新增Follower节点加入集群，

*prev Term*：主要是在保证Leader切换前后数据的一致性


## 12. Log Compaction

*更新*：Leader写入Log，复制到多数节点，变为Committed，再提交给业务状态机

Log会增长，占用资源，所以需要Compaction

<font color='red'>Snapshot是Log Compaction常用的方法</font>

Snapshot的时候，除了业务状态机dump自己的业务数据之外，还需要一些元信息:
- last included index：做Snapshot的时候最后apply的log entry的index
- last included term：做Snapshot的时候最后apply的log entry的term
- last included configuration：做Snapshot的时候最后的Configuration

做完Snapshot之后，last include index之前的Log都会被删除，需要使用term cofiguration index进行重启恢复。

Snapshot会花费比较长的时间，如果期望Snapshot不影响正常的Log Entry同步，需要<font color='red'>采用Copy-On-Write</font>的技术来实现。例如，底层的数据结构或者是存储支持COW，LSM-Tree类型的数据结构和KV库一般都支持Snapshot；或者是使用系统的COW支持，Linux的fork，或者是ZFS的Snapshot等。

## 13. InstalSnapshot

leader和Follower<u>独立的做Snapshot</u>，但是Leader和Follower之间的<u>Log差距是很大的</u>。Leader已经做完了一个Snapshot，但是Follower依然没有同步完Snapshot中的Log，这个时候就<u>需要Leader向Follower发送Snapshot</u>。

Follower收到InstallSnapshot请求之后的处理流程：
1. 检查req.term < currentTerm直接返回失败
2. 创建Snapshot，并接受后续的数据
3. 保存Snapshot元信息，并删除之前的完成的或者是未完成的Snapshot
4. 如果现存的LogEntry与snapshot的last_included_index和last_include_term一致，保留后续的log；否则删除全部Log
5. Follower重新加载Snapshot

由于<u>Snapshot可能会比较大</u>，RPC都有消息大小限制，<u>需要采用些手段进行处理</u>：可以拆分数据采用N个RPC，每个RPC带上offset和data的方式；也可以采用Chunk的方式，采用一个RPC，但是拆分成多个Chunk进行发送。

## 14. Menbership Management

节点的动态删除和动态增加。

**Joint-Consensus**:RAFT采用<font color='red'>协同一致性</font>的方式<u>解决节点的变更</u>。

## 15. Single-Server Change


## 16. Configuration Store

## 17. Safety

## 18. braft性能提升

在braft中，采用了以下几点方法来提高的性能:

- 数据流是全并发的， leader写本地磁盘和向follower复制数据是完全并发的。
- 尽可能的提高局部性，充分发挥不同层面的cache的作用
- 尽可能隔离不同硬件的访问，通过流水线的形式提高吞吐
- 尽可能的降低锁临界区大小， 关键路径上采用lock-free/wait-free算法.

## 19. cli
braft提供了一系列API用来控制复制主或者具体节点, 可以选择在程序了调用[API](./braft-master/src/braft/cli.h)或者使用[braft_cli](./braft-master/tools/braft_cli.cpp)来给节点发远程控制命令

**API**:
```cpp
// Add a new peer（对等点，同级） into the replicating（复制组） group which consists of |conf|.
// Returns OK on success, error information otherwise.
butil::Status add_peer(const GroupId& group_id, const Configuration& conf,
                       const PeerId& peer_id, const CliOptions& options);
// Remove a peer from the replicating group which consists of |conf|.
// Returns OK on success, error information otherwise.
butil::Status remove_peer(const GroupId& group_id, const Configuration& conf,
                          const PeerId& peer_id, const CliOptions& options);
// Gracefully change the peers of the replication group.
butil::Status change_peers(const GroupId& group_id, const Configuration& conf, 
                           const Configuration& new_peers,
                           const CliOptions& options);
// Transfer the leader of the replication group to the target peer
butil::Status transfer_leader(const GroupId& group_id, const Configuration& conf,
                              const PeerId& peer, const CliOptions& options);
// Reset the peer set of the target peer
butil::Status reset_peer(const GroupId& group_id, const PeerId& peer_id,
                         const Configuration& new_conf,
                         const CliOptions& options);
// Ask the peer to dump a snapshot immediately.
butil::Status snapshot(const GroupId& group_id, const PeerId& peer_id,
                       const CliOptions& options);
```

**braft_cli**：
braft_cli提供了命令行工具, 作用和API类似

```shell
braft_cli: Usage: braft_cli [Command] [OPTIONS...]
Command:
  add_peer --group=$group_id --peer=$adding_peer --conf=$current_conf
  remove_peer --group=$group_id --peer=$removing_peer --conf=$current_conf
  change_peers --group=$group_id --conf=$current_conf --new_peers=$new_peers
  reset_peer --group=$group_id --peer==$target_peer --new_peers=$new_peers
  snapshot --group=$group_id --peer=$target_peer
  transfer_leader --group=$group_id --peer=$target_leader --conf=$current_conf
```

## 20. qjm

### 概述
QJM是QuorumJournalManager的简介，是Hadoop V2中的namenode的默认HA方案。QJM的Recovery算法是一个basic paxos实现，不负责选主，选主由外部实现。

两个组件：journal node和libQJM

*libQJM*：负责journal数据的读写，包括journal在异常情况下的一致性恢复。

*JournalNode*：负责log数据的存储。

整体QJM的流程：

1. Fencing prior writers
2. Recovering in-progress logs
3. Start a new log segment
4. Write edits
5. Finalize log segment
6. Go to step 3

### Fencing

解决分布式系统发生“脑裂”的解药

## 21. client

braft并不能直接被任何client访问

一个能方位Braft需要一些要素：

### 总体流程
要访问braft的主节点，需要做这么一些事情:
- 需要知道这个复制组由那些节点
- 查询Leader位置
- 感知Leader变化
- 向Leader发起RPC

### Route Table

Braft提供Route Table功能，帮助你的<u>进程记录和追踪某个节点的主节点位置</u>，包含以下功能：
```cpp
// Update configuration of group in route table
int update_configuration(const GroupId& group, const Configuration& conf);
int update_configuration(const GroupId& group, const std::string& conf_str);
// Get the cached(隐藏，缓存) leader of group.
// Returns:
//  0 : success
//  1 : Not sure about the leader
//  -1, otherwise
int select_leader(const GroupId& group, PeerId* leader);
// Update leader
int update_leader(const GroupId& group, const PeerId& leader);
int update_leader(const GroupId& group, const std::string& leader_str);
// Blocking the thread until query_leader finishes
butil::Status refresh_leader(const GroupId& group, int timeout_ms);
// Remove this group from route table
int remove_group(const GroupId& group);
```