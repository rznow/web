# MiniForum

> 基于 **C++17 从零实现的高性能论坛系统**
>
> Linux + Socket + Epoll ET + Main-Reactor/Sub-Reactor + ThreadPool + MySQL + Redis + JWT
>
> 不依赖现成 Web 框架，自主实现 HTTP Server、Reactor 网络模型、HTTP 请求解析、连接池、线程池以及缓存系统。

---

## 📖 项目简介

**MiniForum** 是一个使用 C++17 从零实现的轻量级高并发论坛系统。

项目没有使用现成的 Web Server 或 Web 框架，而是基于 Linux Socket 和 Epoll 自主实现网络层，并采用：

```text
Main Reactor
     │
     ├── Acceptor
     │
     ▼
Sub Reactor
     │
     ├── Connection
     │
     ├── HTTP Read / Write
     │
     ▼
ThreadPool
     │
     ├── HttpServer
     ├── PostService
     ├── RedisService
     └── MySQL
```

通过项目实践了：

* Linux Socket 网络编程
* Epoll ET 非阻塞 IO
* Main-Reactor / Sub-Reactor
* C++ ThreadPool
* HTTP/1.1 协议解析
* MySQL Prepared Statement
* MySQL Connection Pool
* Redis Connection Pool
* Redis Cache Aside
* Redis 高速计数
* JWT 身份认证
* 评论树构建
* 高并发压力测试
* 多线程并发安全

---

# ✨ Features

## 👤 用户系统

* [x] 用户注册
* [x] 用户登录
* [x] JWT Token 身份认证
* [x] 用户信息查询
* [x] 用户资料
* [x] 用户头像上传
* [x] 用户头像缓存
* [x] 在线用户状态维护
* [x] 用户数据统计

用户统计包括：

```text
帖子数量
评论数量
获赞数量
注册时间
```

---

## 📝 帖子系统

* [x] 发布帖子
* [x] 修改帖子
* [x] 删除帖子
* [x] 帖子详情
* [x] 帖子分页
* [x] 作者信息
* [x] 点赞数量
* [x] 评论数量
* [x] 浏览量统计
* [x] Redis 帖子缓存

帖子缓存采用 Redis Hash：

```text
post:{post_id}

title
content
author
user_id
like
comment
view
create_time
```

---

## 💬 评论系统

支持：

* [x] 一级评论
* [x] 多级回复
* [x] 评论分页
* [x] 评论数量统计
* [x] 评论缓存
* [x] 评论树构建
* [x] 评论作者信息
* [x] 回复用户信息
* [x] 评论头像缓存

评论数据：

```text
comment:{comment_id}

id
post_id
user_id
parent_id
root_comment_id
reply_user_id
author
reply_author
content
create_time
avatar
```

评论索引：

```text
comment:index:{post_id} 存放根评论索引
comment:children:{comment_id} 存放单一级子评论索引
```

使用 Redis 保存评论索引，并通过 Hash 保存评论实体数据。

评论树构建采用 **深层递归**。

---

# ❤️ 点赞系统

* [x] 帖子点赞
* [x] 取消点赞
* [x] 点赞状态查询
* [x] 点赞数量统计
* [x] Redis 高速计数
* [x] 后台同步 MySQL

点赞数量等高频变化数据优先在 Redis 中处理。

---

# 👀 浏览量统计

针对帖子浏览量进行了缓存优化。

采用：

```text
User + Post + 30s
```

的限制策略。

同一用户在一定时间窗口内重复访问同一帖子，不重复增加浏览量。

Redis 负责高频计数，后台线程定期将 Redis 中的脏数据同步到 MySQL。

整体流程：

```text
HTTP Request
     │
     ▼
Redis View Count
     │
     ▼
Dirty Post Set
     │
     ▼
Background Flush Thread
     │
     ▼
MySQL
```

---

# 🏗 系统架构

```text
                         Client
                           │
                          HTTP
                           │
                           ▼
                  ┌─────────────────┐
                  │  Main Reactor   │
                  └────────┬────────┘
                           │
                        Acceptor
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
      ┌───────────────┐         ┌───────────────┐
      │ Sub Reactor 1 │         │ Sub Reactor N │
      └───────┬───────┘         └───────┬───────┘
              │                         │
         Connection                 Connection
              │                         │
              └────────────┬────────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │ ThreadPool  │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │ HttpServer  │
                    └──────┬──────┘
                           │
                ┌──────────┴──────────┐
                │                     │
                ▼                     ▼
             Redis                  MySQL
```

