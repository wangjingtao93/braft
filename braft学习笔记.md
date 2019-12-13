# 1. 概述

[贼棒入门理解小视频](http://thesecretlivesofdata.com/raft/)
[开源库]( https://github.com/brpc/braft)
1. The election timeout is randomized to be between 150ms and 300ms.
2. ACK------>acknowledge vt. 承认；答谢；报偿；告知已收到
3. 每触发一次选举，term+1
4. leader 发送的logEntry是在下一个heartbeat时，和它一起
5. leader response to client是在接收到flower的ACK之哦户
6. 为什么要网络分区呢(network partion)？有什么好处?

**解决分布式一致性问题**（distributed consensus）,即无论发生什么故障，在多个机器间对某一个值都能达成一致。

**做法**：就是把操作变成日志。通常把操作write-ahead-log（简称WAL）。然后后所有的副本WAL保持一致。对WAL按顺序执行即可。

C++，基于brpc的[RAFT 一致算法和 replicated state machine。应用于分布式系统

**过程**：
1. leader wal过程，即将槽做写入日志变成logentry
2. leader将LogEntry复制给多数节点（Follower），此时logEntru有变成committed
3. Follower检测安全的，附带ACK返回给leader
4. leader收到多数的Follower的ACK认为此LogEntry是safe的，则apply于状态机
# 2. raft
Paxos Zab Viewstamped Replication

Paxos是根本

Raft最好，简单易用

raft作为复制状态机，是分布式系统中<font color ='red'>最核心最基础</font>的组件，<font color ='red'>提供命令</font>在多个节点<font color ='red'>有序复制和执行</font>
![img](./images/raft.png)

raft可以解决一致性和分区容忍性，但是不能解决Available问题，<u>分布式系统中一些常用的功能及比较:</u>
|                       | **Multi-Paxos** | **RAFT** | **Zab** | **Viewstamped Replication** |**不成熟的解释**
| --------------------- | --------------- | -------- | ------- | --------------------------- |---------|
| Leader Election       | Yes             | Yes      | Yes     | Yes    |leader选举                      |
| Log Replication       | Yes             | Yes      | Yes     | Yes     |日志复制                    |
| Log Recovery          | Yes?            | Yes      | Yes     | Yes      |日志恢复                   |
| Log Compaction        | Yes?            | Yes      | Yes     | Yes     |日志压缩（snapshot）                    |
| Membership Management | Yes?            | Yes      | No      | Yes     |关系处理（管理）                    |
| Understandable        | Hard            | Easy     | Medium  | Medium                      |
| Protocol Details      | No              | Yes      | Yes     | Yes                         |
| Implements            | Few             | Mass     | Many    | Few                         |


<font color='red'>BRAFT节点状态</font>：
- <font color='red'>Leader</font>：接收Client的请求，并进行复制，任何时刻只有一个Leader
- <font color='red'>Follower</font>：被动接收各种RPC请求
- <font color='red'>Candidate</font>：用于选举出一个新的Leader

Follower--->Candidate::: Follower长时间没收到心跳

Follower--->Leader :::Follower收到多数**投票应答**

Leader、Candidate--->Follower::: Leader、Candidatek接收到更高版本的消息

过程：leader election， Log Replication

# 3. compile and install
1. 装brpc
   - 安装通用开源库
   ```
   yum install -y glog-devel gflags-devel protobuf-devel protobuf-compiler leveldb-devel gperftools-devel gtest-devel snappy-devel openssl-devel libaio-devel libstdc++-devel
   ```
2. cmake -DBRPC_WITH_GLOG=ON,
3. 修改CmakeLists.txt,glog动态库的路径要添上
4. 修改server的gflag
   


# 4. raft功能
raft并不能解决available问题
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


# 5. Leader Election

1. RAFT将时间划分到Term,用于选举

2. 选举出来的Leader，一定是多数节点中Log数据最新的节点。
3. Log新旧的比较，是基于lastLogTerm和lastLogIndex进行比较，而不是基于currentTerm和lastLogIndex进行比较。currentTerm只是用于忽略老的Term的vote请求，或者提升自己的currentTerm，并不参与Log新旧的决策。

4. 选举过程中，每个Candidate节点先将本地的Current Term加一，然后向其他节点发送RequestVote请求。

具体的判断规则（看不懂）：
1. 如果now – lastLeaderUpdateTimestamp < elect_timeout，忽略请求
2. 如果req.term < currentTerm，忽略请求。
3. 如果req.term > currentTerm，设置req.term到currentTerm中，如果是Leader和Candidate转为Follower。
4. 如果req.term == currentTerm，并且本地voteFor记录为空或者是与vote请求中term和CandidateId一致，req.lastLogIndex > lastLogIndex，即Candidate数据新于本地则同意选主请求。
5. 如果req.term == currentTerm，如果本地voteFor记录非空或者是与vote请求中term一致CandidateId不一致，则拒绝选主请求。
6. 如果lastLogTerm > req.lastLogTerm，本地最后一条Log的Term大于请求中的lastLogTerm，说明candidate上数据比本地旧，拒绝选主请求。


# 6. Symmetric network partitioning

对称网络划分：可能是因为节点故障导致，比如网络不通了。

新的节点上线，属于PeerSet中的节点可以加入，不属于PeerSet中的节点，leader会永远忽略。

属于PeerSet中的新节点上线，会导致leader下台（stepDown）。会打断lease(租约)，导致复制组不可用

# 7. Asymmetric network partitioning

原来的RAFT论文中对非对称的网络划分做的不好。


# 8. StepDown

原始的RAFT协议是Leader收到任何term高于Current的请求都会进行stepDown,实际过程中应该在一下几个时刻进行StepDown（不理解）:
- Leader接收到AppendEntries的失败应答，Term比currentTerm大
- Leader在ElectionTimeout内没有写多数成功，通过logic clock检查实现（1个ElectionTimeout内会有10个HeartBeat）
- Leader在进行RemovePeer的LogEntry被Commit的时候，不在节点列表中，进行StepDown，通常还会进行Shutdown

# 9. Log Replication

1. leader负责接收客户端的请求，每个请求包含一个<font color = 'red'>要被复制状态机执行</font>的<font color = 'red'>指令</font>。

2. leader将这个<u>指令（command）追加到log</u>形成一个新的<font color= 'red'>entry</font>。然后通过AppendEntries RPCs将entry<u>分发给其他的servers（follower）</u>，这个时候entry已经时候committed了。

3. 其他sever(follower)如果没发现entr有问题，复制成功后会给leader一个<font color = 'red'>表示成功的ACK</font>。leader收到大多数的ACK后应用（apply）该日志（Log Entry）。所以applied的entry一定是committed了。

4. Log Entry的内容：Command, 其他server从leader收到该entry的term号(判断一些log之间不一致的状态) ,index(索引，指明Entry在log中的位置)

![img](./images/logs.png)
**总结**:Logs由entries组成，entries被顺序编号。每一个entry包括：term和command（用于statmachine的）。如果entry可以被安全的应用于state machines，则被视为committed

<font color = 'red'>leader需要决定什么时候将日志（log Entry）应用给状态机是安全的</font>
<font color = 'red'>committed</font>:可以被状态机应用的Entry叫committed。RAFT<font color = 'red'>保证committed entries持久化</font>，<u>并且最终被其他状态机应用</u>
**重中之重**：<u>一个log entry一旦复制给了大多数节点就成为committed</u>,(意思是大多数节点返回了ACK吧)

**节点重启过程**：先加载上一个Snapshot，再加入RAFT复制组，选主或者是接收更新。。因为Snapshot中的数据一定是Applied，那么肯定是Committed的，加载是安全的。但是Log中的数据，不一定是Committed的，因为我们没有持久化CommittedIndex，所以不确定Log是否是Committed，不能进行加载。样先加载Snapshot虽然延迟了新节点加入集群的时间，但是<u>能够保证一旦一个节点变为Leader之后能够比较快的加载完全数据</u>，并提供服务。同理，Follower接收到InstallSnapshot之后，接收并加载完Snapshot之后再回复Leader。

## 9.1. 关于Replication

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
## 9.2. 链式复制
使用最广泛。

将数据复制到全部节点之后，在想client应答成功
高吞吐，高延时
## 9.3. 树形复制
client ---> primary --->两个Secondary。

只有Primary节点IO慢才会导致复制卡顿

## 9.4. 分发式复制
<u>分发复制中节点都是对等的</u>，Client直接向各个节点直接进行分发写入，节点之间并不进行通信复制，只要写入多数节点成功，就判为写入成功。

延时低
- 不能同时有两个Writer，避免多个写入同时发生，保证数据的一致性
- 另外一个问题就是<u>如何读到最新的数据</u>。因为节点都是对等的，且没有互相通信。Reader只有读取全部节点，根据节点返回的version或者是index之类的才能判断出哪些节点返回的数据是有效的，<u>效率较低</u>
# 10. 关于节点重启
<font color='red'>每个节点重启之后，先加载上一个Snapshot，再加入RAFT复制组，选主或者更新</font>
因为snapshot中的数据一定是Applied的，即committed，加载是安全的。但是log中的数据，不一定适合Committed的，因为我们没有持久化CommittedIndex，所以不确定是否适合Committed的，不能进行加载。

Follower接收到InstallSnapshot之后，接收并加载完Snapshot之后再回复Leader（这是什么操作）
# 11. Log Recovery

**目的**：
1. 保证已经committed的数据不会丢失,
2. 未Committed的数据转变为Committed，
3. 但不会因为 “修复过程" "中断又重启"而影响节点之间一致性。(修复过程还能中断，又重启)

分为current Term修复和prev Term修复：
**current Term修复**：主要是解决某些Follower节点重启加入集群，或者是新增Follower节点加入集群，
**prev Term**：主要是在保证Leader切换前后数据的一致性

细节看下面两节：
## 11.1. current Term修复
leader需要向Follower节点传输漏调的log Entry。如果Follower需要的Log Entry已经在Leader上Log Compaction清除掉了，Leader需要将上一个Snapshot和其后的Log Entry传输给Follower节点。
**注意**：Leader-Alive模式下，只要Leader将某一条Log Entry复制到多数节点上，Log Entry就转变为Committed。
## 11.2. prev Term修复
每次选举出来的Leader一定包含已经committed的数据（因为抽屉原理，选举出来的Leader是多数中数据最新的所以，一定包含已经在多数节点上commit的数据）。<u>新的Leader将会覆盖其他节点上不一致的数据</u>。

**有一种奇怪的情况**：
选举出的leader一定包含一个Term的Leader已经Committed的LogEntry，但是也可能包含上一个Term的Leader切换未committed的Log Entry（为什么涅）。这种LogEntry需要转变成为Committed，相对比较麻烦，需要烤炉Leader多次切换且未完成LogEntry，需要保证最终提案是一致的，确定的。<u>就是说其他Term未committed的LogEntry都得修复变成committed的</u>


<u>下面这段话就是说：先把最新的Term的新的LogEntry复制到或者修复到多数节点，修复之前未Committed的LogEntry，到多数节点，才能被称为committed</u>
“RAFT中增加了一个约束：对于之前Term的未Committed数据，修复到多数节点，且在新的Term下至少有一条新的Log Entry被复制或修复到多数节点之后，才能认为之前未Committed的Log Entry转为Committed”

**具体过程**：（就是说，需要回溯，不细看了）
选出Leader之后，Leader运行过程中会进行副本的修复，这个时候只要多数副本数据完整就可以正常工作。Leader为每个Follower维护一个nextId，标示下一个要发送的logIndex。Follower接收到AppendEntries之后会进行一些一致性检查，检查AppendEntries中指定的LastLogIndex是否一致，如果不一致就会向Leader返回失败。Leader接收到失败之后，会将nextId减1，重新进行发送，直到成功。这个回溯的过程实际上就是寻找Follower上最后一个CommittedId，然后Leader发送其后的LogEntry。因为Follower持久化CommittedId将会导致更新延迟增大，回溯的窗口也只是Leader切换导致的副本间不一致的LogEntry，这部分数据量一般都很小。

![img](./images/log_replication.png)
上图中Follower a与Leader数据都是一致的，只是有数据缺失，可以优化为直接通知Leader从logIndex=5开始进行重传，这样只需一次回溯。Follower b与Leader有不一致性的数据，需要回溯7次才能找到需要进行重传的位置。
重新选取Leader之后，新的Leader没有之前内存中维护的nextId，以本地lastLogIndex+1作为每个节点的nextId。这样根据节点的AppendEntries应答可以调整nextId：
```cpp
local.nextIndex = max(min(local.nextIndex-1, resp.LastLogIndex+1), 1)
```
# 12. Log Compaction

**更新**：Leader写入Log，复制到多数节点，变为Committed，再提交（apply）给业务状态机

Log会增长，占用资源，所以需要Compaction

<font color='red'>Snapshot是Log Compaction常用的方法</font>

![img](./images/log_compaction.png)
服务器用一个新的快照替换其日志中的已committed的entries(索引1到5)，

Snapshot的时候，除了业务状态机dump（转储）自己的业务数据之外，还需要一些元信息:
- last included index：做Snapshot的时候最后apply的log entry的index
- last included term：做Snapshot的时候最后apply的log entry的term
- last included configuration：做Snapshot的时候最后的Configuration

做完Snapshot之后，持久化的一个可靠存储系统中，完成Snapshot之后，last include index之前的Log都会被删除，需要使用term cofiguration index进行重启恢复。

Snapshot会花费比较长的时间，如果期望Snapshot不影响正常的Log Entry同步，需要<font color='red'>采用Copy-On-Write</font>的技术来实现。例如，底层的数据结构或者是存储支持COW，LSM-Tree类型的数据结构和KV库一般都支持Snapshot；或者是使用系统的COW支持，Linux的fork，或者是ZFS的Snapshot等。

# 13. InstallSnapshot

**定义**：leader和Follower<u>独立的做Snapshot</u>，但是Leader和Follower之间的<u>Log差距有很大的时候</u>。Leader已经做完了一个Snapshot，但是Follower依然没有同步完Snapshot中的Log，这个时候就<u>需要Leader向Follower发送Snapshot</u>。这就叫InstallSnapshot

Follower收到InstallSnapshot请求之后的处理流程：
1. 检查req.term < currentTerm直接返回失败
2. 创建Snapshot，并接受后续的数据
3. 保存Snapshot元信息，并删除之前的完成的或者是未完成的Snapshot
4. 如果现存的LogEntry与snapshot的last_included_index和last_include_term一致，保留后续的log；否则删除全部Log
5. Follower重新加载Snapshot

由于<u>Snapshot可能会比较大</u>，RPC都有消息大小限制，<u>需要采用些手段进行处理</u>：可以拆分数据采用N个RPC，每个RPC带上offset和data的方式；也可以采用Chunk的方式，采用一个RPC，但是拆分成多个Chunk进行发送。

**有几种比较特殊的情况**（可以暂且不研究）：
由于InstallSnapshot请求也可能会重传，或者是InstallSnapshot过程中发生了Leader切换，新Leader的last_included_index比较小，可能还有UnCommitted的LogEntry，<u>这个时候就不需要进行InstallSnapshot。所以Follower在收到InstallSnapshot的时候，Follower不是直接删除全部Log，而是将Snapshot的last_include_index及其之前的Log Entry删掉，last_include_index后续的Log Entry继续保留</u>。如果需要保留后面的Log Entry，这个时候其实不用进行加载Snapshot了，如果全部删除的话，就需要重新加载Snapshot恢复到最新的状态。
# 14. Menbership Management

1. 分布式需要支持节点的动态删除和动态增加。
2. 节点的增删过程不能影响当前数据的复制。
3. 并能够自动对新节点进行数据修复
4. 如果删除节点涉及Leader，还需要触发自动选主



## 14.1. Joint-Consensus
RAFT采用<font color='red'>协同一致性</font>的方式<u>解决节点的变更</u>。RAFT采用协同一致性的方式来解决节点的变更，先提交一个包含新老节点结合的Configuration命令，当这条消息Commit之后再提交一条只包含新节点的Configuration命令。新老集合中任何一个节点都可以成为Leader，这样Leader宕机之后，如果新的Leader没有看到包括新老节点集合的Configuration日志（这条configuration日志在老节点集合中没有写到多数），继续以老节点集合组建复制组（老节点集合中收到configuration日志的节点会截断日志）；如果新的Leader看到了包括新老节点集合的Configuration日志，将未完成的节点变更流程走完。具体流程如下：

1. 首先对新节点进行CaughtUp追数据
2. 全部新节点完成CaughtUp之后，向新老集合发送Cold+new命令
3. 如果新节点集合多数和老节点集合多数都应答了Cold+new，就向新老节点集合发送Cnew命令
4. 如果新节点集合多数应答了Cnew，完成节点切换


## 14.2. Single-Server Change


## 14.3. Configuration Store

## 14.4. Safety

## 14.5. braft性能提升

在braft中，采用了以下几点方法来提高的性能:

- 数据流是全并发的， leader写本地磁盘和向follower复制数据是完全并发的。
- 尽可能的提高局部性，充分发挥不同层面的cache的作用
- 尽可能隔离不同硬件的访问，通过流水线的形式提高吞吐
- 尽可能的降低锁临界区大小， 关键路径上采用lock-free/wait-free算法.

# 15. cli
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

# 16. qjm

## 16.1. 概述
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

## 16.2. Fencing

解决分布式系统发生“脑裂”的解药

# 17. client

braft并不能直接被任何client访问

一个能方位Braft需要一些要素：

## 17.1. 总体流程
要访问braft的主节点，需要做这么一些事情:
- 需要知道这个复制组由那些节点
- 查询Leader位置
- 感知Leader变化
- 向Leader发起RPC

## 17.2. Route Table

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


**braft 本身并不提供server功能**， 你可以将braft集成到包括brpc在内的任意编程框架中，本文主要是阐述如何在分布式Server中使用braft来构建高可用系统。具体业务如何实现一个Server，本文不在展开。

# 18. server

[server-side code](../../example/counter/server.cpp) of Counter

## 18.1. 注册并且启动Server

braft需要运行在具体的brpc server里面你可以让braft和你的业务共享同样的端口， 也可以将braft启动到不同的端口中.

brpc允许一个端口上注册多个逻辑Service,  如果你的Service同样运行在brpc Server里面，你可以管理brpc Server并且调用以下任意一个接口将braft相关的Service加入到你的Server中。这样能让braft和你的业务跑在同样的端口里面， 降低运维的复杂度。如果对brpc Server的使用不是非常了解， 可以先查看[wiki](https://github.com/brpc/brpc/blob/master/docs/cn/server.md)页面. **注意: 如果你提供的是对外网用户暴露的服务，不要让braft跑在相同的端口上。**

```cpp
// Attach raft services to |server|, this makes the raft services share the same
// listen address with the user services.
// 使raft Services和usr Services共享listen address 
//
// NOTE: Now we only allow the backing Server to be started with a specific
// listen address, if the Server is going to be started from a range of ports, 
// the behavior is undefined.
// Returns 0 on success, -1 otherwise.
//允许后备服务器以一个指定的listen address start（启动）
// Server(服务器) 从一系列端口启动，行为未定义
int add_service(brpc::Server* server, const butil::EndPoint& listen_addr);
int add_service(brpc::Server* server, int port);
int add_service(brpc::Server* server, const char* const butil::EndPoint& listen_addr);
```

- **调用这些接口之前不要启动server， 否则相关的Service将无法加入到这个server中. 导致调用失败.**
- **启动这个server的端口需要和add_service传入的端口一致， 不然会导致这个节点无法正常收发RPC请求.**

## 18.2. 实现业务状态机

你需要继承braft::StateMachine并且实现里面的接口

```cpp

#include <braft/raft.h>

// NOTE: All the interfaces are not required to be thread safe and they are 
// called sequentially, saying that every single method will block all the 
// following ones.
// 所有接口都不需要是线程安全的,他们被顺序调用（啥意思，读写内存 互斥 锁之类的？） 
// 说每个方法都会阻止以下所有方法
class YourStateMachineImple : public braft::StateMachine {
protected:
    // on_apply是*必须*实现的
    // on_apply会在一条或者多条日志被多数节点持久化之后调用， 通知用户将这些日志所表示的操作应用到业务状态机中.
    // 通过iter, 可以从遍历所有未处理但是已经提交的日志， 如果你的状态机支持批量更新，可以一次性获取多
    // 条日志提高状态机的吞吐.
    // 
    void on_apply(braft::Iterator& iter) {
        // A batch(批) of tasks are committed, which must be processed through 
        // |iter|
        // 一批task要提交，必须通过|iter|实现
        for (; iter.valid(); iter.next()) {
            // This guard helps invoke iter.done()->Run() asynchronously to
            // avoid that callback blocks the StateMachine.
            braft::AsyncClosureGuard closure_guard(iter.done());
            // Parse（解析） operation from iter.data() and execute this operation
            // op = parse(iter.data());
            // result = process(op)
          
            // The purpose of following logs is to help you understand the way
            // this StateMachine works.
            // Remove these logs in performance-sensitive servers.
            LOG_IF(INFO, FLAGS_log_applied_task) 
                    << "Exeucted operation " << op
                    << " and the result is " << result
                    << " at log_index=" << iter.index();
        }
    }
    // 当这个braft节点被shutdown之后， 当所有的操作都结束， 会调用on_shutdown, 来通知用户这个状态机不再被使用。
    // 这时候你可以安全的释放一些资源了.
    virtual void on_shutdown() {
        // Cleanup resources you'd like
    }
```

## 18.3. iterator

通过braft::iterator你可以遍历从所有有的任务

```cpp
class Iterator {
    // Move to the next task.
    void next();
    // Return a unique and monotonically(单调） increasing identifier（标识符） of the current 
    // task:
    //  - Uniqueness(唯一) guarantees that committed tasks in different peers with 
    //    the same index（相同） are always the same and kept unchanged.
    //    唯一性保证在具有/相同索引的不同对等点中提交的任务总是相同的，并且保持不变。
    //    相同索引，不同peers
    //  - Monotonicity guarantees that for any index pair i, j (i < j), task 
    //    at index |i| must be applied before task at index |j| in all the 
    //    peers from the group.
    //    单调性保证顺序执行
    int64_t index() const;
    // Returns the term of the leader which to task was applied to.
    // 返回要执行任务的领导人的任期
    int64_t term() const;
    // Return the data whose content is the same as what was passed to
    // 返回内容与传递内容相同的数据
    // Node::apply in the leader node.
    const butil::IOBuf& data() const;
    // If done() is non-NULL, you must call done()->Run() after applying this
    // task no matter this operation succeeds or fails, otherwise the
    // corresponding resources would leak.
    //
    // If this task is proposed by(由...提出) this Node when it was the leader of this 
    // group and the leadership has not changed before this point在此之前领导层没有改变 , done() is 
    // exactly what was passed to Node::apply which may stand for some 
    // continuation (such as respond to the client) after updating the 
    // StateMachine with the given task. Otherweise done() must be NULL.
    Closure* done() const;
    // Return true this iterator is currently references to a valid task返回true这个迭代器当前是对有效任务的引用 ,
    //false otherwise, indicating that the iterator has reached the end of this
    // batch of tasks or some error has occurred
    bool valid() const;
    // Invoked when some critical error occurred发生某些严重错误时调用
    // And we will consider the last 
    // |ntail| tasks (starting from the last iterated one) as not applied. After
    // this point, no further changes on the StateMachine as well as the Node 
    // would be allowed and you should try to repair this replica or just drop 
    // it.
    //
    // If |st| is not NULL, it should describe the detail of the error.
    void set_error_and_rollback(size_t ntail = 1, const butil::Status* st = NULL);
};
```

## 18.4. 构造braft::Node

一个Node代表了一个RAFT实例， Node的ID由两个部分组成:

- GroupId: 为一个string, 表示这个复制组的ID.比如BG_name
- PeerId, 结构是一个[EndPoint](https://github.com/brpc/brpc/blob/master/src/butil/endpoint.h)表示对外服务的端口, 外加一个index(默认为0). <u>其中index的作用是让不同的副本能运行在同一个进程内</u>, 在下面几个场景中，这个值不能忽略:

```cpp
Node(const GroupId& group_id, const PeerId& peer_id);
```

启动这个节点:

```cpp
struct NodeOptions {
    // A follower would become a candidate if it doesn't receive any message 
    // from the leader in |election_timeout_ms| milliseconds
    // Default: 1000 (1s)，重新选举触发时间
    int election_timeout_ms;

    // A snapshot saving would be triggered（触发） every |snapshot_interval_s| seconds
    // if this was reset as a positive number如果这被重置为正数
    // If |snapshot_interval_s| <= 0, the time based snapshot would be disabled.
    // 为负值，则快照不可用 
    //
    // Default: 3600 (1 hour)
    int snapshot_interval_s;

    // We will regard a adding peer as caught up if the margin between the 
    // last_log_index of this peer and the last_log_index of leader is less than
|   // catchup_margin|
    // 我们将把添加的同行视为追赶，如果此peer的last_log_index和leader的last_log_inde之间的差距小于1000
    // Default: 1000
    int catchup_margin;

    // If node is starting from a empty environment (both LogStorage and
    // SnapshotStorage are empty), it would use |initial_conf| as the
    // configuration of the group, otherwise it would load（加载） configuration from
    // the existing environment.
    // 如果节点的启动环境是空的（没有log和snapshot），则根据|initial_conf|作为group配置
    // 否则将从现有的环境加载配置
    // Default: A empty group
    Configuration initial_conf;

    // The specific StateMachine implemented your business logic, which must be
    // a valid（有效的） instance（实例）.
    // 特特定的状态机，有效的实例，实现业务逻辑
    StateMachine* fsm;

    // If |node_owns_fsm| is true. |fms| would be destroyed（销毁） when the 
    // backing Node(后备节点) is no longer referenced.
    // 当不再引用后备节点时候，fms会被摧毁
    // Default: false
    bool node_owns_fsm;

    // Describe a specific LogStorage in format ${type}://${parameters}
    std::string log_uri;

    // Describe a specific StableStorage in format ${type}://${parameters}
    std::string raft_meta_uri;

    // Describe a specific SnapshotStorage in format ${type}://${parameters}
    std::string snapshot_uri;
    
    // If enable, duplicate(重复) files will be filtered（过滤） out before copy snapshot from remote
    // 如果启用，则在从远程复制快照之前将过滤掉重复的文件，避免无用的传输（本地和远程相同的文件名和校验和（存储在meta）视为重复）
    // to avoid useless transmission. Two files in local and remote are duplicate,
    // only if they has the same filename and the same checksum (stored in file meta).
    // Default: false
    bool filter_before_copy_remote;
    
    // If true, RPCs through raft_cli will be denied则通过raft_cli的RPC将被拒绝.
    // Default: false
    bool disable_cli;
};
class Node {
    int init(const NodeOptions& options);
};
```

* initial_conf只有在这个复制组从空节点（<font color='red'>什么才是空节点</font>）启动才会生效，当有snapshot和log里的数据不为空的时候的时候从其中恢复Configuration。<u>initial_conf只用于创建复制组</u>，第一个节点将自己设置进initial_conf，再调用add_peer添加其他节点，其他节点initial_conf设置为空；也可以多个节点同时设置相同的inital_conf(多个节点的ip:port)来同时启动空节点。

<font color='red'>
* RAFT需要三种不同的持久存储, 分别是:

  * RaftMetaStorage, 用来存放一些RAFT算法自身的状态数据， 比如term, vote_for等信息.
  * LogStorage, 用来存放用户提交的WAL
  * SnapshotStorage, 用来存放用户的Snapshot以及元信息.</font>

  用三个不同的uri（<font color='red'>uri是什么</font>）来表示, 并且提供了基于本地文件系统的默认实现， type为local, 比如 local://data 就是存放到当前文件夹的data目录， local:///home/disk1/data 就是存放在 /home/disk1/data中。libraft中有默认的local://实现，<u>用户可以根据需要继承实现相应的Storage即自己可以十二指路径</u>

## 18.5. 将操作提交到复制组

你需要将你的操作序列化成[IOBuf](https://github.com/brpc/brpc/blob/master/src/butil/iobuf.h), 这是一个非连续零拷贝的缓存结构. 构造一个Task, 并且向braft::Node提交

```cpp
#include <braft/raft.h>

...
void function(op, callback) {
    butil::IOBuf data;
    serialize(op, &data);
    braft::Task task;
    task.data = &data;
    task.done = make_closure(callback);
    task.expected_term = expected_term;
    return _node->apply(task);
}
```

具体接口

```cpp
struct Task {
    Task() : data(NULL), done(NULL) {}

    // The data applied to StateMachine
    // 数据应用于StateMachine
    base::IOBuf* data;

    // Continuation when the data is applied to StateMachine or error occurs.
    // 数据应用于StateMachine
    Closure* done;
 
    // Reject this task if expected_term doesn't match the current term of this Node if the value is not -1
    //如果expected_term与此节点的当前term不匹配（如果值不为-1），则拒绝此任务
    // Default: -1
    int64_t expected_term;
};
    
// apply task to the replicated-state-machine
// 将任务应用于复制状态机
// About the ownership(所有权):啥是所有权，有啥用
// |task.data|: for the performance consideration, we will take way the  
//              content. If you want keep the content, copy it before call
//              this function
//              为了性能考虑，我们将采取内容
//              需要保留内容的话，则在调用函数之前拷贝他
// |task.done|: If the data is successfully committed to the raft group. We
//              will pass the ownership to StateMachine::on_apply.
//              Otherwise we will specify the error and call it.
//              data成功提交给raft group后，所有权就会移交给StateMachine::on_apply.   
//              否则我们将指定错误并调用它 

void apply(const Task& task);
```

* **Thread-Safety**: apply是线程安全的，并且实现基本等价于是[wait-free](https://en.wikipedia.org/wiki/Non-blocking_algorithm#Wait-freedom). 这意味着你可以在多线程向同一个Node中提交WAL.（WAL是什么）


* **apply不一定成功**，如果失败的话会设置done中的status，并回调。on_apply中一定是成功committed的，但是apply的结果在leader发生切换的时候存在[false negative](https://en.wikipedia.org/wiki/False_positives_and_false_negatives#False_negative_error), 即框架通知这次WAL写失败了， 但最终相同内容的日志被新的leader确认提交并且通知到StateMachine. 这个时候通常客户端会重试(超时一般也是这么处理的), 所以一般需要确保日志所代表的操作是[幂等](https://en.wikipedia.org/wiki/Idempotence)（任意多次执行所产生的影响均与一次执行的影响相同）的。<font color='red'>apply不一定成功，committed一定成功，leader切换的是偶apply结果会false negative</font>

* 不同的日志处理结果是独立的, **一个线程**连续提交了A,B两个日志， 那么以下组合都有可能发生:

  * A 和 B都成功
  * A 和 B都失败
  * A 成功 B失败
  * A 失败 B成功

  当A, B都成功的时候， 他们在日志中的顺序会和提交顺序严格保证一致.

* 由于apply是异步的，有可能某个节点在term1是leader，apply了一条log，但是中间发生了主从切换，在很短的时间内这个节点又变为term3的leader，之前apply的日志才开始进行处理，这种情况下要实现严格意义上的复制状态机，需要解决这种ABA问题，可以在apply的时候设置leader当时的term.

raft::Closure是一个特殊的protobuf::Closure的子类， 可以用了标记一次异步调用成功或者失败. 和protobuf::Closure一样， 你需要继承这个类，实现Run接口。 当一次异步调用真正结束之后， Run会被框架调用， 此时你可以通过[status()](https://github.com/brpc/brpc/src/butil/status.h)来确认这次调用是否成功或者失败。

```cpp
// Raft-specific closure which encloses a base::Status to report if the
// operation was successful.
// 如果操作成功，Raft-specific 会关闭 还有report
class Closure : public google::protobuf::Closure {
public:
    base::Status& status() { return _st; }
    const base::Status& status() const { return _st; }
};
```

## 18.6. 监听braft::Node状态变更

StateMachine中还提供了一些接口, 实现这些接口能够监听Node的状态变化，你的系统可以针对这些状态变化实现一些特定的逻辑(比如转发消息给leader节点)

```cpp
class StateMachine {
...
    // Invoked once when the raft node was shut down. Corresponding resources are safe to cleared ever after
    // 当raft node 关闭时调用一次。相应的资源是可以安全的清除，默认什么都不做
    // to cleared ever after.
    // Default do nothing
    virtual void on_shutdown();
    // Invoked when the belonging node becomes the leader of the group at |term|
    // Default: Do nothing
    // 当所属节点成为| term |的组的领导者时调用 
    virtual void on_leader_start(int64_t term);
    // Invoked when this node is no longer the leader of the belonging group.
    // |status| describes more details about the reason.
    // 当此节点不再是所属组的领导者时调用
    virtual void on_leader_stop(const butil::Status& status);
    // Invoked when some critical error occurred and this Node stops working  ever after 
    // 发生某些严重错误时调用，此节点此后停止工作 
    virtual void on_error(const ::braft::Error& e);
    // Invoked when a configuration has been committed to the group
    virtual void on_configuration_committed(const ::braft::Configuration& conf);
    // 将配置提交到组virtual void on_configuration_committed时调用（const :: draft :: Configuration conf） 
    // Invoked when a follower stops following a leader
    // 跟随者停止跟随领导者时调用 
    // situations(情况) including: 
    // 1. Election timeout is expired. 选举超时已过期 
    // 2. Received message from a node with higher term选举超时已过期 
    virtual void on_stop_following(const ::braft::LeaderChangeContext& ctx);
    // Invoked when this node starts to follow a new leader.
    // 当此节点开始跟随新的领导者时调用
    virtual void on_start_following(const ::braft::LeaderChangeContext& ctx);
...
};
```

## 18.7. 实现Snapshot

在braft中，Snapshot被定义为**在特定持久化存储中的文件集合**, 用户将状态机序列化到一个或者多个文件中， 并且任何节点都能从这些文件中恢复状态机到当时的状态.<font color='red'>就是文件</font>

Snapshot有两个作用:

- 启动加速， 启动阶段变为加载Snapshot和追加之后日志两个阶段， 而不需要重新执行历史上所有的操作.
- Log Compaction， 在完成Snapshot完成之后， 这个时间之前的日志都可以被删除了， 这样可以减少日志占用的资源.

在braft的中， 可以通过<font color='red'>SnapshotReader和SnapshotWriter</font>来控制访问相应的Snapshot.

```cpp
class Snapshot : public butil::Status {
public:
    Snapshot() {}
    virtual ~Snapshot() {}

    // Get the path of the Snapshot
    virtual std::string get_path() = 0;

    // List all the existing files in the Snapshot currently
    virtual void list_files(std::vector<std::string> *files) = 0;

    // Get the implementation-defined file_meta获取实现定义的file_meta
    virtual int get_file_meta(const std::string& filename, 
                              ::google::protobuf::Message* file_meta) {
        (void)filename;
        file_meta->Clear();
        return 0;
    }
};

class SnapshotWriter : public Snapshot {
public:
    SnapshotWriter() {}
    virtual ~SnapshotWriter() {}

    // Save the meta information of the snapshot which is used by the raft framework
    // 保存raft框架使用的快照的meta information
    virtual int save_meta(const SnapshotMeta& meta) = 0;

    // Add a file to the snapshot.
    // |file_meta| is an implmentation-defined protobuf message 
    // | file_meta |是一个implmentation定义的protobuf消息
    // All the implementation must handle the case that |file_meta| is NULL and
    // no error can be raised.
    // 所有实现必须处理file_meta| is NULL的情况，且没有异常可以抛出
    // Note that whether the file will be created onto the backing storage 
    // implementation-defined.
    // 请注意，是否将在后备存储上创建文件是isimplementation-defined.
    virtual int add_file(const std::string& filename) { 
        return add_file(filename, NULL);
    }

    virtual int add_file(const std::string& filename, 
                         const ::google::protobuf::Message* file_meta) = 0;

    // Remove a file from the snapshot
    // Note that whether the file will be removed from the backing storage is
    // implementation-defined.
    // 是否将从后备存储中删除该文件是 implementation-defined
    virtual int remove_file(const std::string& filename) = 0;
};

class SnapshotReader : public Snapshot {
public:
    SnapshotReader() {}
    virtual ~SnapshotReader() {}

    // Load meta from 
    virtual int load_meta(SnapshotMeta* meta) = 0;

    // Generate uri for other peers to copy this snapshot.为其他peer生成uri以复制此快照 
    // Return an empty string if some error has occcured
    // 如果发生了某些错误，则返回空字符串
    virtual std::string generate_uri_for_copy() = 0;
};
```

 不同业务的Snapshot千差万别，因为SnapshotStorage并没有抽象具体读写Snapshot的接口，而是抽象出SnapshotReader和SnapshotWriter，交由用户扩展具体的snapshot创建和加载逻辑。

**Snapshot创建流程**：

- SnapshotStorage::create创建一个临时的Snapshot，并返回一个SnapshotWriter
- SnapshotWriter将状态数据写入到临时snapshot中
- SnapshotStorage::close来将这个snapshot转为合法的snapshot

**Snapshot读取流程**：

- SnapshotStorage::open打开最近的一个Snapshot，并返回一个SnapshotReader
- SnapshotReader将状态数据从snapshot中恢复出来
- SnapshotStorage::close清理资源

libraft内提供了基于文件列表的LocalSnapshotWriter和LocalSnapshotReader默认实现，具体使用方式为：

- 在fsm的on_snapshot_save回调中，将状态数据写入到本地文件中，然后调用SnapshotWriter::add_file将相应文件加入snapshot meta。
- 在fsm的on_snapshot_load回调中，调用SnapshotReader::list_files获取本地文件列表，按照on_snapshot_save的方式进行解析，恢复状态数据。

实际情况下，用户业务状态机数据的snapshot有下面几种实现方式：

- 状态数据存储使用支持MVCC的存储引擎，创建snapshot之后，再异步迭代snapshot句柄将数据持久化
- 状态数据全内存且数据量不大，直接加锁将数据拷贝出来，再异步将数据持久化
- 定时启动一个离线线程，合并上一次的snapshot和最近的log，生成新的snapshot(需要业务fsm再持久化一份log，可以通过定制logstorage实现raft和fsm共享log)
- fork子进程，在子进程中遍历状态数据并进行持久化(多线程程序实现中需要避免死锁)

对于业界一些newsql系统，它们大都使用类rocksdb的lsm tree的存储引擎，支持MVCC。在进行raft snapshot的时候，使用上面的方案1，先创建一个db的snapshot，然后创建一个iterator，遍历并持久化数据。tidb、cockroachdb都是类似的解决方案。

## 18.8. 控制这个节点

braft::Node可以通过调用api控制也可以通过[braft_cli](./cli.md)来控制, 本章主要说明如何使用api.

### 18.8.1. 节点配置变更

在分布式系统中，机器故障，扩容，副本均衡是管理平面需要解决的基本问题，braft提供了几种方式:

* 增加一个节点
* 删除一个节点
* 全量替换现有节点列表

```cpp
// Add a new peer to the raft group. done->Run() would be invoked after this
// operation finishes, describing the detailed result.
void add_peer(const PeerId& peer, Closure* done);

// Remove the peer from the raft group. done->Run() would be invoked after
// this operation finishes, describing the detailed result.
void remove_peer(const PeerId& peer, Closure* done);

// Gracefully change the configuration of the raft group to |new_peers| , done->Run()
// would be invoked after this operation finishes, describing the detailed
// result.
// 优雅地将raft组的配置更改为| new_peers |
void change_peers(const Configuration& new_peers, Closure* done);
```

节点变更分为几个阶段:

- **追赶阶段**: 如果新的节点配置相对于当前有新增的一个或者多个节点，leader对应的Replicator, 向把最新的snapshot再这个这些中安装，然后开始同步之后的日志。等到所有的新节点数据都追的差不多，就开始进入一下一阶段。
  - 追赶是为了避免新加入的节点数据和集群相差过远而影响集群的可用性. 并不会影响数据安全性.
  - 在追赶阶段完成前， **只有**leader知道这些新节点的存在，这个节点都不会被记入到集群的决策集合中，包括选主和日志提交的判定。追赶阶段任意节点失败，则这次节点变更就会被标记为失败。
- **联合选举阶段**: leader会将旧节点配置和新节点配置写入Log, 在这个阶段之后直道下一个阶段之前，所有的选举和日志同步都需要在**新老节点之间达到多数**。 这里和标准算法有一点不同， 考虑到和之前实现的兼容性，如果这次只变更了一个节点,  则直接进入下一阶段。
- **新配置同步阶段:**  当联合选举日志正式被新旧集群接受之后，leader将新节点配置写入log，之后所有的log和选举只需要在新集群中达成一致。 等待日志提交到**新集群**中的多数节点中之后， 正式完全节点变更。
- **清理阶段**: leader会将多余的Replicator(如果有)关闭，特别如果当leader本身已经从节点配置中被移除，这时候leader会执行stepdown并且唤醒一个合适的节点触发选举。

> 当考虑节点删除的时候， 情况会变得有些复杂, 由于判断成功提交的节点数量变少， 可能会出现在前面的日志没有成功提交的情况下， 后面的日志已经被判断已经提交。 这时候为了状态机的操作有序性， 即使之前的日志还未提交， 我们也会强制判断为成功.
>
> 举个例子:
>
> - 当前集群为 (A, B, **C, D**), 其中**C D**属于故障， 由于多数节点处于故障阶段, 存在10条还未被提交的日志(A B 已经写入， **C D** 未写入), 这时候发起操作，将D从集群中删除， 这条日志的成功判定条件变为在(A, B, **C**)， 这时候只需要A, B都成功写入这条日志即可认为这个日志已经成功提交， 但是之前还存在10条未写入日志. 这时候我们会强制认为之前的10条已经成功提交.
> - 这个case比较极端， 通常这个情况下leader都会step down， 集群会进入无主状态， 需要至少修复CD中的一个节点之后集群才能正常提供服务。

### 18.8.2. 重置节点列表

<u>当多数节点故障的时候</u>，是不能通过add_peer/remove_peer/change_peers进行节点变更的，这个时候安全的做法是等待多数节点恢复，能够保证数据安全。如果业务追求服务的可用性，放弃数据安全性的话，可以使用reset_peers飞线设置复制组Configuration。

```cpp
// Reset the configuration of this node individually, without any repliation
// to other peers before this node beomes the leader. This function is
// supposed to be inovoked when the majority of the replication group are
// dead and you'd like to revive the service in the consideration of
// availability.
// Notice that neither consistency nor consensus are guaranteed in this
// case, BE CAREFULE when dealing with this method.
butil::Status reset_peers(const Configuration& new_peers);
```

reset_peer之后，新的Configuration的节点会开始重新选主，当新的leader选主成功之后，会写一条新Configuration的Log，这条Log写成功之后，reset_peer才算成功。如果中间又发生了失败的话，外部需要重新选取peers并发起reset_peers。

**不建议使用reset_peers**，reset_peers会破坏raft对数据一致性的保证，而且可能会造成脑裂。例如，{A B C D E}组成的复制组G，其中{C D E}故障，将{A B} set_peer成功恢复复制组G'，{C D E}又重新启动它们也会形成一个复制组G''，这样复制组G中会存在两个Leader，且{A B}这两个复制组中都存在，其中的follower会接收两个leader的AppendEntries，当前只检测term和index，可能会导致其上数据错乱。

```cpp
// Add a new peer to the raft group when the current configuration matches
// |old_peers|. done->Run() would be invoked after this operation finishes,
// describing the detailed result.
void add_peer(const std::vector<PeerId>& old_peers, const PeerId& peer, Closure* done);
```

### 18.8.3. 转移Leader

```
// Try transferring leadership to |peer|.
// If peer is ANY_PEER, a proper follower will be chosen as the leader the
// the next term.
// Returns 0 on success, -1 otherwise.
int transfer_leadership_to(const PeerId& peer);
```

在一些场景中，我们会需要外部强制将leader切换到另外的节点， 比如:

- 主节点要重启， 这时候发起一次主迁移能够减少集群的不可服务时间
- 主节点所在的机器过于繁忙， 我们需要迁移到另外一个相对空闲的机器中.
- 复制组跨IDC部署， 我们希望主节点存在于离Client延时最小的集群中.

braft实现了主迁移算法， 这个算法包含如下步骤:

1. 主停止写入， 这时候所有的apply会报错.
2. 继续向所有的follower同步日志， 当发现目标节点的日志已经和主一样多之后， 向对应节点发起一个TimeoutNow RPC
3. 节点收到TimeoutNowRequest之后，  直接变为Candidate, 增加term，并开始进入选主
4. 主收到TimeoutNowResponse之后， 开始step down.
5. 如果在election_timeout_ms时间内主没有step down， 会取消主迁移操作， 开始重新接受写入请求.

## 18.9. 查看节点状态

braft中在Node启动之后，会在http://${your_server_endpoint}/raft_stat中列出当前这个进程上Node的列表，及其每个Node的内部状态。

其中包括：

| 字段                   | 说明                                       |
| -------------------- | ---------------------------------------- |
| state                | 节点状态，包括LEADER/FOLLOWER/CANDIDATE         |
| term                 | 当前term                                   |
| conf_index           | 上一个Configuration产生的log index             |
| peers                | 当前Configuration中节点列表                     |
| leader               | 当前Configuration中Leader节点                 |
| election_timer       | 选主定时器，FOLLOWER状态下启用                      |
| vote_timer           | 投票定时器，CANDIDATE状态下启用                     |
| stepdown_timer       | 主切从定时器，LEADER状态下启用                       |
| snapshot_timer       | 快照定时器                                    |
| storage              | log storage中first log index和last log index |
| disk_index           | 持久化的最后一个log index                        |
| known_applied_index  | fsm已经apply的最后一个log index                 |
| last_log_id          | 最后一条内存log信息（log先写内存再批量刷disk）             |
| state_machine        | fsm状态，包括IDLE/COMMITTED/SNAPSHOT_SAVE/SNAPSHOT_LOAD/LEADER_STOP/ERROR |
| last_committed_index | 已经committed的最大log index                  |
| last_snapshot_index  | 上一次snapshot中包含的最后一条log index             |
| last_snapshot_term   | 上一次snapshot中包含的最后一条log的term              |
| snapshot_status      | snapshot状态，包括：LOADING/DOWNLOADING/SAVING/IDLE，其中LOADING和DOWNLOADING会显示snapshot uri和snapshot meta |

## 18.10. flags配置项

raft中有很多flags配置项，运行中可以通过http://endpoint/flags查看，具体如下：

| flags名                         | 说明                         |
| ------------------------------ | -------------------------- |
| raft_sync                      | 是否开启sync                   |
| raft_max_append_buffer_size    | log manager中内存缓存大小         |
| raft_leader_batch              | log manager中最大batch合并      |
| raft_max_entries_size          | AppendEntries包含entries最大数量 |
| raft_max_body_size             | AppendEntris最大body大小       |
| raft_max_segment_size          | 单个logsegment大小             |
| raft_max_byte_count_per_rpc    | snapshot每次rpc下载大小          |
| raft_apply_batch               | apply的时候最大batch数量          |
| raft_election_heartbeat_factor | election超时与heartbeat超时的比例  |

## 18.11. 汇总

### 18.11.1. 一个server的main过程

### 18.11.2. 需要封装的类
1. `class BlockClosure : public braft::Closure`

2. `class Block;`
`class Block : public braft::StateMachine`

3. `void BlockClosure::Run()`

4. `// Implements example::BlockService if you are using brpc.`
`class BlockServiceImpl : public BlockService`

#### 18.11.2.1. StateMachine
包括的函数：
1. start：start this node
2. write: 实现写方法
3. read
4. on_apply(braft::Iterator& iter)
5. static void *save_snapshot(void* arg) 