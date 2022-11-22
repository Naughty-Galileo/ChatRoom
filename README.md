# ChatRoom
> 用poll实现简单的聊天室程序 \
> 

## server
- 编译
```bash
g++ server.cpp -o server
g++ server.cpp ./CGImysql/sql_connection_pool.cpp -o server -lmysqlclient -lpthread
```
- 运行
```bash
./server 9190
```

## client
- 编译
```bash
g++ client.cpp -o client
```
- 运行
```bash
./client 127.0.0.1 9190
```

## todo
- [ ] sql用户信息存储

