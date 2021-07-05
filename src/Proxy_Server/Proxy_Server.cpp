#pragma warning( disable : 4996)
#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _AFXDLL

#include "Proxy_Sever.h"
#include <afxsock.h>
#include <afx.h>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include "assert.h"
using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define USERAGENT "HTMLGET 1.1"
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#define SIZE_BUFF 4096

CWinApp theApp;
vector<string> Blocked_websites; //Tạo biến vector cho các website cấm sử dụng


struct Web_Browser
{
	CSocket client;
	SOCKET socket;
};


//Mở file Blocked_Websites để thêm những domain vào vector
void black_list()
{
	fstream config_file;
	string str;
	
	config_file.open("Blocked_Websites.config", ios::in);
	while (!config_file.eof())
	{
		getline(config_file, str); //lấy địa chỉ domain đầu trong file Blocked_Websites.config
		fflush(stdin);
		if (str[0] == '<')
		{
			continue;
		}
		Blocked_websites.push_back(str);
	}
	config_file.close();

}

//Hàm chuyển đổi kiểu char* sang wchar*
wchar_t* Convert_charptr_to_wcharptr(char* character)
{
	wchar_t* wChar = new wchar_t[SIZE_BUFF];
	MultiByteToWideChar(CP_ACP, 0, character, -1, wChar, SIZE_BUFF);
	return wChar;
}

//Hàm Gửi text hiển thị 403 Forbidden
void Check_403_Forbidden(Web_Browser* web)
{
	fstream file;
	string buff;
	
	file.open("403_page.html", ios::in);
	while (!file.eof())
	{
		getline(file, buff);
		web->client.Send(buff.c_str(), buff.length(), 0);  //Gửi những dòng lệnh từ tập 403_page html lên web browser
	}
	file.close();
	web->client.Close();
}

//Hàm lấy domain name từ web browser
char* Domain_Name(Web_Browser* web, string domain_name)
{
	char* domain = new char[domain_name.length() + 1];
	strcpy(domain, domain_name.c_str());
	char* strtoken;
	strtoken = strtok(domain, "/"); //Lúc này biến strtoken chỉ là "http:"
	strtoken = strtok(NULL, "/"); //Lấy token lần nữa, lúc này strtoken sẽ là tên miền

	if (strtoken == NULL)
	{
		delete strtoken;
		delete []domain;
		web->client.Close();
		delete web;
		return 0;
	}

    return strtoken;
}

bool check_blacklist(Web_Browser* web, char* domain)
{
	for (int i = 0; i < Blocked_websites.size(); i++)
	{
		if (strcmp(Blocked_websites[i].c_str(), domain)==0)
		{
			Check_403_Forbidden(web);
			delete web;
			return false;
		}
	}
	return true;
}


//Đa luồng
DWORD WINAPI Multi_Thread(void* client)
{
	stringstream open;
	char str[SIZE_BUFF + 1];
	memset(str, 0, SIZE_BUFF + 1);

	Web_Browser* vClient = (Web_Browser*)client; // Tao 1 client ảo để 1 client kết nối

	vClient->client.Attach(vClient->socket);
	vClient->client.Receive(str, SIZE_BUFF, 0);

	if (strcmp(str, "") == 0)
	{
		vClient->client.Close();
		delete vClient;
		return 0;
	}
	
	string firstline;
	open.str(str);
	getline(open, firstline);

	//Lấy tên miền
	char* domain_name = Domain_Name(vClient, firstline);
	if (Domain_Name(vClient, firstline) == 0) //Nếu tên miền đó không tồn tại
	{
		return 0;
	}

	//Xem xét nếu tên miền đó có trong black list hay không:
	if (check_blacklist(vClient, domain_name) == 0)
	{
		return 0;
	}

	hostent* host = gethostbyname(domain_name);
	if (host == NULL)
	{
		vClient->client.Close();
		delete vClient;
		return 0;
	}

	cout << str;

	//Tạo client thật
	CSocket real_client;
	real_client.Create();
	real_client.Connect(Convert_charptr_to_wcharptr(domain_name), 80); //Kết nối vào port 80

	real_client.Send(str, SIZE_BUFF, 0);

	ofstream file;
	file.open("web.html", ios::binary);
	while (true)
	{
		char ch_arr[SIZE_BUFF+1];
		memset(ch_arr, 0, SIZE_BUFF);

		int value = real_client.Receive(ch_arr, SIZE_BUFF);

		if (value == 0 || value == -1)
		{
			break;
			return 0;
		}
		vClient->client.Send(ch_arr, value);

		file.write(ch_arr, SIZE_BUFF);
	}


	file.close();
	vClient->client.Close();
	real_client.Close();

	return 0;

}

int _tmain(int argc, _TCHAR* argv[])
{
	int nRetCode = 0;
	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			_tprintf(_T("Fatal Error: MFC initialization failed\n"));
			nRetCode = 1;
		}
		else
		{
			// TODO: code your application's behavior here.

			AfxSocketInit(NULL); //Khởi tạo socket

			CSocket Proxy_server; //tạo server

			Proxy_server.Create(8888); //KẾT NỐI VÀO PORT 8888 CỦA PROXY
				
			if (Proxy_server.Listen(5) == false) //Nếu lắng nghe thất bại
			{
				cout << "Cann't listen to this port" << endl;
				Proxy_server.Close();
			}

			else 
			{
				cout << "Ket noi voi Web Browser thanh cong" << endl;
				cout << endl;
				//Chạy sever
				while (true) 
				{
					//Đọc danh sách trang web bị cấm (những cái danh sách này có trong file Blocked_Websites)		
					black_list();
					//Xử lý Đa luồng
					Web_Browser* web_client = new Web_Browser;
					Proxy_server.Accept(web_client->client);
					web_client->socket = web_client->client.Detach();
					CreateThread(0, 0, Multi_Thread, web_client, 0, 0);
				}
			}
			Proxy_server.Close();
			
		}

	}

	else
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: GetModuleHandle failed\n"));
		nRetCode = 1;
	}

	return nRetCode;
}



