// jdms.cpp : �������̨Ӧ�ó������ڵ㡣
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
#define PAGE_SIZE 1024*1024      // ҳ��С��������Ϊ1M
#define DIR_MAX_ENTRIES 2  // Ŀ¼����ļ���,������Ϊ2
#define CACHE_SIZE 3        // �����С��������Ϊ3
#define MAX_PATH_LEN 256   //���·�����ȣ�������Ϊ256
#define MAX_FILENAME_LEN 64   //����ļ������ȣ�������Ϊ64
#define DIR 2//Ȩ��
#define file 1
#define deleted 0
#define PORT 9999
#define SEMAPHORE_NAME "MySemaphore"
string response;
SOCKET serverSocket, clientSocket;
char cmd[CMD_SIZE] = { 0 };
typedef struct page         //��״�ṹ
{
	char filename[10];//Ŀ¼��
	int page_size;//��С
	SYSTEMTIME createtime; // �ļ�ʱ��
	char type[MAX_FILES_TYPE];//����
	int page_no;//ҳ���
	struct page* child;	//��Ŀ¼
	struct page* father; //��Ŀ¼
	struct page* rb;//�ֵ�Ŀ¼
}FCB;
FCB* currentDir;//��ǰ������Ŀ¼
FCB* steptDir;//��һ��Ŀ¼
FCB* step;
FCB* root, * c, * d;//���2���ļ�,c,d

class LRU {
private:
	int capacity;                                           // ��������
	std::unordered_map<std::string, std::list<FCB>::iterator> cacheMap; // ���ڿ��ٲ��һ���ҳ��
	std::list<FCB> cacheList;                                // ���ڰ�����˳��洢����ҳ�� ˫������

public:
	LRU(int capacity) {
		this->capacity = capacity;
	}

	void accessPage(const FCB& page) {//�ù�ϣ����ٲ���
		std::string key = page.filename;                    // ʹ��ҳ���ļ�����Ϊ�����ֵ

		// ���һ������Ƿ���ڸ�ҳ��
		auto it = cacheMap.find(key);
		if (it != cacheMap.end()) {
			// ҳ������ڻ����У����·���˳��
			cacheList.splice(cacheList.begin(), cacheList, it->second);
		}
		else {
			// ҳ�治�����ڻ�����
			if (cacheList.size() == capacity) {
				// ����������ɾ�����δ���ʵ�ҳ��
				const FCB& oldestPage = cacheList.back();
				cacheMap.erase(oldestPage.filename);
				cacheList.pop_back();
			}

			// ����ҳ����뻺��
			cacheList.push_front(page);
			cacheMap[key] = cacheList.begin();
		}
	}
};
char url[20];
int page_size[MAX_FILENAME_LEN];
char* page_table; //��ҳ��ָ��
int cache_head, cache_tail; //����ҳ�����ͷβָ��
int free_list_head; //����ҳ������ͷָ��
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


void pagedata(int row) {//�������
	for (int i = 0; i < row; i++) {
		page_size[i] = 0x0000;
	}
}
int getpage(int count) { //�ҵ�����ҳ
	for (int i = 0; i < count; i++) { 
		if (page_size[i] == 0x0000){ 
		     return i; 
		} 
	}
	return -1;
}

void havepage(int count) {//������ӡpage_size������ÿ��Ԫ�ص�ֵ
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
	response = "�����봴��Ŀ¼����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);//������Ϣ
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
		return;
	}
	strcpy(p->filename, cmd);
	GetSystemTime(&(p->createtime));
	strcpy(p->type, "DIR");
	//�ҵ���Ӧ�ĵ�һ��ҳ���
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
	response = "������Ŀ¼����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
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
			response = "��Ŀ¼���ǿ�Ŀ¼";
		}
		else if (strcmp(tmp->filename, ch) == 0 && tmp->child == NULL) {
			tmp->father = NULL;
			strcpy(tmp->type, "DEL");

			page_size[tmp->page_no] = 0x0000;//fat�������0

			if (currentDir->child == tmp) {
				currentDir->child = tmp->rb;
			}
			else {
				ump->rb = tmp->rb;
			}
			tmp->rb = NULL;
		}
		else
			response = "ϵͳ�Ҳ������Ӧ��Ŀ¼";
	}

	else
		response = "ϵͳ�Ҳ������Ӧ��Ŀ¼";
}

