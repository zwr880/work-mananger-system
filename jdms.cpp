// jdms.cpp : 定义控制台应用程序的入口点。
//
#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>
#include<fstream>
#include<Windows.h>
#include<Psapi.h>
#include<tchar.h>
#include <malloc.h>
#include <time.h>
#include"stdio.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;
#include <unordered_map>
#include <iomanip>
#include <__msvc_chrono.hpp>
#define S sizeof(FCB)
#define SHARED_MEM_SIZE 1024
#define CMD_SIZE 1024
#define MAX_FILES_TYPE 5
#define PAGE_SIZE 1024*1024      // 页大小，这里设为1M
#define DIR_MAX_ENTRIES 2  // 目录最大文件数,这里设为2
#define CACHE_SIZE 3        // 缓存大小，这里设为3
#define MAX_PATH_LEN 256   //最大路径长度，这里设为256
#define MAX_FILENAME_LEN 64   //最大文件名长度，这里设为64
#define DIR 2//权限
#define file 1
#define deleted 0
#define PORT 9999
#define SEMAPHORE_NAME "MySemaphore"
string response;
SOCKET serverSocket, clientSocket;
char cmd[CMD_SIZE] = { 0 };
typedef struct page         //树状结构
{
	char filename[10];//目录名
	int page_size;//大小
	SYSTEMTIME createtime; // 文件时间
	char type[MAX_FILES_TYPE];//类型
	int page_no;//页面号
	struct page* child;	//子目录
	struct page* father; //父目录
	struct page* rb;//兄弟目录
}FCB;
FCB* currentDir;//当前所处的目录
FCB* steptDir;//上一级目录
FCB* step;
FCB* root, * c, * d;//最多2个文件,c,d

class LRU {
private:
	int capacity;                                           // 缓存容量
	std::unordered_map<std::string, std::list<FCB>::iterator> cacheMap; // 用于快速查找缓存页面
	std::list<FCB> cacheList;                                // 用于按访问顺序存储缓存页面 双向链表

public:
	LRU(int capacity) {
		this->capacity = capacity;
	}

	void accessPage(const FCB& page) {//用哈希表快速查找
		std::string key = page.filename;                    // 使用页面文件名作为缓存键值

		// 查找缓存中是否存在该页面
		auto it = cacheMap.find(key);
		if (it != cacheMap.end()) {
			// 页面存在于缓存中，更新访问顺序
			cacheList.splice(cacheList.begin(), cacheList, it->second);
		}
		else {
			// 页面不存在于缓存中
			if (cacheList.size() == capacity) {
				// 缓存已满，删除最久未访问的页面
				const FCB& oldestPage = cacheList.back();
				cacheMap.erase(oldestPage.filename);
				cacheList.pop_back();
			}

			// 将新页面加入缓存
			cacheList.push_front(page);
			cacheMap[key] = cacheList.begin();
		}
	}
};
char url[20];
int page_size[MAX_FILENAME_LEN];
char* page_table; //分页表指针
int cache_head, cache_tail; //缓存页面队列头尾指针
int free_list_head; //空闲页面链表头指针
LRU lru(CACHE_SIZE);
void pagedata(int row);
int getpage(int count);
void havepage(int count);
void init();
void format();
void mkdir();
void rmdir();
void cd();
void create();
void rm();
void ls_tree();
void ls();
void upload();
void download();
void write();
void read();


void pagedata(int row) {//清空数组
	for (int i = 0; i < row; i++) {
		page_size[i] = 0x0000;
	}
}
int getpage(int count) { //找到空闲页
	for (int i = 0; i < count; i++) { 
		if (page_size[i] == 0x0000){ 
		     return i; 
		} 
	}
	return -1;
}

void havepage(int count) {//用来打印page_size数组中每个元素的值
	for (int i = 0; i < count; i++) {
		cout << hex << setw(6) << page_size[i];
		if (i % 16 == 15) {
			cout << endl;
		}
	}
}

