//
// Created by root on 2019/8/22.
//

#ifndef RAPTOR_SERIMPL_H
#define RAPTOR_SERIMPL_H

#include <gflags/gflags.h>   // DEFINE_*
#include <brpc/controller.h> // brpc::Controller
#include <brpc/server.h>     // brpc::Server

#include "block.pb.h" // BlockService
#include "raftserver.h"

// Implements example::BlockService if you are using brpc
namespace example
{

class Block;

class BlockServiceImpl : public BlockService
{
public:
    explicit BlockServiceImpl(Block *block);

    void write(::google::protobuf::RpcController *controller,
               const ::example::BlockRequest *request,
               ::example::BlockResponse *response,
               ::google::protobuf::Closure *done);

    void read(::google::protobuf::RpcController *controller,
              const ::example::BlockRequest *request,
              ::example::BlockResponse *response,
              ::google::protobuf::Closure *done);

private:
    Block *_block;
};

} // namespace example

#endif //RAPTOR_SERIMPL_H
