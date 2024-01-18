#pragma once

#define SERVERIP4  _T("20.196.202.69")
#define BLANK _T("")
#define SERVERPORT  9000

#define SIZE_TOT 256                    // 전송 패킷(헤더 + 데이터) 전체 크기
#define SIZE_DAT (SIZE_TOT-sizeof(int)) // 헤더를 제외한 데이터 부분만의 크기

#define TYPE_CHAT     1000              // 메시지 타입: 채팅

// 공통 메시지 형식
// sizeof(COMM_MSG) == 256
typedef struct _COMM_MSG
{
	char name[20];
	char selname[20];
	char notice[50];
	char dummy[SIZE_DAT];
} COMM_MSG;

// 채팅 메시지 형식
// sizeof(CHAT_MSG) == 256
typedef struct _CHAT_MSG
{
	char name[20];
	char selname[20];
	char notice[50];
	char msg[SIZE_DAT];
} CHAT_MSG;

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);
// 에디트 컨트롤 출력 함수
void DisplayText(const char *fmt, ...);
void DisplayText2(const char* fmt, ...);
void DisplayText3(const char* fmt, ...);