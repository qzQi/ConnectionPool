#include "ConnectionPool.h"
#include "public.h"
#include<string.h>
#include<thread>
#include<functional>//for bind
#include<chrono>
ConnectionPool* ConnectionPool::getConnectionPool(){
    // 静态成员的初始化发生在对象的构造之前
    // 陈硕那个模板实现，通用的不如这个。成员函数内部使用私有的
    static ConnectionPool pool;//这个什么时候发生构造呢？
    return &pool;
}

// 从配置文件加载配置
bool ConnectionPool::loadConfigFile(){
    FILE* pf=fopen("./mysql.ini","r");
    if(pf==nullptr){
        cout<<strerror(errno)<<endl;
        LOG("mysql.ini file not exist");
        return false;
    }
    while(!feof(pf)){
        char line[1024]={0};
        fgets(line,1024,pf);
        string str(line);
        int idx=str.find('=',0);
        if(idx==-1){
            continue;//无效的配置项
        }
        // eg:passwd=123456\n;
        int endidx=str.find('\n',idx);
        string key=str.substr(0,idx);
        string value=str.substr(idx+1,endidx-idx-1);
        if(key=="ip"){
            _ip=value;
        }else if(key=="port"){
            _port=atoi(value.c_str());
        }else if(key=="username"){
            _username=value;
        }else if(key=="password"){
            _password=value;
        }else if(key=="dbname"){
            _dbname=value;
        }else if(key=="initSize"){
            _initSize=atoi(value.c_str());
        }else if(key=="maxSize"){
            _maxSize=atoi(value.c_str());
        }else if(key=="maxIdleTime"){
            _maxIdleTime=atoi(value.c_str());
        }else if(key=="connectionTimeout"){
            _connectionTimeout=atoi(value.c_str());
        }
    }
    return true;
}

// 初始化连接池,哪里都没出错，为啥没有MySQL连接？
// 正确的，每条连接均可用
ConnectionPool::ConnectionPool(){
    if(!loadConfigFile()){
        return ;
    }
    // 创建初始的mysql连接
    LOG("begin init pool");
    cout<<"initSize:"<<_initSize<<endl;
    for(int i=0;i<_initSize;i++){
        // 没有写析构函数啊！ 直接使用智能指针得了，使用sp也没啥
        Connection* p=new Connection();
        p->connect(_ip,_port,_username,_password,_dbname);
        p->refreshAliveTime();//每次进入连接词刷新起始空闲时间
        _connectionQue.push(p);
        _connectionCnt++;//原子操作
    }

    // 创建生产连接的线程
    thread producer(bind(&ConnectionPool::produceConnectionTask,this));
    producer.detach();

    // 创建定期删除空闲连接线程
    thread scanner(bind(&ConnectionPool::scannerConnectionTask,this));
    scanner.detach();
}


// 生产者：在连接队列为空的时候才开始进行生产连接
void ConnectionPool::produceConnectionTask(){
    for(;;){
        unique_lock<mutex> lk(_queueMutex);
        while(!_connectionQue.empty()){
            empty.wait(lk);//unique_lock和cv的配合使用
        }

        // 当前已创建的连接数小于maxSize才可以新建
        if(_connectionCnt<_maxSize){
            Connection* p=new Connection();
            p->connect(_ip,_port,_username,_password,_dbname);
            _connectionQue.push(p);
            _connectionCnt++;
        }

        // 通知消费者线程，已经有新的连接了
        not_empty.notify_all();
    }
}



// 消费者：提供给用户的接口，返回一个shared_ptr
// 有一个最大请求时间，使用wait_for；
shared_ptr<Connection> ConnectionPool::getConnection(){
    unique_lock<mutex> lk(_queueMutex);
    // 如果连接池为空，阻塞时间最多为_connectionTimeout
    while(_connectionQue.empty()){
        if(cv_status::timeout==not_empty.wait_for(lk,chrono::microseconds(_connectionTimeout))){
            //超时后如果仍然为空，返回nullptr
            if(_connectionQue.empty()){
                LOG("获取MySQL连接失败");
                return nullptr;
            }
        }
    }

    // 返回的shared_ptr默认的析构当作是delete，但是我们queue里面存放的是裸指针
    // 如果存放的是智能指针，这里就不需要自定义析构了
    shared_ptr<Connection> sp(_connectionQue.front(),
    [&](Connection* pcon){
        unique_lock<mutex> lk(_queueMutex);
        // 连接进入连接池，更新该条连接的时间
        // 还真得使用裸指针，这样每次sp对象析构的时候就可以更新连接时间
        // 连接队列必须使用裸指针，如果直接使用智能指针并在一开始就传入析构动作会导致根本无法析构
        // （因为析构动作就是加入连接队列）
        pcon->refreshAliveTime();
        _connectionQue.push(pcon);
    });

    _connectionQue.pop();
    if(_connectionQue.empty()){
        empty.notify_all();
    }
    return sp;
}


// 定时删除maxIdleTime不使用的空闲mysql连接，队列结构前面的就是最早放入队列的
// 实现这个功能需要每条连接的起始空闲时间，为Connection类增加成员
// 起始空闲时间也就是新建/回收后 加入连接池的时间
void ConnectionPool::scannerConnectionTask(){
    for(;;){
        // sleep 每条连接的空闲时间是IdleTime所以我们每IdleTime运行一次
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        unique_lock<mutex> lk(_queueMutex);
        // while(_connectionQue.size())连接池里面仅仅是空闲的，而不是所有已经创建的
        // queue里面是空闲的
        while(_connectionCnt>_initSize){
            if(_connectionQue.empty())break;
            Connection* p=_connectionQue.front();
            // clock除以CLOCKS_PER_SEC才是秒数            
            if(p->getAliveeTime()>=_maxIdleTime*1000*1000){
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }else{
                break;
            }
        }
    }
}
