#include<iostream>
#include<string>
#include "Connection.h"
#include "ConnectionPool.h"
#include<thread>
using namespace std;

/*
测试的目的是什么？使用连接池减少了哪部分的开销？
1、减少了TCP连接的建立/取消  
测试数据，单线程条件下使用连接池插入1000条数据 versus 不使用连接池
*/

void tick(){
    clock_t be=clock();

    // do sth

    clock_t ed=clock();
    cout<<(ed-be)/1000<<"ms"<<endl;
}

// 不使用连接池的写入
void notUse(){
    for(int i=0;i<1000;i++){
        Connection conn;
        conn.connect("101.42.23.45", 3306, "qzyDB", "helloQzy", "chat");
        string sql="insert into user (name,age,sex) values (\"zhangsan\",20,\"male\")";
        conn.update(sql.c_str());
    }
}

// 使用连接池的写入
void usePool(){
    ConnectionPool* cp=ConnectionPool::getConnectionPool();
    for(int i=0;i<1000;i++){//woc代码  写了3w才发现
        auto spConn=cp->getConnection();
        string sql="insert into user (name,age,sex) values (\"zhangsan\",20,\"male\")";
        spConn->update(sql.c_str());
    }
}

int main(){
    // cout<<"hello world"<<endl;
    // Connection conn;
    // conn.connect("101.42.23.45", 3306, "qzyDB", "helloQzy", "chat");
    // string sql="insert into user(name,age,sex) values (\"zhangsan\",20,\"male\")";
    // conn.update(sql.c_str());
    // cout<<"begin main"<<endl;
    // ConnectionPool* pool=ConnectionPool::getConnectionPool();
    // 线程池实例，从server端看下是否连接，单例的使用不是说只能调用一次get
    // 而是每次get都是返回的相同的实例
    clock_t be=clock();
    // usePool();
    notUse();
    clock_t ed=clock();
    cout<<(ed-be)<<"ticks"<<endl;

    // 多个线程的使用，很简单每个线程都执行usePool
    // thread th1(usePool);
    // thread th2(usePool);

    // th1.join();th2.join();
}