---

# ⚡ 网络模型

## Main-Reactor

Main Reactor 主要负责：

* 创建监听 Socket
* 设置 Socket 非阻塞
* `bind`
* `listen`
* `accept4`
* 新连接建立
* Connection 分发

---

## Sub-Reactor

Sub Reactor 负责：

* `epoll_wait`
* Socket Read
* Socket Write
* Connection 生命周期
* EPOLLIN
* EPOLLOUT
* ET 模式事件处理

多个 Sub-Reactor 可以并行处理不同连接。

---

## ThreadPool

网络线程主要负责 IO，不直接执行耗时业务逻辑。

请求处理流程：

```text
epoll
  │
  ▼
Connection::handleRead()
  │
  ▼
HTTP Request Parse
  │
  ▼
ThreadPool::enqueue()
  │
  ▼
HttpServer
  │
  ├── Redis
  └── MySQL
```

业务处理完成后：

```text
ThreadPool
    │
    ▼
Response Queue
    │
    ▼
eventfd
    │
    ▼
Sub-Reactor
    │
    ▼
EPOLLOUT
    │
    ▼
Connection::handleWrite()
```

使用 `eventfd` 将业务线程产生的 Response 任务通知 Reactor，避免业务线程直接操作 Epoll。

---

# 🌐 HTTP Server

自主实现 HTTP/1.1 Server。

支持：

* HTTP/1.1
* Keep-Alive
* GET
* POST
* JSON
* multipart/form-data
* 文件上传
* 非阻塞 Socket
* HTTP Request Parser
* HTTP Response
* 多请求缓冲处理

请求处理：

```text
recv/read
   │
   ▼
InputBuffer
   │
   ▼
HttpRequest
   │
   ▼
ThreadPool
   │
   ▼
HttpServer
   │
   ▼
HttpResponse
   │
   ▼
OutputBuffer
   │
   ▼
write/send
```

---

# 📦 Buffer 设计

HTTP 数据通过 Buffer 进行管理。

主要处理：

* 半包
* 粘包
* 多个 HTTP 请求
* HTTP Header 完整性
* Request Body

例如：

```text
第一次 read

GET /post HTTP/1.1
Host: xxx


第二次 read

Content-Length: 10

hello.....
```

数据不会因为一次 `read()` 没有拿到完整请求而直接丢弃，而是持续保存在输入 Buffer 中。

---

# 🗄 MySQL

MySQL 用于持久化核心业务数据：

```text
users
posts
comments
post_like
...
```

数据库访问采用：

* Prepared Statement
* MySQL Connection Pool
* RAII 管理数据库资源

Connection Pool：

```text
Thread
  │
  ▼
MySQLPool
  │
  ├── Connection
  ├── Connection
  ├── Connection
  └── Connection
```

避免每次 HTTP 请求都重新建立 MySQL TCP 连接。

---

# 🔥 Redis

Redis 主要负责高频访问数据和临时状态。

使用：

* Hash
* ZSet
* Set
* Pipeline
* Connection Pool

主要缓存：

```text
帖子
评论
用户信息
用户统计
点赞数量
浏览量
评论索引
在线用户
```

---

# 🚀 Redis Cache Aside

核心缓存策略采用 **Cache Aside Pattern**。

读取：

```text
             Request
                │
                ▼
             Redis
                │
        ┌───────┴───────┐
        │               │
       Hit             Miss
        │               │
        ▼               ▼
      Return          MySQL
                        │
                        ▼
                    Redis Cache
                        │
                        ▼
                      Return
```

更新：

```text
Update MySQL
     │
     ▼
Delete Redis Cache
     │
     ▼
Next Request
     │
     ▼
Reload Redis
```

避免更新数据库后旧数据长期存在于缓存中。

---

# 📌 Redis 数据结构

## Post

```text
post:{id}
```

Redis Hash：

```text
title
content
author
user_id
like_count
comment_count
view_count
```

---

## Comment

```text
comment:{id}
```

Redis Hash：

```text
comment_id
post_id
user_id
parent_id
root_comment_id
reply_user_id
author
reply_author
avatar
content
create_time
```

---

## Comment Index

```text
post:{id}:comments
```

使用 Redis ZSet 保存评论顺序。

例如：

```text
score = create_time
member = comment_id
```

查询时可以直接按照时间获取评论 ID，再批量读取评论实体。

---

## User

```text
user:{id}
```

