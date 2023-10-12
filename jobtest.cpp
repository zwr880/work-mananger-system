// jobtest.cpp : 定义控制台应用程序的入口点。
//
#include<Windows.h>
#include<tchar.h>
#include <string.h>
#include <time.h>
#include<vector>
#include<iostream>
#include<sstream>
#include <condition_variable>
#define SHARED_MEM_SIZE 1024
#define SEMAPHORE_NAME "MySemaphore"
using namespace std;
string response;
struct Job {
    int id;
    int pid;
    string status;
    string log;
    double ratio;
    int jobwork;//初始值为0，总共有5个
    time_t start;
    time_t end;
    time_t timestamp;
};
vector<Job>jobs;
int createjob() {
    return static_cast<int>(std::time(nullptr));
}
int getId() {
    static int id = 0;
    return ++id;
}
void showJobstatus() {
    int run = 0;
    int suspend = 0;
    int cancel = 0;
    int finished = 0;
    int m = jobs.size();
    for (const auto& job : jobs) {
        if (job.status == "running") {
            run++;
         }
        else if (job.status == "suspend") {
            suspend++;
        }
        else if (job.status == "cancel") {
            cancel++;
        }
        else if (job.status == "finished") {
            finished++;
        }
    }
    response = "task running: " + to_string(run) + "\t";
    response += "task suspend: " + to_string(suspend) + "\t";
    response += "task cancel: " + to_string(cancel) + "\t";
    response += "task finished: " + to_string(finished) + "\t";
}
int main() {
    HANDLE hSemaphore;  // 信号量句柄
    HANDLE hMapFile;    // 共享内存句柄
    LPCTSTR pBuf;       // 共享内存指针

    // 创建信号量
    hSemaphore = CreateSemaphore(NULL, 0, 1, L"Semaphore");
    if (hSemaphore == NULL)
    {
        cout << "创建信号量失败，错误代码: " << GetLastError() << std::endl;
        return 1;
    }

    // 创建共享内存
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHARED_MEM_SIZE, L"SharedMemory");
    if (hMapFile == NULL)
    {
        cout << "创建共享内存失败，错误代码: " << GetLastError() << std::endl;
        CloseHandle(hSemaphore);
        return 1;
    }

    // 映射共享内存
    pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (pBuf == NULL)
    {
        cout << "映射共享内存失败，错误代码: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        CloseHandle(hSemaphore);
        return 1;
    }

    while (true) {
        // 等待客户端请求
        cout << "作业管理系统等待客户端请求中：" << std::endl;
        WaitForSingleObject(hSemaphore, INFINITE);
        // 读取客户端请求
        string request((char*)pBuf);
        cout << "从客户端接收到请求: " << request << std::endl;
        string requestType;
        int jobID;
        istringstream iss(request);
        iss >> requestType >> jobID;
        if (request == "createJob") {
            Job job1;
            job1.id = getId();
            job1.pid = createjob();
            job1.jobwork = 0;
            job1.status = "running";
            job1.start = time(NULL);
            job1.ratio = 0.0;
            job1.end = 0;
            jobs.push_back(job1);

            response = "create job successfully.\n";
            response += "jobID: " + to_string(job1.id) + "\n";
            response += "pid: " + to_string(job1.pid) + "\n";
            response += "job status: " + job1.status + "\n";
            response += "job ratio: " + to_string(job1.ratio) +"%" + "\n";

        }
        else if (request == "JobLog") {
            string str;
            for (const auto& job : jobs) {
                str += "jobId: " + to_string(job.id) + "\n";
                str += "job pid: " + to_string(job.pid) + "\n";
                str += "job status: " + job.status + "\n";
                str += "job ratio: " + to_string(job.ratio) + "\n";
            }
            response = str;
        }
        else if (request == "showJobstatus") {
            showJobstatus();
        }
        else if (requestType == "suspend") {
            jobs[jobID - 1].status = "suspend";
            response = "jobID为"+to_string(jobID) + "运行已停止" + "\n";
        }
        else if (requestType == "cancel") {
            jobs[jobID - 1].status = "cancel";
            response = "jobID为" + to_string(jobID) + "作业已删除" + "\n";
        }
        else if (requestType == "resume") {
            jobs[jobID - 1].status = "running";
            response = "jobID为" + to_string(jobID) + "运行已运行" + "\n";
        }
        else {
            response = "命令错误\n";
        }
        if (!jobs.empty()) {//进度怎么显示
            for (int i = 0; i < jobs.size(); i++) {
                if (jobs[i].status == "running" && jobs[i].jobwork<=5) {
                    jobs[i].jobwork++;
                    jobs[i].ratio = jobs[i].jobwork / 5;
                }
                if (jobs[i].jobwork == 5) {
                    jobs[i].status = "finished";
                }
           }
        }
        // 将响应写入共享内存
        CopyMemory((PVOID)pBuf, response.c_str(), (response.length() + 1) * sizeof(char));

        // 通知客户端响应已写入
        ReleaseSemaphore(hSemaphore, 1, NULL);
    }
    // 清理资源
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hSemaphore);

    return 0;

}