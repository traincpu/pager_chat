#pragma once

#define SERVERIP4  _T("20.196.202.69")
#define BLANK _T("")
#define SERVERPORT  9000

#define SIZE_TOT 256                    // ���� ��Ŷ(��� + ������) ��ü ũ��
#define SIZE_DAT (SIZE_TOT-sizeof(int)) // ����� ������ ������ �κи��� ũ��

#define TYPE_CHAT     1000              // �޽��� Ÿ��: ä��

// ���� �޽��� ����
// sizeof(COMM_MSG) == 256
typedef struct _COMM_MSG
{
	char name[20];
	char selname[20];
	char notice[50];
	char dummy[SIZE_DAT];
} COMM_MSG;

// ä�� �޽��� ����
// sizeof(CHAT_MSG) == 256
typedef struct _CHAT_MSG
{
	char name[20];
	char selname[20];
	char notice[50];
	char msg[SIZE_DAT];
} CHAT_MSG;

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);
// ����Ʈ ��Ʈ�� ��� �Լ�
void DisplayText(const char *fmt, ...);
void DisplayText2(const char* fmt, ...);
void DisplayText3(const char* fmt, ...);