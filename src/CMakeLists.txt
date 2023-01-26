add_executable(main main.cpp Connection.cpp ConnectionPool.cpp)
target_link_libraries(main mysqlclient pthread)