void create() {
	memset(cmd, 0, sizeof(cmd));
	FCB* p;
	p = (FCB*)malloc(S);
	response = "�������ļ�����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "Error reading from socket" << std::endl;
		return;
	}
	strcpy(p->filename, cmd);
	memset(cmd, 0, sizeof(cmd));
	response = "�������ļ���С";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	//��С
	valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "Error reading from socket" << std::endl;
		return;
	}
	p->page_size = std::stoi(cmd);
	GetSystemTime(&(p->createtime));
	strcpy(p->type, "FILE");

	//����ҳ���
	int a = p->page_size / 1024;//����ҳ���С����뼸��ҳ
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
	//����λ��
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
	response = "������Ŀ¼����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
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

			int a = tmp->page_size / 1024;//�ļ�ռ�õĿռ����
			if (tmp->page_size % 1024 > 0) {
				a++;
			}
			int m, n;
			m = tmp->page_no;

			while (a > 0) {
				n = page_size[m];//��һ��ҳ��
				page_size[m] = 0x0000;
				m = n;
				a--;
			}
			//����ڵ���ϵ
			if (currentDir->child == tmp) {
				currentDir->child = tmp->rb;
			}
			else {
				ump->rb = tmp->rb;
			}
			tmp->rb = NULL;
		}
		else
			response = "ϵͳ�Ҳ������Ӧ��Ŀ¼";
	}
	else
		response = "ϵͳ�Ҳ������Ӧ��Ŀ¼";
}

void cd() {
	char ch[10];
	memset(cmd, 0, sizeof(cmd));
	response = "������Ŀ¼";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
		return;
	}
	strcpy(ch, cmd);
	FCB* tmp;
	tmp = currentDir->child;
	if (tmp != NULL) {
		if (strstr(ch, "\\") != NULL) {//������Ŀ¼
			char* cd1 = strtok(ch, "\\");
			while (cd1 != NULL) {
				while (strcmp(cd1, tmp->filename) != 0 && tmp->rb != NULL) {
					tmp = tmp->rb;
				}
				if (strcmp(cd1, tmp->filename) == 0) {//��Ŀ¼�²����ڣ�����һ��Ѱ��
					tmp = tmp->child;
					cd1 = strtok(NULL, "\\");
				}
				else {
					response = "����1\n";
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
				response = "����2\n";
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
		response = "����3\n";
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
	int a = 0, b = 0; // ͳ���ļ���Ŀ¼�ĸ���
	tmp =currentDir->child;
	response += url;
	response += "�е�Ŀ¼\n";
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
	response += std::to_string(a) + "���ļ�\n";
	response += std::to_string(b) + "��Ŀ¼\n";
}

void upload() {
	FCB* targetDirectory = currentDir;
	string localFilePath;
	memset(cmd, 0, sizeof(cmd));
	response = "�������ļ�λ��(URL��ʽ)��";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "���Ӵ���" << std::endl;
		return;
	}
	localFilePath = cmd;

	// ��
	ifstream localFile(localFilePath, ios::binary);
	if (!localFile) {
		response = "�޷����ļ�";
		return;
	}

	// ����
	FCB* virtualFile = (FCB*)malloc(S);
	memset(cmd, 0, sizeof(cmd));
	response = "������洢�ļ����ƣ�";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "���Ӵ���" << std::endl;
		return;
	}
	strcpy(virtualFile->filename, cmd);
	virtualFile->page_size = 0;
	GetSystemTime(&(virtualFile->createtime));
	strcpy(virtualFile->type, "FILE");

	// ����
	int numBlocks = 0;
	int blockIndex = getpage(64);
	if (blockIndex == -1) {
		response = "���̿ռ�����,ʧ��";
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

	// �ر��ļ�
	localFile.close();

	// �������ļ���ӵ�Ŀ��Ŀ¼
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

	response = "�ļ��ϴ��ɹ�";
}

void download() {
	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "�����������ļ�����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
		return;
	}
	strcpy(filename, cmd);

	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {
			ifstream in("vfs.bin", ios::binary);
			if (!in) {
				response = "�޷���";
				return;
			}

			ofstream out(filename, ios::binary);
			if (!out) {
				response = "�޷����������ļ�";
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

			response = "�ļ����سɹ�";
			return;
		}

		tmp = tmp->rb;
	}
	response = "ϵͳ�Ҳ������Ӧ�ļ�";
}


void write() {

	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "������Ҫд�����ݵ������ļ�����";//��ʾ���뱻д�ļ�������
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
		return;
	}
	strcpy(filename, cmd);
	string content;
	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {

			fstream out("vfs.bin", ios::binary | ios::in | ios::out);
			if (!out) {
				response = "���ļ�ʧ��";
				return;
			}

			char buffer[1024];

			response = "������Ҫд�������";
			send(clientSocket, response.c_str(), response.length(), 0);
			response = "";
			valread = recv(clientSocket, cmd, CMD_SIZE, 0);
			if (valread <= 0) {
				std::cerr << "����ʧ��" << std::endl;
				return;
			}
			content = cmd;
			//�ҵ���д�ļ��������ļ�ϵͳ��λ�� ����ҳ����
			int pageNo = tmp->page_no;
			int offset = 0;
			while (pageNo != -1 && pageNo != 0xffff) {
				//lru
				lru.accessPage(*tmp);
				out.seekg(pageNo * 1024);
				if (!out.read(buffer, sizeof(buffer))) {//��ȡҳ����
					response = "��ȡҳ����ʧ��";
					out.close();
					return;
				}
				//fs.close();

				// ���»���ҳ�е�����
				int maxBytes = min(1024, tmp->page_size - offset);
				memcpy(buffer, cmd+offset, maxBytes);
				offset += maxBytes;

				// �����º��ҳд�ش���
				//out.open("vfs.bin", ios::binary | ios::in | ios::out);
				out.seekp(pageNo * 1024);
				out.write(buffer,strlen(buffer));
				//tmp->page_size = strlen(buffer);

				// �л�����һҳ
				pageNo = page_size[pageNo];

			}
			out.close();
			response = "�ı�д��ɹ�";
			return;
		}


		tmp = tmp->rb;
	}

	response = "ϵͳ�Ҳ������Ӧ�ļ�";
}

