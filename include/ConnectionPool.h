
// 实现连接池的功能，所有的连接放在一个队列里面，但是队列需要保证线程安全
// 所以直接使用阻塞队列，直接使用现成的组件或者自己写一下。
// 可以迭代两个版本，第一个不使用阻塞队列，而是直接自己写
// shilei代码有点问题吧，这显然是一个有界的阻塞队列得用两个条件变量
// 确实应该两个cv，一个是not_empty 一个 empty
#pragma once
#include<string>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<iostream>
#include<memory>// for sp
#include "Connection.h"

using namespace std;
 
class ConnectionPool{
public:
    //获取单例
    static ConnectionPool* getConnectionPool();

    // 用户接口，获取数据库连接，若超时返回null
    shared_ptr<Connection> getConnection();

    // 还是写成智能指针吧，这样还得写析构函数,来关闭连接
    ~ConnectionPool(){
        while(!_connectionQue.empty()){
            Connection* p=_connectionQue.front();
            _connectionQue.pop();
            delete p;
        }
    }
private:
    ConnectionPool();//单例的实现

    // 从配置文件加载配置
    bool loadConfigFile();

    // 生产MySQL连接的线程执行体
    void produceConnectionTask();

    // 定期删除超时连接的Task/thread
    void scannerConnectionTask();

    // 服务器配置及其性能参数
    string _ip;
    unsigned short _port;
    string _username;
    string _password;
    string _dbname;
    int _initSize;
    int _maxSize;
    int _maxIdleTime;
    int _connectionTimeout;

    // 实现线程安全的阻塞队列，有边界的阻塞队列？ 变形，生产的时机是连接队列为空
    // 生消模型都迷？
    // 为什么存放裸指针? 一开始直接存放的智能指针，但是需要用户使用后自己放入
    // 后来就直接存在裸指针，由裸指针构造智能指针再自定义析构动作
    queue<Connection*> _connectionQue;//存裸指针/智能指针 这个场景不复杂
    mutex _queueMutex;
    atomic_int _connectionCnt;
    // 不算是严格的生产者/消费者，那个无界的一个条件变量not_empty，有界的两个not_empty和not_full
    condition_variable empty;
    condition_variable not_empty;
};