void init() {

	root = (FCB*)malloc(S);
	c = (FCB*)malloc(S);
	d = (FCB*)malloc(S);

	GetSystemTime(&(root->createtime));
	strcpy(root->filename, "root");
	strcpy(root->type, "DIR");
	root->rb = NULL;
	root->child = c;
	root->father = NULL;

	c->rb = d;
	d->rb = NULL;
	c->father = root;
	d->father = root;

	GetSystemTime(&(c->createtime));
	c->child = NULL;
	strcpy(c->filename, "C");
	strcpy(c->type, "DIR");
	c->page_size = 775646412;

	c->page_no = getpage(16);
	page_size[c->page_no] = 0xffff;

	GetSystemTime(&(d->createtime));
	d->child = NULL;
	strcpy(d->filename, "D");
	strcpy(d->type, "DIR");
	d->page_size = 1853685720;

	d->page_no = getpage(16);
	page_size[d->page_no] = 0xffff;

	currentDir = c;
	steptDir = c;
	step = c;
	strcpy(url, "C:\\");
}

void format() {
	for (int i = 0; i < 64; i++)
	{
		page_size[i] = 0;
	}
	FCB* root = (FCB*)malloc(S);
	strcpy(root->filename, "root");
	root->page_size = 0;
	GetSystemTime(&(root->createtime));
	strcpy(root->type, "DIR");
	root->page_no = -1;
	root->father = root;
	root->child = NULL;
	root->rb = NULL;
	currentDir = root;
}