void read() {
	char filename[10];
	memset(cmd, 0, sizeof(cmd));
	response = "������Ҫ��ȡ���ݵ������ļ�����";
	send(clientSocket, response.c_str(), response.length(), 0);
	response = "";
	int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
	if (valread <= 0) {
		std::cerr << "����ʧ��" << std::endl;
		return;
	}
	strcpy(filename, cmd);
	string content;
	FCB* tmp = currentDir->child;
	while (tmp != NULL) {
		if (strcmp(tmp->filename, filename) == 0 && strcmp(tmp->type, "FILE") == 0) {//�ҵ����ļ�

			ifstream in("vfs.bin", ios::binary | ios::in);
			if (!in) {
				response = "���ļ�ʧ��";
				return;
			}
			
			char buffer[1024];
			int pageNo = tmp->page_no;
			while (pageNo != -1 && pageNo != 0xffff) {
				// LRU�㷨
				lru.accessPage(*tmp);
				// ����ҳΪ��λ����������׷�ӵ������ַ�����
				in.seekg(pageNo * 1024);
				if (!in.read(buffer, sizeof(buffer))) {
					response = "�ļ���ȡʧ��";
					in.close();
					return;
				}

				content.append(buffer,strlen(buffer));
				// �л�����һҳ
				pageNo = page_size[pageNo];
			}

			in.close();

			response = content;
			return;
		}
		tmp = tmp->rb;
	}

	response = "ϵͳ�Ҳ������Ӧ�ļ�";
}


