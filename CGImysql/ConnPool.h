#ifndef CONNPOOL_H
#define CONNPOOL_H

#include "../sync.h"
#include <error.h>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <string>

using namespace std;

class ConnPool {
  public:
    MYSQL *GetConn();              // 获取数据库连接
    bool ReleaseConn(MYSQL *conn); // 释放连接
    int GetFreeConn();             // 获取连接
    void DestroyPool();            // 销毁所有连接

    // 单例模式
    static ConnPool *instance() {
        static ConnPool connPool;
        return &connPool;
    }

    void init(string url, int Port, string User, string PassWord,
              string DataBaseName, unsigned int MaxConn);

    ~ConnPool();

  private:
    ConnPool();
    unsigned int MaxConn;  // 最大连接数
    unsigned int CurConn;  // 当前已使用的连接数
    unsigned int FreeConn; // 当前空闲的连接数

  private:
    MutexLock lock;
    list<MYSQL *> connList; // 连接池
    Semaphore reserve;

  private:
    string host_;         // 主机地址
    string port_;         // 数据库端口号
    string user_;         // 登陆数据库用户名
    string password_;     // 登陆数据库密码
    string databaseName_; // 使用数据库名
};

class ConnRAII {

  public:
    ConnRAII(MYSQL **conn, ConnPool *connPool);
    ~ConnRAII();

  private:
    MYSQL *conn;
    ConnPool *pool;
};

#endif /* CONNPOOL_H */