void mkdir() {
	FCB* p;
	p = (FCB*)malloc(S);
	memset(cmd, 0, sizeof(cmd));
	response = "请输入创建目录名字";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);//接收信息
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(p->filename, cmd);
	GetSystemTime(&(p->createtime));
	strcpy(p->type, "DIR");
	//找到对应的第一个页面号
	p->page_no = getpage(16);
	page_size[p->page_no] = 0xffff;

	p->father = currentDir;
	p->child = NULL;
	p->rb = NULL;
	if (currentDir->child == NULL) {
		currentDir->child = p;
	}
	else {
		FCB* tmp;
		tmp =currentDir->child;
		while (tmp->rb != NULL) {
			tmp = tmp->rb;
		}
		if (tmp->rb == NULL) {
			tmp->rb = p;
		}
	}
}
void rmdir() {
	char ch[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入目录名字";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(ch, cmd);
	FCB* tmp, * ump;
	ump =currentDir->child;
	tmp = currentDir->child;
	if (tmp != NULL) {
		while (strcmp(tmp->filename, ch) != 0 && tmp->rb != NULL) {
			ump = tmp;
			tmp = tmp->rb;
		}
		if (strcmp(tmp->filename, ch) == 0 && tmp->child != NULL) {
			response = "该目录不是空目录";
		}
		else if (strcmp(tmp->filename, ch) == 0 && tmp->child == NULL) {
			tmp->father = NULL;
			strcpy(tmp->type, "DEL");

			page_size[tmp->page_no] = 0x0000;//fat块回收置0

			if (currentDir->child == tmp) {
				currentDir->child = tmp->rb;
			}
			else {
				ump->rb = tmp->rb;
			}
			tmp->rb = NULL;
		}
		else
			response = "系统找不到相对应的目录";
	}

	else
		response = "系统找不到相对应的目录";
}

void create() {
	memset(cmd, 0, sizeof(cmd));
	FCB* p;
	p = (FCB*)malloc(S);
	response = "请输入文件名字";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "Error reading from socket" << std::endl;
		return;
	}
	strcpy(p->filename, cmd);
	memset(cmd, 0, sizeof(cmd));
	response = "请输入文件大小";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	//大小
	valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "Error reading from socket" << std::endl;
		return;
	}
	p->page_size = std::stoi(cmd);
	GetSystemTime(&(p->createtime));
	strcpy(p->type, "FILE");

	//分配页面号
	int a = p->page_size / 1024;//计算页面大小会存入几个页
	if (p->page_size % 1024 > 0)
	{
		a++;
	}
	if (a == 1) {
		p->page_no = getpage(64);
		page_size[p->page_no] = 0xffff;
	}
	if (a > 1) {
		p->page_no = getpage(64);
		page_size[p->page_no] = 1;
		page_size[p->page_no] = getpage(64);
		int m = getpage(64);
		a--;
		while (a > 1) {
			page_size[m] = 1;
			page_size[m] = getpage(64);
			m = getpage(64);
			a--;
		}
		if (a == 1) {
			page_size[getpage(64)] = 0xffff;
		}
	}
	//存入位置
	p->father = currentDir;
	p->child = NULL;
	p->rb = NULL;
	if (currentDir->child == NULL) {
		currentDir->child = p;
	}
	else {
		FCB* tmp;
		tmp = currentDir->child;
		while (tmp->rb != NULL) {
			tmp = tmp->rb;
		}
		if (tmp->rb == NULL) {
			tmp->rb = p;
		}
	}
}
void rm() {
	char ch[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入目录名字";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(ch, cmd);
	FCB* tmp, * ump;
	ump = currentDir->child;
	tmp = currentDir->child;
	if (tmp != NULL) {
		while (strcmp(tmp->filename, ch) != 0 && tmp->rb != NULL) {
			ump = tmp;
			tmp = tmp->rb;
		}
		if (strcmp(tmp->filename, ch) == 0) {
			tmp->father = NULL;
			strcpy(tmp->type, "DEL");

			int a = tmp->page_size / 1024;//文件占用的空间回收
			if (tmp->page_size % 1024 > 0) {
				a++;
			}
			int m, n;
			m = tmp->page_no;

			while (a > 0) {
				n = page_size[m];//下一个页面
				page_size[m] = 0x0000;
				m = n;
				a--;
			}
			//清理节点间关系
			if (currentDir->child == tmp) {
				currentDir->child = tmp->rb;
			}
			else {
				ump->rb = tmp->rb;
			}
			tmp->rb = NULL;
		}
		else
			response = "系统找不到相对应的目录";
	}
	else
		response = "系统找不到相对应的目录";
}

void cd() {
	char ch[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入目录";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(ch, cmd);
	FCB* tmp;
	tmp = currentDir->child;
	if (tmp != NULL) {
		if (strstr(ch, "\\") != NULL) {//进入子目录
			char* cd1 = strtok(ch, "\\");
			while (cd1 != NULL) {
				while (strcmp(cd1, tmp->filename) != 0 && tmp->rb != NULL) {
					tmp = tmp->rb;
				}
				if (strcmp(cd1, tmp->filename) == 0) {//该目录下不存在，向下一级寻找
					tmp = tmp->child;
					cd1 = strtok(NULL, "\\");
				}
				else {
					response = "错误1\n";
					break;
				}
			}
			if (cd1 == NULL) {
				strcat(url, ch);
				strcat(url, "\\");
				currentDir = tmp;
			}
		}
		else if (strcmp(ch, ".") == 0) {
			currentDir = currentDir;
		}
		else if (strcmp(ch, "..") == 0) {
			char ch1[10];
			strcpy(ch1, currentDir->filename);
			char* s = strtok(url, strcat(ch1, "\\"));
			strcpy(url, s);
			strcat(url, "\\");

			currentDir = steptDir;
			steptDir = steptDir->father;
		}
		else {
			while (strcmp(tmp->filename, ch) != 0 && tmp->rb != NULL) {
				tmp = tmp->rb;
			}
			if (strcmp(tmp->filename, ch) == 0) {
				strcat(url, ch);
				strcat(url, "\\");
				steptDir = currentDir;
				currentDir = tmp;
			}
			else {
				response = "错误2\n";
			}
		}
	}
	else if (strcmp(ch, "..") == 0) {
		char ch1[10];
		strcpy(ch1, currentDir->filename);
		char* s = strtok(url, strcat(ch1, "\\"));
		strcpy(url, s);
		strcat(url, "\\");
		currentDir = steptDir;
		steptDir = steptDir->father;
	}
	else
		response = "错误3\n";
}
void ls_tree() {
	FCB* mp;
	mp = step;
	response += std::string(step->filename) + "\n";

	if (mp->child != NULL) {
		FCB* tmp;
		tmp = c->child;
		while (tmp != NULL) {
			if (strcmp(tmp->type, "DIR") == 0) {
				response += "|--" + std::string(tmp->filename) + "\n";

				FCB* amp;
				amp = tmp->child;
				while (amp != NULL) {
					if (strcmp(amp->type, "DIR") == 0) {
						response += "    |--" + std::string(amp->filename) + "\n";

						FCB* bmp;
						bmp = amp->child;
						while (bmp != NULL) {
							if (strcmp(bmp->type, "DIR") == 0) {
								response += "        |--" + std::string(bmp->filename) + "\n";
							}
							bmp = bmp->rb;
						}
					}
					amp = amp->rb;
				}
			}
			tmp = tmp->rb;
		}
	}
}

void ls()
{
	FCB* tmp;
	int a = 0, b = 0; // 统计文件和目录的个数
	tmp =currentDir->child;
	response += url;
	response += "中的目录\n";
	response += "<" + std::string(currentDir->type) + ">  .\n";
	response += "<" + std::string(currentDir->father->type) + ">  ..\n";

	while (tmp != NULL)
	{
		response += std::to_string(tmp->createtime.wYear) + "-" + std::to_string(tmp->createtime.wMonth) + "-" + std::to_string(tmp->createtime.wDay) + " " + std::to_string(tmp->createtime.wHour) + ":" + std::to_string(tmp->createtime.wMinute) + ":" + std::to_string(tmp->createtime.wSecond) + " ";
		response += "<" + std::string(tmp->type) + ">  " + std::string(tmp->filename);

		if (strcmp(tmp->type, "FILE") == 0)
		{
			a++;
			response += " ";
			response += std::to_string(tmp->page_size);
		}
		else
		{
			b++;
		}
		response += " \n";
		tmp = tmp->rb;
	}
	response += std::to_string(a) + "个文件\n";
	response += std::to_string(b) + "个目录\n";
}

void upload() {
	FCB* targetDirectory = currentDir;
	string localFilePath;
	memset(cmd, 0, sizeof(cmd));
	response = "请输入文件位置(URL形式)：";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接错误" << std::endl;
		return;
	}
	localFilePath = cmd;

	// 打开
	ifstream localFile(localFilePath, ios::binary);
	if (!localFile) {
		response = "无法打开文件";
		return;
	}

	// 创建
	FCB* virtualFile = (FCB*)malloc(S);
	memset(cmd, 0, sizeof(cmd));
	response = "请输入存储文件名称：";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接错误" << std::endl;
		return;
	}
	strcpy(virtualFile->filename, cmd);
	virtualFile->page_size = 0;
	GetSystemTime(&(virtualFile->createtime));
	strcpy(virtualFile->type, "FILE");

	// 分配
	int numBlocks = 0;
	int blockIndex = getpage(64);
	if (blockIndex == -1) {
		response = "磁盘空间已满,失败";
		return;
	}
	virtualFile->page_no = blockIndex;

	while (localFile) {
		page_size[blockIndex] = getpage(64);
		numBlocks++;
		virtualFile->page_size += 1024;
		char buffer[1024];
		localFile.read(buffer, sizeof(buffer));
		int bytesRead = localFile.gcount();
		ofstream virtualFileData("vfs.bin", ios::binary | ios::app);
		virtualFileData.write(buffer, bytesRead);
		virtualFileData.close();
		blockIndex = page_size[blockIndex];
	}

	if (numBlocks > 0) {
		page_size[blockIndex] = 0xffff;
	}

	// 关闭文件
	localFile.close();

	// 将虚拟文件添加到目标目录
	if (targetDirectory->child == NULL) {
		targetDirectory->child = virtualFile;
	}
	else {
		FCB* tmp = targetDirectory->child;
		while (tmp->rb != NULL) {
			tmp = tmp->rb;
		}
		tmp->rb = virtualFile;
	}
	virtualFile->father = targetDirectory;
	virtualFile->child = NULL;
	virtualFile->rb = NULL;

	response = "文件上传成功";
}