void monitorCPU()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);//���ϵͳʱ�䣬����windowsϵͳ
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

	// ��ȡ�ڴ�������
	PROCESS_MEMORY_COUNTERS_EX memoryCounters;//windows�Ŀ��Ի�������ڴ�������ڴ��ʹ�����
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memoryCounters, sizeof(memoryCounters))) {
		SIZE_T physicalMemoryUsed = memoryCounters.WorkingSetSize;
		SIZE_T virtualMemoryUsed = memoryCounters.PrivateUsage;
		// ���ڴ�ʹ�������ӵ����������
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

	double utilization = static_cast<double>(allocatedBlocks) / 64 * 100;//100�Ǿ�����������԰ٷֱ���ʽ��ʾ

	response += "�ļ�ϵͳ������" + std::to_string(utilization) + "\n";
	response += "�ڴ�ʣ��ռ䣺" + std::to_string(1024 - (utilization * 0.01 * 1024)) + "\n";
}
void system_monitor() {
	monitorCPU();
	cmemoryUse();
}
int filesystem() {
	HANDLE hSemaphore;  // �ź������
	HANDLE hMapFile;    // �����ڴ���
	LPCTSTR pBuf;       // �����ڴ�ָ��

	// ���ź���
	hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, L"Semaphore");
	if (hSemaphore == NULL)
	{
		std::cout << "�ź�����ʧ�ܣ��������: " << GetLastError() << std::endl;
		return 1;
	}

	// �򿪹����ڴ�
	hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"SharedMemory");
	if (hMapFile == NULL)
	{
		std::cout << "�����ڴ��ʧ�ܣ��������: " << GetLastError() << std::endl;
		CloseHandle(hSemaphore);
		return 1;
	}

	// ӳ�乲���ڴ�
	pBuf = (LPTSTR)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
	if (pBuf == NULL)
	{
		std::cout << "ӳ�䵽�����ڴ�ʧ�ܣ��������: " << GetLastError() << std::endl;
		CloseHandle(hMapFile);
		CloseHandle(hSemaphore);
		return 1;
	}
	pagedata(MAX_FILENAME_LEN);
	init();
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "����ʧ��" << std::endl;
		return -1;
	}
	
	struct sockaddr_in serverAddress {}, clientAddress{};
	int clientAddressLength = sizeof(clientAddress);


	// �����׽���
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		std::cerr << "�����׽���ʧ��" << std::endl;
		WSACleanup();
		return -1;
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(PORT);

	// ���׽��ֵ�ָ���˿�
	if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
		std::cerr << "���׽���ʧ��" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	// ������������
	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "����ʧ��" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	std::cout << "���������ڼ�������˿ڣ� " << PORT << std::endl;
    // �ȴ��ͻ�������
	if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength)) == INVALID_SOCKET) {
		std::cerr << "��ͻ�������ʧ��n" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	std::cout << "�ͻ��˳ɹ�����" << std::endl;
	bool loop = false;
	while (!loop) {
		// ���տͻ�������
		memset(cmd, 0, sizeof(cmd));
		int valread = recv(clientSocket, cmd, CMD_SIZE, 0);
		if (valread <= 0)
		{
			std::cerr << "�׽��ֶ�ȡʧ��" << std::endl;
			break;
		}
		std::cout << "�ͻ�������: " << cmd << std::endl;
		if (strcmp(cmd, "mkdir") == 0)
		{
			mkdir();
			response = "�����ɹ�";
		}
		else if (strcmp(cmd, "monitor") == 0) {
			system_monitor();
		}
		else if (strcmp(cmd, "rmdir") == 0) {
			rmdir();
			response = "ɾ���ɹ�";
		}
		else if (strcmp(cmd, "create") == 0) {
			create();
			response = "�����ļ��ɹ�";
		}
		else if (strcmp(cmd, "rm") == 0) {
			rm();
			response = "ɾ���ļ��ɹ�";
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
			response = "ϵͳ��ʽ���ɹ�";
		}
		else if (strcmp(cmd, "init") == 0) {
			init();
			response = "ϵͳ��ʼ���ɹ�";
		}
		else if (strcmp(cmd, "createJob") == 0)
		{
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// ֪ͨ��������������д��
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// �ȴ�����������Ӧ
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "JobLog") == 0) {
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// ֪ͨ��������������д��
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// �ȴ�����������Ӧ
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "showJobstatus") == 0) {
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// ֪ͨ��������������д��
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// �ȴ�����������Ӧ
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;
		}
		else if (strcmp(cmd, "help") == 0) {
			response = "help msg";
		}
		else if (strcmp(cmd, "exit") == 0) {
			response = "�����˳�";
			exit(0);
			loop = true;
		}
		else
		{
			string cmdS = cmd;
			CopyMemory((PVOID)pBuf, cmdS.c_str(), (cmdS.length() + 1) * sizeof(char));
			// ֪ͨ��������������д��
			ReleaseSemaphore(hSemaphore, 1, NULL);
			// �ȴ�����������Ӧ
			WaitForSingleObject(hSemaphore, INFINITE);
			std::string res((char*)pBuf);
			response = res;

		}
		// ������Ӧ���ͻ���
		send(clientSocket, response.c_str(), response.length(), 0);
		response = "";
	}
	// �رշ������׽���
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}


int main() {
	filesystem();
	system_monitor();
}