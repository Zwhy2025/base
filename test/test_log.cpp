#include <gtest/gtest.h>
#include "log.h"

// 定义一个模板函数来测量和打印函数执行时间
template <typename Func, typename... Args>
void trackTime(const std::string &funcName, Func func, Args&& ...args)
{
    // 获取开始时间
    auto start = std::chrono::high_resolution_clock::now();
    
    // 执行传入的函数
    func(std::forward<Args>(args)...);
    
    // 获取结束时间
    auto end = std::chrono::high_resolution_clock::now();
    
    // 计算时间差并转换为毫秒
    std::chrono::duration<double, std::milli> duration = end - start;
    
    // 打印函数名称和执行时间
    std::cout << "Function " << funcName << " took " << duration.count() << " ms" << std::endl;
}


void log_bench(int frequency=250,int ms=1,bool enable=true){
    if(enable){
        for (int i = 0; i < frequency; i++)
        {   
            LOG_DBUG<<"测试";
            LOG_INFO<<"日志";
            LOG_WARN<<"输出";
            LOG_EROR<<"完毕";
        
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }else{
        for (int i = 0; i < frequency; i++)
        {        
            std::cout<<"测试"<<std::endl;
            std::cout<<"日志"<<std::endl;
            std::cout<<"输出"<<std::endl;
            std::cout<<"完毕"<<std::endl;      
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }

}

TEST(LogTest, init) {
    INIT_LOG();
    
    LOG_DBUG<<"测试";
    LOG_INFO<<"日志";
    LOG_WARN<<"输出";
    LOG_EROR<<"完毕";
    
    WIAT_LOG();
    
    EXPECT_EQ(1, 1);
}

TEST(LogTest, log_bench) {
    INIT_LOG();
    
    trackTime("test_log_bench_thread",log_bench,250,1,true);

    WIAT_LOG();
    
    EXPECT_EQ(1, 1);
}