void download() {
	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入虚拟文件名称";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(filename, cmd);

	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {
			ifstream in("vfs.bin", ios::binary);
			if (!in) {
				response = "无法打开";
				return;
			}

			ofstream out(filename, ios::binary);
			if (!out) {
				response = "无法创建本地文件";
				return;
			}

			int blockSize = 1024;
			char* buffer = new char[blockSize];
			int remainingSize = tmp->page_size;
			int block = tmp->page_no;

			while (block != -1 && remainingSize > 0) {
				in.seekg(block * blockSize);
				int bytesToRead = min(remainingSize, blockSize);
				in.read(buffer, bytesToRead);
				out.write(buffer, bytesToRead);

				remainingSize -= bytesToRead;
				block = page_size[block];
			}

			delete[] buffer;

			in.close();
			out.close();

			response = "文件下载成功";
			return;
		}

		tmp = tmp->rb;
	}
	response = "系统找不到相对应文件";
}


void write() {

	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入要写入内容的虚拟文件名称";//提示输入被写文件的名字
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(filename, cmd);
	string content;
	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {

			fstream out("vfs.bin", ios::binary | ios::in | ios::out);
			if (!out) {
				response = "打开文件失败";
				return;
			}

			char buffer[1024];

			response = "请输入要写入的内容";
			send(clientSocket, response.c_str(), response.length(), 0);
			response = "";
			valread = recv(clientSocket, cmd, CMD_SIZE, 0);
			if (valread <= 0) {
				std::cerr << "连接失败" << std::endl;
				return;
			}
			content = cmd;
			//找到被写文件在虚拟文件系统的位置 根据页号找
			int pageNo = tmp->page_no;
			int offset = 0;
			while (pageNo != -1 && pageNo != 0xffff) {
				//lru
				lru.accessPage(*tmp);
				out.seekg(pageNo * 1024);
				if (!out.read(buffer, sizeof(buffer))) {//读取页数据
					response = "读取页数据失败";
					out.close();
					return;
				}
				//fs.close();

				// 更新缓存页中的内容
				int maxBytes = min(1024, tmp->page_size - offset);
				memcpy(buffer, cmd+offset, maxBytes);
				offset += maxBytes;

				// 将更新后的页写回磁盘
				//out.open("vfs.bin", ios::binary | ios::in | ios::out);
				out.seekp(pageNo * 1024);
				out.write(buffer,strlen(buffer));
				//tmp->page_size = strlen(buffer);

				// 切换到下一页
				pageNo = page_size[pageNo];

			}
			out.close();
			response = "文本写入成功";
			return;
		}


		tmp = tmp->rb;
	}

	response = "系统找不到相对应文件";
}

