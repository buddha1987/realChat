#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class ChatServer : boost::noncopyable
{
public:
    ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
    : server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
    {
        server_.setConnectionCallback(
            boost::bind(&ChatServer::onConnection, this, _1));
        server_.setMessageCallback(
            boost::bind(&ChatServer::onMessage, this, _1, _2, _3));
    }

    void start()
    {
    server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");
        
        if (conn->connected())
        {
            connections_.insert(conn);
            string welcomeMsg("You are online!");
            codec_.send(get_pointer(conn), welcomeMsg);   
        }
        else
        {
            connections_.erase(conn);
        }
    }

    void onMessage(const TcpConnectionPtr& conn,
                       Buffer* buf,
                       Timestamp receiveTime)
    {
        while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
        {
            int32_t msgLength = 0; 
            codec_.decode(buf, msgLength);
            if (msgLength > 65536 || msgLength < 0)
            {
                LOG_ERROR << "Invalid length " << msgLength;
                conn->shutdown();  // FIXME: disable reading
                break;
            }
            if (buf->readableBytes() >= msgLength + kHeaderLen)
            {
                buf->retrieve(kHeaderLen);
                muduo::string message(buf->peek(), msgLength);
                boardCastMessage(message);
                buf->retrieve(msgLength);
            }
            else
            {
                break;
            }
        }
    }

    void boardCastMessage(const string& message)
    {
        for (ConnectionList::iterator it = connections_.begin();
            it != connections_.end();
            ++it)
        {
            codec_.send(get_pointer(*it), message);
        }
    }

    void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
    {
    for (ConnectionList::iterator it = connections_.begin();
        it != connections_.end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
    }

    typedef std::set<TcpConnectionPtr> ConnectionList;
    TcpServer server_;
    LengthHeaderCodec codec_;
    ConnectionList connections_;
    const static size_t kHeaderLen = LengthHeaderCodec::kHeaderLen;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port\n", argv[0]);
  }
}

