#include "../inc/ConnPool.h"
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace std;

ConnPool::ConnPool() {
    this->CurConn = 0;
    this->FreeConn = 0;
}

/**
 * @brief 初始化数据库连接池
 *
 * @param host     主机名
 * @param user     数据库用户名
 * @param password 数据库密码
 * @param DBName   数据库名
 * @param port     端口号
 * @param maxConn  最大连接数
 */
void ConnPool::init(string host, string user, string password, string DBName,
                    int port, unsigned int maxConn) {
    this->host_ = host;
    this->port_ = port;
    this->user_ = user;
    this->password_ = password;
    this->databaseName_ = DBName;

    lock.lock();
    for (int i = 0; i < maxConn; i++) {
        MYSQL *conn = NULL;
        conn = mysql_init(conn);

        if (conn == NULL) {
            cout << "Error:" << mysql_error(conn);
            exit(1);
        }
        conn =
            mysql_real_connect(conn, host.c_str(), user.c_str(),
                               password.c_str(), DBName.c_str(), port, NULL, 0);

        if (conn == NULL) {
            cout << "Error: " << mysql_error(conn);
            exit(1);
        }
        connList.push_back(conn);
        ++FreeConn;
    }

    reserve = Semaphore(FreeConn);

    this->MaxConn = FreeConn;

    lock.unlock();
}

// 当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL *ConnPool::GetConn() {
    MYSQL *conn = NULL;

    if (0 == connList.size())
        return NULL;

    reserve.wait();

    lock.lock();

    conn = connList.front();
    connList.pop_front();

    --FreeConn;
    ++CurConn;

    lock.unlock();
    return conn;
}

// 释放当前使用的连接
bool ConnPool::ReleaseConn(MYSQL *con) {
    if (NULL == con)
        return false;

    lock.lock();

    connList.push_back(con);
    ++FreeConn;
    --CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

// 销毁数据库连接池
void ConnPool::DestroyPool() {

    lock.lock();
    if (connList.size() > 0) {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it) {
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();

        lock.unlock();
    }

    lock.unlock();
}

// 当前空闲的连接数
int ConnPool::GetFreeConn() { return this->FreeConn; }

ConnPool::~ConnPool() { DestroyPool(); }

ConnRAII::ConnRAII(MYSQL **SQL, ConnPool *connPool) {
    *SQL = connPool->GetConn();

    conn = *SQL;
    pool = connPool;
}

ConnRAII::~ConnRAII() { pool->ReleaseConn(conn); }
