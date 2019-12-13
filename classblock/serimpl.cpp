//
// Created by root on 2019/8/22.
//
#include <iostream>
#include "serimpl.h"

namespace example
{
    BlockServiceImpl::BlockServiceImpl(Block *block): _block(block) {}

    
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
void BlockServiceImpl::write(::google::protobuf::RpcController *controller,
                             const ::example::BlockRequest *request,
                             ::example::BlockResponse *response,
                             ::google::protobuf::Closure *done)
{
    brpc::Controller *cntl = (brpc::Controller *)controller;
    return _block->write(request, response,
                         &cntl->request_attachment(), done);
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
void BlockServiceImpl::read(::google::protobuf::RpcController *controller,
                            const ::example::BlockRequest *request,
                            ::example::BlockResponse *response,
                            ::google::protobuf::Closure *done)
{
    brpc::Controller *cntl = (brpc::Controller *)controller;
    brpc::ClosureGuard done_guard(done);
    return _block->read(request, response, &cntl->response_attachment());
}

} // namespace example