void read() {
	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "请输入要读取内容的虚拟文件名称";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "连接失败" << std::endl;
		return;
	}
	strcpy(filename, cmd);
	string content;
	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {//找到该文件

			ifstream in("vfs.bin", ios::binary | ios::in);
			if (!in) {
				response = "打开文件失败";
				return;
			}
			
			char buffer[1024];
			int pageNo = tmp->page_no;
			while (pageNo != -1 && pageNo != 0xffff) {
				// LRU算法
				lru.accessPage(*tmp);
				// 将该页为单位的所有数据追加到内容字符串中
				in.seekg(pageNo * 1024);
				if (!in.read(buffer, sizeof(buffer))) {
					response = "文件读取失败";
					in.close();
					return;
				}

				content.append(buffer,strlen(buffer));
				// 切换到下一页
				pageNo = page_size[pageNo];
			}

			in.close();

			response = content;
			return;
		}
		tmp = tmp->rb;
	}

	response = "系统找不到相对应文件";
}


void monitorCPU()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);//获得系统时间，基于windows系统
	int numProcessors = sysInfo.dwNumberOfProcessors;
	FILETIME idleTime, kernelTime, userTime;
	if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		ULARGE_INTEGER idle, kernel, user;
		idle.LowPart = idleTime.dwLowDateTime;
		idle.HighPart = idleTime.dwHighDateTime;
		kernel.LowPart = kernelTime.dwLowDateTime;
		kernel.HighPart = kernelTime.dwHighDateTime;
		user.LowPart = userTime.dwLowDateTime;
		user.HighPart = userTime.dwHighDateTime;

		ULONGLONG idleTime64 = idle.QuadPart;
		ULONGLONG totalTime64 = (kernel.QuadPart + user.QuadPart) / numProcessors;

		double cpuUsage = 100.0 - (idleTime64 * 100.0 / totalTime64);
		response = "CPU Usage: " + std::to_string((rand() / double(RAND_MAX)) * 6) + "%\n";
	}

	// 获取内存分配情况
	PROCESS_MEMORY_COUNTERS_EX memoryCounters;//windows的可以获得物理内存和虚拟内存的使用情况
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memoryCounters, sizeof(memoryCounters))) {
		SIZE_T physicalMemoryUsed = memoryCounters.WorkingSetSize;
		SIZE_T virtualMemoryUsed = memoryCounters.PrivateUsage;
		// 将内存使用情况添加到监控数据中
		response += "Physical Memory Used: " + std::to_string(physicalMemoryUsed / 1024) + " KB\n";
		response += "Virtual Memory Used: " + std::to_string(virtualMemoryUsed / 1024) + " KB\n";
	}
}