保存：

```text
user_id
user_name
avatar
```

用户头像已经和相关业务数据一起进行 Redis 缓存，减少高并发场景下重复查询用户表。

---

# 🔄 Redis 后台同步

对于浏览量、点赞数、评论数等高频变化数据，不立即频繁更新 MySQL。

采用：

```text
Redis Counter
      │
      ▼
Dirty Set
      │
      ▼
Background Thread
      │
      ▼
MySQL
```

例如：

```text
post_id = 7

Redis:
view_count = 1631772

Dirty Set:
7
```

后台线程定期：

```text
SPop Dirty Post
       │
       ▼
读取 Redis Counter
       │
       ▼
UPDATE posts
       │
       ▼
MySQL
```

这样可以减少大量高频 `UPDATE` 对 MySQL 的压力。

---

# 👥 在线用户

使用 Redis 维护在线用户状态。

用户访问需要认证的接口时：

```text
JWT Verify
    │
    ▼
updateOnline(user_id)
    │
    ▼
Redis
```

后台线程定期清理超时用户。

---

# 🧵 并发安全

项目涉及多个线程：

```text
Main Reactor
Sub Reactor
ThreadPool Workers
Flush Thread
Online User Thread
```

因此对跨线程数据访问进行了同步设计。

主要使用：

* `std::mutex`
* `std::unique_lock`
* `std::lock_guard`
* `std::condition_variable`
* `std::atomic`
* RAII
* Thread-safe Queue
* eventfd

ThreadPool 基于：

```cpp
std::mutex
std::condition_variable
std::queue<std::function<void()>>
```

实现任务提交和 Worker 唤醒。

---

# 📊 性能测试

压力测试使用：

```text
wrk
```

测试命令：

```bash
make bench \
    THREADS=4 \
    CONNS=200 \
    DURATION=30s \
    PATH_URL="/post?id=1"
```

---

## Post 接口

测试：

```bash
wrk -t4 -c200 -d30s \
http://192.168.1.8:8080/post?id=1
```

测试结果：

```text
Requests/sec:  11179.57
Latency:       18.03ms
```

---

## Comment 接口

测试：

```bash
wrk -t4 -c200 -d30s \
"http://192.168.1.8:8080/posts/7/comments?page=1&size=10"
```

一次测试结果：

```text
Requests/sec: 3075.12
Latency:      65.28ms
```

评论接口包含：

```text
Redis Index
Comment Cache
Comment Tree
User Information
JSON Serialization
```

因此相比简单帖子查询具有更高的业务复杂度。

---

## 不同并发连接数

评论接口测试：

| Connections |   QPS | Avg Latency |
| ----------: | ----: | ----------: |
|          10 | ~1127 |      7.26ms |
|          20 | ~1126 |     18.28ms |
|          30 | ~1131 |     25.04ms |
|          40 | ~1324 |     30.42ms |
|          50 | ~1180 |     41.14ms |
|         100 | ~2458 |     40.72ms |
|         200 | ~3075 |     65.28ms |

可以观察到：

```text
并发增加
   │
   ▼
吞吐量增加
   │
   ▼
ThreadPool / Redis / MySQL
逐渐成为瓶颈
   │
   ▼
Latency 上升
```

后续优化重点应该放在 **业务层等待时间和数据库/Redis访问**，而不是单纯增加 Worker 数量。

---

# 🔍 性能分析

项目中对请求处理链路进行了纳秒级耗时统计：

```text
HTTP Request Parse
        │
        ▼
ThreadPool Queue Wait
        │
        ▼
HttpServer
        │
        ▼
Response Queue
        │
        ▼
Socket Write
```

典型情况下：

```text
Request Parse     ≈ μs
Response Queue    ≈ μs
Socket Write      ≈ 10~100 μs
```

而在高并发评论查询中，ThreadPool Queue Wait 会明显增加。

因此性能瓶颈主要不是：

```text
HTTP Parser
Socket Write
JSON
```

而更多来自：

```text
ThreadPool Queue
     │
     ▼
Redis / MySQL
     │
     ▼
业务处理
```

这也是后续优化的重点。

---

# 🧪 性能优化方向

目前已经进行：

* [x] Redis Cache
* [x] Redis Counter
* [x] Redis Comment Index
* [x] Redis User Cache
* [x] MySQL Connection Pool
* [x] Redis Connection Pool
* [x] ThreadPool
* [x] Main/Sub Reactor
* [x] eventfd
* [x] Keep-Alive
* [x] 评论缓存
* [x] 头像缓存
* [x] 后台 Redis → MySQL 同步
* [x] Redis Pipeline

