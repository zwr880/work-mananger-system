// jobtest.cpp : �������̨Ӧ�ó������ڵ㡣
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
    int jobwork;//��ʼֵΪ0���ܹ���5��
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
    HANDLE hSemaphore;  // �ź������
    HANDLE hMapFile;    // �����ڴ���
    LPCTSTR pBuf;       // �����ڴ�ָ��

    // �����ź���
    hSemaphore = CreateSemaphore(NULL, 0, 1, L"Semaphore");
    if (hSemaphore == NULL)
    {
        cout << "�����ź���ʧ�ܣ��������: " << GetLastError() << std::endl;
        return 1;
    }

    // ���������ڴ�
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHARED_MEM_SIZE, L"SharedMemory");
    if (hMapFile == NULL)
    {
        cout << "���������ڴ�ʧ�ܣ��������: " << GetLastError() << std::endl;
        CloseHandle(hSemaphore);
        return 1;
    }

    // ӳ�乲���ڴ�
    pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (pBuf == NULL)
    {
        cout << "ӳ�乲���ڴ�ʧ�ܣ��������: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        CloseHandle(hSemaphore);
        return 1;
    }

    while (true) {
        // �ȴ��ͻ�������
        cout << "��ҵ����ϵͳ�ȴ��ͻ��������У�" << std::endl;
        WaitForSingleObject(hSemaphore, INFINITE);
        // ��ȡ�ͻ�������
        string request((char*)pBuf);
        cout << "�ӿͻ��˽��յ�����: " << request << std::endl;
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
            response = "jobIDΪ"+to_string(jobID) + "������ֹͣ" + "\n";
        }
        else if (requestType == "cancel") {
            jobs[jobID - 1].status = "cancel";
            response = "jobIDΪ" + to_string(jobID) + "��ҵ��ɾ��" + "\n";
        }
        else if (requestType == "resume") {
            jobs[jobID - 1].status = "running";
            response = "jobIDΪ" + to_string(jobID) + "����������" + "\n";
        }
        else {
            response = "�������\n";
        }
        if (!jobs.empty()) {//������ô��ʾ
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
        // ����Ӧд�빲���ڴ�
        CopyMemory((PVOID)pBuf, response.c_str(), (response.length() + 1) * sizeof(char));

        // ֪ͨ�ͻ�����Ӧ��д��
        ReleaseSemaphore(hSemaphore, 1, NULL);
    }
    // ������Դ
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    CloseHandle(hSemaphore);

    return 0;

}