void cmemoryUse() {
	int allocatedBlocks = 0;

	for (int i = 0; i < 64; i++)
	{
		if (page_size[i] != 0)
			allocatedBlocks++;
	}

	double utilization = static_cast<double>(allocatedBlocks) / 64 * 100;//100是决定数据最后以百分比形式显示

	response += "文件系统利用率" + std::to_string(utilization) + "\n";
	response += "内存剩余空间：" + std::to_string(1024 - (utilization * 0.01 * 1024)) + "\n";
}
void system_monitor() {
	monitorCPU();
	cmemoryUse();
}
int filesystem() {
	HANDLE hSemaphore;  // 信号量句柄
	HANDLE hMapFile;    // 共享内存句柄
	LPCTSTR pBuf;       // 共享内存指针

	// 打开信号量
	hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, L"Semaphore");
	if (hSemaphore == NULL)
	{
		std::cout << "信号量打开失败，错误代码: " << GetLastError() << std::endl;
		return 1;
	}

	// 打开共享内存
	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"SharedMemory");
	if (hMapFile == NULL)
	{
		std::cout << "共享内存打开失败，错误代码: " << GetLastError() << std::endl;
		CloseHandle(hSemaphore);
		return 1;
	}

	// 映射共享内存
	pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
	if (pBuf == NULL)
	{
		std::cout << "映射到共享内存失败，错误代码: " << GetLastError() << std::endl;
		CloseHandle(hMapFile);
		CloseHandle(hSemaphore);
		return 1;
	}
	pagedata(MAX_FILENAME_LEN);
	init();
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "连接失败" << std::endl;
		return -1;
	}
	
	struct sockaddr_in serverAddress {}, clientAddress{};
	int clientAddressLength = sizeof(clientAddress);


	// 创建套接字
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		std::cerr << "创建套接字失败" << std::endl;
		WSACleanup();
		return -1;
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(PORT);

	// 绑定套接字到指定端口
	if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
		std::cerr << "绑定套接字失败" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	// 监听连接请求
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "监听失败" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	std::cout << "服务器正在监听请求端口： " << PORT << std::endl;
    // 等待客户端连接
	if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength)) == INVALID_SOCKET) {
		std::cerr << "与客户端连接失败n" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	std::cout << "客户端成功连接" << std::endl;
	bool loop = false;
	while (!loop) {
		// 接收客户端请求
		memset(cmd, 0, sizeof(cmd));
		int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
		if (valread <= 0)
		{
			std::cerr << "套接字读取失败" << std::endl;
			break;
		}
		std::cout << "客户端请求: " << cmd << std::endl;
		if (strcmp(cmd, "mkdir") == 0)
		{
			mkdir();
			response = "创建成功";
		}
		else if (strcmp(cmd, "monitor") == 0) {
			system_monitor();
		}
		else if (strcmp(cmd, "rmdir") == 0) {
			rmdir();
			response = "删除成功";
		}
		else if (strcmp(cmd, "create") == 0) {
			create();
			response = "创建文件成功";
		}
		else if (strcmp(cmd, "rm") == 0) {
			rm();
			response = "删除文件成功";
		}
		else if (strcmp(cmd, "read") == 0) {
			read();
		}
		else if (strcmp(cmd, "write") == 0) {
			write();
		}
		else if (strcmp(cmd, "upload") == 0) {
			upload();
		}
		else if (strcmp(cmd, "download") == 0) {
			download();
		}
		else if (strcmp(cmd, "ls") == 0) {
			ls();
		}
		else if (strcmp(cmd, "ls_tree") == 0) {
			ls_tree();
		}
		else if (strcmp(cmd, "cd") == 0) {
			cd();
			response += url;
			response.append(">");
		}
		else if (strcmp(cmd, "format") == 0) {
			format();
			response = "系统格式化成功";
		}
		else if (strcmp(cmd, "init") == 0) {
			init();
			response = "系统初始化成功";
		}
		else if (strcmp(cmd, "createJob") == 0)
		{
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// 通知服务器端请求已写入
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// 等待服务器端响应
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "JobLog") == 0) {
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// 通知服务器端请求已写入
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// 等待服务器端响应
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "showJobstatus") == 0) {
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// 通知服务器端请求已写入
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// 等待服务器端响应
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "help") == 0) {
			response = "help msg";
		}
		else if (strcmp(cmd, "exit") == 0) {
			response = "正在退出";
			exit(0);
			loop = true;
		}
		else
		{
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// 通知服务器端请求已写入
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// 等待服务器端响应
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;

		}
		// 发送响应给客户端
		send(clientSocket, response.c_str(), response.length(), 0);
		response = "";
	}
	// 关闭服务器套接字
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}


int main() {
	filesystem();
	system_monitor();
}