后续可以继续：

* [ ] MySQL 批量查询(进行中)
* [ ] 减少 ThreadPool Queue 等待
* [ ] 数据库索引优化
* [ ] 热点帖子缓存
* [ ] JSON 序列化优化
* [ ] 异步日志
* [ ] 性能监控

---

# 📂 项目结构

```text
MiniForum
│
├── include
│   ├── common
│   ├── http
│   ├── mysql
│   ├── network
│   ├── redis
│   ├── service
│   ├── thread
│   └── util
│
├── src
│   ├── Acceptor.cpp
│   ├── Reactor.cpp
│   ├── Connection.cpp
│   ├── HttpRequest.cpp
│   ├── HttpResponse.cpp
│   ├── HttpServer.cpp
│   ├── PostService.cpp
│   ├── RedisService.cpp
│   ├── MySQL.cpp
│   └── ...
│
├── www
│   ├── css
│   ├── images
│   ├── html
│   ├── js
│   └── upload
│
├── workbench
├── build
├── server.conf
├── Makefile
└── README.md
```

---

# ⚙️ 配置

示例：

```ini
# server
port=8080
thread_num=8
reactor_num=4
max_events=1000

# mysql
mysql_host=127.0.0.1
mysql_user=webserver
mysql_password=******
mysql_database=miniforum
mysql_port=3306
mysql_pool_size=10

# redis
redis_host=127.0.0.1
redis_port=6379
redis_pool_size=10
```

---

# 🛠 编译运行

## 环境

```text
Linux
GCC / G++ >= 11
C++17
MySQL
Redis
Make
```

---

## 编译

```bash
make
```

---

## 运行

```bash
make run
```

或者：

```bash
./server
```

---

## 单元 / Workbench 测试

```bash
make test
```

---

## 压力测试

```bash
make bench \
    THREADS=4 \
    CONNS=200 \
    DURATION=30s \
    PATH_URL="/post?id=1"
```

---

# 🧩 已完成

## 网络层

* [x] TCP Server
* [x] Linux Socket
* [x] Non-blocking Socket
* [x] Epoll
* [x] Epoll ET
* [x] Main Reactor
* [x] Sub Reactor
* [x] Acceptor
* [x] Connection
* [x] eventfd
* [x] Keep-Alive
* [x] EPOLLIN / EPOLLOUT

## HTTP

* [x] HTTP/1.1
* [x] Request Parser
* [x] Response
* [x] GET
* [x] POST
* [x] JSON
* [x] multipart/form-data
* [x] 文件上传
* [x] Buffer
* [x] 粘包 / 半包处理

## 并发

* [x] ThreadPool
* [x] Mutex
* [x] Condition Variable
* [x] Atomic
* [x] RAII
* [x] 跨线程 Response Queue

## 数据库

* [x] MySQL
* [x] Prepared Statement
* [x] MySQL Connection Pool
* [x] 数据库索引优化

## Redis

* [x] Redis
* [x] Redis Connection Pool
* [x] Hash
* [x] Set
* [x] ZSet
* [x] Cache Aside
* [x] Counter
* [x] Comment Index
* [x] User Cache
* [x] Background Flush
* [x] Redis Pipeline

## 业务

* [x] 用户注册
* [x] 用户登录
* [x] JWT
* [x] 用户资料
* [x] 头像上传
* [x] 帖子
* [x] 评论
* [x] 评论回复
* [x] 点赞
* [x] 浏览量
* [x] 分页
* [x] 缓存

---

# 🚧 RoadMap

## 工程化

* [ ] Async Logger
* [ ] Config Manager
* [ ] Docker
* [ ] Nginx
* [ ] HTTPS
* [ ] CI/CD

## 性能

* [ ] MySQL Batch Query
* [ ] 异步日志
* [ ] Prometheus
* [ ] Flame Graph
* [ ] CPU / IO / Lock profiling
* [ ] 更完善的 Benchmark

## 功能

* [ ] 帖子搜索
* [X] 用户主页
* [ ] 消息通知
* [ ] WebSocket
* [ ] Elasticsearch

## AI

* [ ] AI 帖子摘要
* [ ] AI 内容审核
* [ ] RAG 搜索
* [ ] AI Agent

---

# 🧠 项目难点

## 1. Epoll ET + 非阻塞 IO

