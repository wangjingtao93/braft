// Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Authors: Zhangyi Chen(chenzhangyi01@baidu.com)

#include "braft/cli_service.h"

#include <brpc/controller.h>       // brpc::Controller
#include "braft/node_manager.h"          // NodeManager
#include "braft/closure_helper.h"        // NewCallback

namespace braft {

static void add_peer_returned(brpc::Controller* cntl,
                          const AddPeerRequest* request,
                          AddPeerResponse* response,
                          std::vector<PeerId> old_peers,
                          scoped_refptr<NodeImpl> /*node*/,
                          ::google::protobuf::Closure* done,
                          const butil::Status& st) {
    brpc::ClosureGuard done_guard(done);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    bool already_exists = false;
    for (size_t i = 0; i < old_peers.size(); ++i) {
        response->add_old_peers(old_peers[i].to_string());
        response->add_new_peers(old_peers[i].to_string());
        if (old_peers[i] == request->peer_id()) {
            already_exists = true;
        }
    }
    if (!already_exists) {
        response->add_new_peers(request->peer_id());
    }
}

void CliServiceImpl::add_peer(::google::protobuf::RpcController* controller,
                              const ::braft::AddPeerRequest* request,
                              ::braft::AddPeerResponse* response,
                              ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;
    butil::Status st = get_node(&node, request->group_id(), request->leader_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    std::vector<PeerId> peers;
    st = node->list_peers(&peers);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    PeerId adding_peer;
    if (adding_peer.parse(request->peer_id()) != 0) {
        cntl->SetFailed(EINVAL, "Fail to parse peer_id %s",
                                request->peer_id().c_str());
        return;
    }
    LOG(WARNING) << "Receive AddPeerRequest to " << node->node_id() 
                 << " from " << cntl->remote_side()
                 << ", adding " << request->peer_id();
    Closure* add_peer_done = NewCallback(
            add_peer_returned, cntl, request, response, peers, node,
            done_guard.release());
    return node->add_peer(adding_peer, add_peer_done);
}

static void remove_peer_returned(brpc::Controller* cntl,
                          const RemovePeerRequest* request,
                          RemovePeerResponse* response,
                          std::vector<PeerId> old_peers,
                          scoped_refptr<NodeImpl> /*node*/,
                          ::google::protobuf::Closure* done,
                          const butil::Status& st) {
    brpc::ClosureGuard done_guard(done);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    for (size_t i = 0; i < old_peers.size(); ++i) {
        response->add_old_peers(old_peers[i].to_string());
        if (old_peers[i] != request->peer_id()) {
            response->add_new_peers(old_peers[i].to_string());
        }
    }
}

void CliServiceImpl::remove_peer(::google::protobuf::RpcController* controller,
                                 const ::braft::RemovePeerRequest* request,
                                 ::braft::RemovePeerResponse* response,
                                 ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;
    butil::Status st = get_node(&node, request->group_id(), request->leader_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    std::vector<PeerId> peers;
    st = node->list_peers(&peers);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    PeerId removing_peer;
    if (removing_peer.parse(request->peer_id()) != 0) {
        cntl->SetFailed(EINVAL, "Fail to parse peer_id %s",
                                request->peer_id().c_str());
        return;
    }
    LOG(WARNING) << "Receive RemovePeerRequest to " << node->node_id() 
                 << " from " << cntl->remote_side()
                 << ", removing " << request->peer_id();
    Closure* remove_peer_done = NewCallback(
            remove_peer_returned, cntl, request, response, peers, node,
            done_guard.release());
    return node->remove_peer(removing_peer, remove_peer_done);
}

void CliServiceImpl::reset_peer(::google::protobuf::RpcController* controller,
                                const ::braft::ResetPeerRequest* request,
                                ::braft::ResetPeerResponse* response,
                                ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;
    butil::Status st = get_node(&node, request->group_id(), request->peer_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    Configuration new_peers;
    for (int i = 0; i < request->new_peers_size(); ++i) {
        PeerId peer;
        if (peer.parse(request->new_peers(i)) != 0) {
            cntl->SetFailed(EINVAL, "Fail to parse %s",
                                    request->new_peers(i).c_str());
            return;
        }
        new_peers.add_peer(peer);
    }
    LOG(WARNING) << "Receive set_peer to " << node->node_id()
                 << " from " << cntl->remote_side();
    st = node->reset_peers(new_peers);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
    }
}

static void snapshot_returned(brpc::Controller* cntl,
                              scoped_refptr<NodeImpl> node,
                              ::google::protobuf::Closure* done,
                              const butil::Status& st) {
    brpc::ClosureGuard done_guard(done);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
    }
}

void CliServiceImpl::snapshot(::google::protobuf::RpcController* controller,
                              const ::braft::SnapshotRequest* request,
                              ::braft::SnapshotResponse* response,
                              ::google::protobuf::Closure* done) {

    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;
    butil::Status st = get_node(&node, request->group_id(), request->peer_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    Closure* snapshot_done = NewCallback(snapshot_returned, cntl, node,
                                         done_guard.release());
    return node->snapshot(snapshot_done);
}

void CliServiceImpl::get_leader(::google::protobuf::RpcController* controller,
                                const ::braft::GetLeaderRequest* request,
                                ::braft::GetLeaderResponse* response,
                                ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    std::vector<scoped_refptr<NodeImpl> > nodes;
    NodeManager* const nm = NodeManager::GetInstance();
    if (request->has_peer_id()) {
        PeerId peer;
        if (peer.parse(request->peer_id()) != 0) {
            cntl->SetFailed(EINVAL, "Fail to parse %s",
                                    request->peer_id().c_str());
            return;
        }
        scoped_refptr<NodeImpl> node = nm->get(request->group_id(), peer);
        if (node) {
            nodes.push_back(node);
        }
    } else {
        nm->get_nodes_by_group_id(request->group_id(), &nodes);
    }
    if (nodes.empty()) {
        cntl->SetFailed(ENOENT, "No nodes in group %s",
                                request->group_id().c_str());
        return;
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
        PeerId leader_id = nodes[i]->leader_id();
        if (!leader_id.is_empty()) {
            response->set_leader_id(leader_id.to_string());
            return;
        }
    }
    cntl->SetFailed(EAGAIN, "Unknown leader");
}
/********************************************************************
*Function:       
*Description:     这个函数经常用到,根据BG_name（group_id：00000...）和BD(peer_id:127.0.0.1:10001:0)，从map表里取出这个node(这个node具体包含什么信息，不知道),
                    或者说给这个*node赋值
                 1. 需要判断BG（group_id）是否在这个BD(127.0.0.1:10001:0)上。因为BG是三副本，可能分布在不同的BD上，
                 2. 判断此node是否可access（权限）
*Calls:          
                
*Table Accessed: 
*Table Updated: 
*Input:          1. scoped_refptr<NodeImpl>* node
                 2.  const GroupId& group_id（BG_name）
                 3. const std::string& peer_id,传入的是request->leader_id(127.0.0.1:10001:0)      

*Output:         scoped_refptr<NodeImpl>* node       
*Return:         butil::Status
*Others:        
*********************************************************************/
butil::Status CliServiceImpl::get_node(scoped_refptr<NodeImpl>* node,
                                      const GroupId& group_id,
                                      const std::string& peer_id) {
    NodeManager* const nm = NodeManager::GetInstance();
    if (!peer_id.empty()) {
        *node = nm->get(group_id, peer_id);//这个地方，给*node赋值
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

    //判断此node是否可access（权限）
    if ((*node)->disable_cli()) {
        return butil::Status(EACCES, "CliService is not allowed to access node "
                                    "%s", (*node)->node_id().to_string().c_str());
    }

    return butil::Status::OK();
}

static void change_peers_returned(brpc::Controller* cntl,
                          const ChangePeersRequest* request,
                          ChangePeersResponse* response,
                          std::vector<PeerId> old_peers,
                          Configuration new_peers,
                          scoped_refptr<NodeImpl> /*node*/,
                          ::google::protobuf::Closure* done,
                          const butil::Status& st) {
    brpc::ClosureGuard done_guard(done);
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    for (size_t i = 0; i < old_peers.size(); ++i) {
        response->add_old_peers(old_peers[i].to_string());
    }
    for (Configuration::const_iterator
            iter = new_peers.begin(); iter != new_peers.end(); ++iter) {
        response->add_new_peers(iter->to_string());
    }
}
/********************************************************************
*Function:       
*Description:    server端入口，处理client的rpc请求
*Calls:          butil::Status CliServiceImpl::get_node(scoped_refptr<NodeImpl>* node,
                                      const GroupId& group_id,
                                      const std::string& peer_id)
                 node.cpp, butil::Status NodeImpl::list_peers(std::vector<PeerId>* peers)
                 configuration.h, bool add_peer(const PeerId& peer),
                 node.cpp, void NodeImpl::change_peers(const Configuration& new_peers, Closure* done) 
                
*Table Accessed: 
*Table Updated: 
*Input:         ::google::protobuf::RpcController* controller
                const ::braft::ChangePeersRequest* request
                ::braft::ChangePeersResponse* response 
                ::google::protobuf::Closure* done                     
*Output:         
*Return:        node->change_peers(conf, change_peers_done) 
*Others:        
*********************************************************************/
void CliServiceImpl::change_peers(::google::protobuf::RpcController* controller,
                                  const ::braft::ChangePeersRequest* request,
                                  ::braft::ChangePeersResponse* response,
                                  ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;

    //从map里取出node
    butil::Status st = get_node(&node, request->group_id(), request->leader_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }

    //整出来old_peers，给NewCallback（）使用
    std::vector<PeerId> old_peers;
    st = node->list_peers(&old_peers);//这个是此BG原先所在的BD的 ip：port：0
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }

     //一组peers时
    Configuration conf;//Configuration这个类有些方法比如add_peer change_peer等
    for (int i = 0; i < request->new_peers_size(); ++i) {
        PeerId peer;
        if (peer.parse(request->new_peers(i)) != 0) {
            //经常会用到，像是判断peers格式是否有正确：ip:port+index
            cntl->SetFailed(EINVAL, "Fail to parse %s",
                                    request->new_peers(i).c_str());
            return;
        }

        // Add a peer，像是要向一个内存中的set容器即_peer中插入这条ip记录，至于什么作用，不清楚
        // new_peers有两个element,测试时元素就是两个IP+prort，一个是BG_to_Remove(127.0.0.1:10001)
        //一个是BG_to_add(127.0.0.1:10003)
        conf.add_peer(peer);
    }

    // 异步，
    // We use protobuf utility（有效的） `NewCallback' to create a closure（关闭，终止，结束） object
    // that will call our callback `HandleEchoResponse'. This closure
    // will automatically delete itself after being called once
    Closure* change_peers_done = NewCallback(
            change_peers_returned, 
            cntl, request, response, old_peers, conf, node,
            done_guard.release());

     //configuration chanage，入参：实例conf，指针change_peers_done，这俩入参牛逼
    return node->change_peers(conf, change_peers_done);
}

void CliServiceImpl::transfer_leader(
                    ::google::protobuf::RpcController* controller,
                    const ::braft::TransferLeaderRequest* request,
                    ::braft::TransferLeaderResponse* response,
                    ::google::protobuf::Closure* done) {
    brpc::Controller* cntl = (brpc::Controller*)controller;
    brpc::ClosureGuard done_guard(done);
    scoped_refptr<NodeImpl> node;
    butil::Status st = get_node(&node, request->group_id(), request->leader_id());
    if (!st.ok()) {
        cntl->SetFailed(st.error_code(), "%s", st.error_cstr());
        return;
    }
    PeerId peer = ANY_PEER;
    if (request->has_peer_id() && peer.parse(request->peer_id()) != 0) {
        cntl->SetFailed(EINVAL, "Fail to parse %s", request->peer_id().c_str());
        return;
    }
    const int rc = node->transfer_leadership_to(peer);
    if (rc != 0) {
        cntl->SetFailed(rc, "Fail to invoke transfer_leadership_to : %s",
                            berror(rc));
        return;
    }
}

}  //  namespace braft