ET 模式下必须持续读取 Socket：

```cpp
while (true)
{
    int n = read(fd, buffer, sizeof(buffer));

    if (n > 0)
    {
        inputbuffer.append(buffer, n);
    }
    else if (n == 0)
    {
        // connection closed
        break;
    }
    else if (errno == EAGAIN ||
             errno == EWOULDBLOCK)
    {
        break;
    }
}
```

否则可能因为没有读取干净而导致后续事件无法再次触发。

---

## 2. ThreadPool 与 Reactor 解耦

IO 线程不直接执行数据库和 Redis 等耗时操作：

```text
Reactor
   │
   │ enqueue
   ▼
ThreadPool
   │
   ▼
Business
   │
   │ response
   ▼
eventfd
   │
   ▼
Reactor
   │
   ▼
EPOLLOUT
```

实现 IO 与业务处理的解耦。

---

## 3. 高频数据缓存

浏览量、点赞数等数据如果每次请求都直接更新 MySQL：

```text
10000 Requests
      │
      ▼
10000 UPDATE
      │
      ▼
MySQL
```

会造成数据库压力。

因此改为：

```text
10000 Requests
      │
      ▼
Redis Counter
      │
      ▼
1 次 / 批量 UPDATE
      │
      ▼
MySQL
```

减少数据库写压力。

---

## 4. 评论树构建

评论不仅是简单的列表：

```text
Comment
├── Reply
│   ├── Reply
│   └── Reply
└── Reply
```

---

# ⭐ 项目亮点

### 1. 从零实现网络层

没有使用：

```text
Nginx
Boost.Asio
Muduo
Crow
Drogon
```

而是自己实现：

```text
Socket
Epoll
Reactor
Connection
HTTP Parser
HTTP Response
ThreadPool
```

---

### 2. Main-Reactor + Sub-Reactor

将连接建立与 IO 处理分离：

```text
Main Reactor
      │
      ▼
   Acceptor
      │
      ▼
Sub Reactor
      │
      ▼
 Connection
```

能够充分利用多核 CPU 并行处理连接 IO。

---

### 3. Redis + MySQL

使用 Redis 处理高频访问和计数：

```text
Redis
 │
 ├── Cache
 ├── Counter
 ├── Index
 └── Online State
```

MySQL 负责最终数据持久化。

---

### 4. Cache Aside

通过：

```text
Redis Hit
Redis Miss
MySQL
Cache Reload
Cache Invalidation
```

完整实践缓存系统中的 Cache Aside Pattern。

---

### 5. 后台数据同步

将高频变化数据：

```text
Redis
  ↓
Dirty Set
  ↓
Background Thread
  ↓
MySQL
```

降低数据库写入压力。

---

### 6. 压力测试与性能分析

使用 `wrk` 对接口进行实际压力测试，并通过纳秒级耗时统计分析：

```text
Request Parse
Queue Wait
Business
Response Queue
Socket Write
```

定位系统性能瓶颈，而不是只关注最终 QPS。

---

# 📚 技术收获

通过 MiniForum，深入实践了：

### C++

* C++11/14/17
* RAII
* Smart Pointer
* Move Semantics
* Lambda
* STL
* `std::thread`
* Mutex
* Condition Variable
* Atomic

### Linux

* Linux Socket
* TCP/IP
* Non-blocking IO
* Epoll
* ET
* eventfd
* 多线程
* IO 多路复用

### 后端

* HTTP/1.1
* Reactor
* ThreadPool
* Connection Pool
* Cache Aside
* Redis
* MySQL
* JWT
* 高并发处理

### 性能

* wrk
* QPS
* Latency
* ThreadPool Queue
* Redis Cache
* MySQL Optimization
* 性能瓶颈分析

---

# 📈 后续目标

项目后续重点不再单纯增加业务功能，而是逐步向更加完整的后端工程演进：

```text
MiniForum
    │
    ├── Network
    │     ├── Reactor
    │     ├── Epoll
    │     └── HTTP
    │
    ├── Concurrency
    │     ├── ThreadPool
    │     └── Async Task
    │
    ├── Storage
    │     ├── MySQL
    │     └── Redis
    │
    ├── Performance
    │     ├── Benchmark
    │     ├── Profiling
    │     └── Monitoring
    │
    └── Engineering
          ├── Logger
          ├── Docker
          ├── Nginx
          └── HTTPS
```

# 📄 License

MIT License

仅用于学习、交流与技术实践。
