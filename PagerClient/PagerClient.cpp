#include "Common.h"
#include "PagerClient.h"
#include "resource.h"

/* 윈도우 관련 전역 변수 */
static HINSTANCE     g_hInstance;     // 프로그램 인스턴스 핸들
static HWND          g_hBtnSendMsg;   // [메시지 전송] 버튼
static HWND          g_hEditStatus1;   // 각종 메시지 출력 영역
static HWND          g_hEditStatus2;	// 사용가능한 삐삐 암호 출력 영역
static HWND          g_hEditNoticeView; // 공지 출력 영역

/* 통신 관련 전역 변수 */
static char          g_ipaddr[64];    // 서버 IP 주소(문자열)
static char           g_name;          // 전송한 사람 닉네임
static char           g_selname;      // 몰래보내기 선택한 닉네임
static char           g_myname[10];   // 자신의 고유한 이름
static HANDLE        g_hClientThread; // 스레드 핸들
static volatile bool g_bCommStarted;  // 통신 시작 여부
static SOCKET        g_sock;          // 클라이언트 소켓
static HANDLE        g_hReadEvent;    // 이벤트 핸들(1)
static HANDLE        g_hWriteEvent;   // 이벤트 핸들(2)

/* 메시지 관련 전역 변수 */
static CHAT_MSG      g_chatmsg;       // 채팅 메시지

/* 몰래보내기, 보낸이, 일반수신에 따라 메시지 출력값이 다르게 하기 위한 flag 변수 */
//send_flag는 값이 2가지로 나뉜다. -> 0 : 전송하지 않음 | 1 : 본인이 전송함
//recv_flag는 값이 3가지로 나뉜다. -> 0 : 몰래보내기 값 없을 때 일반 출력 | 1 : 몰래보내기 닉네임에 해당하여 몰래 받은 값 출력 | 2 : 몰래보내기 닉네임에 해당하지않아 값 출력 불가
int send_flag = 0;
int recv_flag = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// 이벤트 생성(각각 신호, 비신호 상태)
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// 대화상자 생성
	g_hInstance = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// 이벤트 제거
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 대화상자 프로시저
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEditIPaddr;
	static HWND hEditName;
	static HWND hEditSelName;
	static HWND hEditNotice;
	static HWND hBtnConnect;
	static HWND hBtnSendMsg; // 전역 변수에도 저장
	static HWND hEditMsg;
	static HWND hEditStatus1; // 전역 변수에도 저장
	static HWND hEditStatus2;
	static HWND hEditNoticeView;

	switch (uMsg) {
	case WM_INITDIALOG:
		// 컨트롤 핸들 얻기
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditName = GetDlgItem(hDlg, IDC_NAME);
		hEditName = GetDlgItem(hDlg, IDC_SELNAME);
		hEditNotice = GetDlgItem(hDlg, IDC_NOTICE);
		hBtnConnect = GetDlgItem(hDlg, IDC_CONNECT);
		hBtnSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		g_hBtnSendMsg = hBtnSendMsg; // 전역 변수에 저장
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		hEditStatus1 = GetDlgItem(hDlg, IDC_STATUS1);
		hEditStatus2 = GetDlgItem(hDlg, IDC_STATUS2);
		hEditNoticeView = GetDlgItem(hDlg, IDC_NOTICEVIEW);
		g_hEditStatus1 = hEditStatus1; // 전역 변수에 저장
		g_hEditStatus2 = hEditStatus2;
		g_hEditNoticeView = hEditNoticeView;

		// 컨트롤 초기화
		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIP4);
		EnableWindow(g_hBtnSendMsg, FALSE);
		SendMessage(hEditMsg, EM_SETLIMITTEXT, SIZE_DAT / 2, 0);

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			SetDlgItemText(hDlg, IDC_IPADDR, SERVERIP4);
			return TRUE;

		case IDC_CONNECT: // 커넥트 변수 눌렀을 경우

			GetDlgItemTextA(hDlg, IDC_NAME, g_myname, sizeof(g_myname)); //설정한 닉네임정보 g_myname에 저장
			if (strcmp(g_myname, "") != 0) { // 만약 설정한 닉네임 정보가 공백이 아닐경우 진행
				GetDlgItemTextA(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr)); //설정한 IP정보 g_ipaddr에 저장

				// 소켓 통신 스레드 시작
				g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
				if (g_hClientThread == NULL) exit(0);
				// 서버 접속 성공 기다림
				while (g_bCommStarted == false);
				// 컨트롤 상태 변경
				EnableWindow(hEditIPaddr, FALSE);	//커넥트 버튼 누른 이후 IP입력 불가능하도록 비활성상태로 변경,
				EnableWindow(hBtnConnect, FALSE);                         //커넥트버튼 못누르도록 비활성 상태로 변경,
				EnableWindow(g_hBtnSendMsg, TRUE);                        //메시지전송버튼 누를 수 있도록 활성 상태로 변경

				/* 사용가능한 삐삐암호 목록 출력 */
				DisplayText2("%s <%s>\r\n", "8282", "빨리빨리");
				DisplayText2("%s <%s>\r\n", "0000", "보고싶다");
				DisplayText2("%s <%s>\r\n", "0027", "땡땡이 치자");
				DisplayText2("%s <%s>\r\n", "0242", "연인사이");
				DisplayText2("%s <%s>\r\n", "045", "빵사와");
				DisplayText2("%s <%s>\r\n", "041004", "영원히 사랑해 넌 나의 천사");
				DisplayText2("%s <%s>\r\n", "075", "공치러 가자");
				DisplayText2("%s <%s>\r\n", "0925", "볼링장 가자");
				DisplayText2("%s <%s>\r\n", "100", "돌아와");
				DisplayText2("%s <%s>\r\n", "1004", "당신의 천사");
				DisplayText2("%s <%s>\r\n", "1010235", "열렬히 사모합니다");
				DisplayText2("%s <%s>\r\n", "10288", "열이 펄펄");
				DisplayText2("%s <%s>\r\n", "555555", "호오오오~");
				DisplayText2("%s <%s>\r\n", "108108", "난지금 고민스러워");
				DisplayText2("%s <%s>\r\n", "112", "긴급상황");
				DisplayText2("%s <%s>\r\n", "2848", "이판사판");
				DisplayText2("%s <%s>\r\n", "2929", "짜증난다");
				DisplayText2("%s <%s>\r\n", "337337", "힘내라");
				DisplayText2("%s <%s>\r\n", "505", "SOS");
				DisplayText2("%s <%s>\r\n", "5114", "오늘밤에 전화해라");
				DisplayText2("%s <%s>\r\n", "515", "오~ 이런");
				DisplayText2("%s <%s>\r\n", "5200", "오늘은 잊을 수 없는 날입니다");
				DisplayText2("%s <%s>\r\n", "5233", "미안해요");
				DisplayText2("%s <%s>\r\n", "7115", "축하해요");
				DisplayText2("%s <%s>\r\n", "7179", "친한친구");
				DisplayText2("%s <%s>\r\n", "79337", "친구야 힘내!");
				return TRUE;
			}
			else { //만약 닉네임이 공백일경우 에러 메시지 출력
				MessageBox(NULL, _T("닉네임을 입력해주세요!"), _T("알림"), MB_ICONERROR);
				return FALSE;
			}

		case IDC_SENDMSG: //전송버튼 눌렀을 경우
			GetDlgItemTextA(hDlg, IDC_NAME, g_myname, sizeof(g_myname)); // 닉네임 변경기능 : 메시지전송시 닉네임입력란에 있는 문자로 닉네임 변경
			if (strcmp(g_myname, "") != 0) { // 받아온 닉네임이 공백이 아니면 아래 실행
				// 이전에 얻은 채팅 메시지 읽기 완료를 기다림
				WaitForSingleObject(g_hReadEvent, INFINITE);
				GetDlgItemTextA(hDlg, IDC_NAME, g_chatmsg.name, 20); // 메시지 전송자 닉네임저장
				GetDlgItemTextA(hDlg, IDC_SELNAME, g_chatmsg.selname, 20); // 몰래보내기 닉네임 저장
				GetDlgItemTextA(hDlg, IDC_NOTICE, g_chatmsg.notice, 50); // 공지내용 저장
				GetDlgItemTextA(hDlg, IDC_MSG, g_chatmsg.msg, SIZE_DAT);	// 새로운 채팅 메시지를 얻고 쓰기 완료를 알림
				SetDlgItemText(hDlg, IDC_MSG, BLANK);	   // 저장 후 메시지 텍스트필드 공백으로 변경          
				SetDlgItemText(hDlg, IDC_SELNAME, BLANK);  // 저장 후 몰래보내기 텍스트필드 공백으로 변경
				SetDlgItemText(hDlg, IDC_NOTICE, BLANK);   // 저장 후 공지 텍스트필드 공백으로 변경
				SetEvent(g_hWriteEvent);
				// 입력된 텍스트 전체를 선택 표시
				SendMessage(hEditMsg, EM_SETSEL, 0, -1);
				send_flag = 1; // 보냈으므로 1저장
				return TRUE;
			}
			else {	// 닉네임이 공백일 경우
				MessageBox(NULL, _T("닉네임을 입력해주세요!"), _T("알림"), MB_ICONERROR);
				return FALSE;
			}

		case IDCANCEL: //우측상단 X버튼 누를 경우 : 종료
			closesocket(g_sock);
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
	}
	return FALSE;
}

// 소켓 통신 스레드 함수(1) - 메인
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;


	g_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (g_sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	inet_pton(AF_INET, g_ipaddr, &serveraddr.sin_addr);
	serveraddr.sin_port = htons(9000);
	retval = connect(g_sock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	MessageBox(NULL, _T("서버에 접속했습니다."), _T("알림"), MB_ICONINFORMATION);

	// 읽기 & 쓰기 스레드 생성
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) exit(1);
	g_bCommStarted = true;

	// 스레드 종료 대기 (둘 중 하나라도 종료할 때까지)
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	MessageBox(NULL, _T("연결이 끊겼습니다."), _T("알림"), MB_ICONERROR);

	g_bCommStarted = false;
	closesocket(g_sock);
	return 0;
}

// 소켓 통신 스레드 함수(2) - 데이터 수신
DWORD WINAPI ReadThread(LPVOID arg)
{
	int retval;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;

	while (1) {
		retval = recv(g_sock, (char*)&comm_msg, SIZE_TOT, MSG_WAITALL);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		chat_msg = (CHAT_MSG*)&comm_msg; //데이터 수신받을 것으 chat_msg로 전달
		if (strcmp(chat_msg->notice, "") != 0) // 받은 공지가 있을 때만 공지 출력
			DisplayText3("%s : %s\r\n", chat_msg->name, chat_msg->notice);

		/* 메시지 출력형식 지정 */
		if (strcmp(g_myname, chat_msg->selname) == 0) // 본인의 닉네임이 몰래보내기 닉네임과 같으면 1저장
			recv_flag = 1;
		else if (strcmp(chat_msg->selname, "") != 0 && strcmp(g_myname, chat_msg->selname) != 0) // 몰래보내기 닉네임값이 존재하고, 본인의 닉네임이 몰래보내기 닉네임과 다르면 2저장
			recv_flag = 2;

		if (recv_flag == 1 || (send_flag == 1 && strcmp(chat_msg->selname, "") != 0)) { // 본인이 몰래보내기 닉네임에 해당 또는 (자신이 메시지를 전송했고, 몰래보낸 값이 존재하는 경우)
			if (strcmp(chat_msg->msg, "8282") == 0)								// 삐삐암호 해독하여 출력하고, 그 대상과만 은밀하게 통신
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래",chat_msg->name, chat_msg->msg, "빨리빨리");
			else if (strcmp(chat_msg->msg, "0000") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "보고싶다");
			else if (strcmp(chat_msg->msg, "0027") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "땡땡이 치자");
			else if (strcmp(chat_msg->msg, "0242") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "연인사이");
			else if (strcmp(chat_msg->msg, "045") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "빵사와");
			else if (strcmp(chat_msg->msg, "041004") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "영원히 사랑해 넌 나만의 천사");
			else if (strcmp(chat_msg->msg, "075") == 0 || strcmp(chat_msg->msg, "07590") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "공치러 가자");
			else if (strcmp(chat_msg->msg, "0925") == 0 || strcmp(chat_msg->msg, "092590") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "볼링장 가자");
			else if (strcmp(chat_msg->msg, "100") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "돌아와");
			else if (strcmp(chat_msg->msg, "1004") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "당신의 천사");
			else if (strcmp(chat_msg->msg, "1010235") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "열렬히 사모합니다");
			else if (strcmp(chat_msg->msg, "10288") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "열이 펄펄");
			else if (strcmp(chat_msg->msg, "555555") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "호오오오~");
			else if (strcmp(chat_msg->msg, "108108") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "난지금 고민스러워");
			else if (strcmp(chat_msg->msg, "112") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "긴급상황");
			else if (strcmp(chat_msg->msg, "2848") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "이판사판");
			else if (strcmp(chat_msg->msg, "2929") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "짜증난다");
			else if (strcmp(chat_msg->msg, "337337") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "힘내라");
			else if (strcmp(chat_msg->msg, "505") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "SOS");
			else if (strcmp(chat_msg->msg, "5114") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "오늘밤에 전화해라");
			else if (strcmp(chat_msg->msg, "515") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "오~ 이런");
			else if (strcmp(chat_msg->msg, "5200") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "오늘은 잊을 수 없는 날입니다");
			else if (strcmp(chat_msg->msg, "5233") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "미안해요");
			else if (strcmp(chat_msg->msg, "7115") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "축하해요");
			else if (strcmp(chat_msg->msg, "7179") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "친한친구");
			else if (strcmp(chat_msg->msg, "79337") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "몰래", chat_msg->name, chat_msg->msg, "친구야 힘내!");
			else  //삐삐 메시지가 아닐 경우
				DisplayText("#%s# [%s] : %s\r\n", "몰래", chat_msg->name, chat_msg->msg);

		}
		else if (recv_flag == 2)	// 자신의 닉네임이 몰래보내기 닉네임에 해당하지 않는 경우 -> 메시지를 읽을 수 없음
			DisplayText("%s\r\n", "**메시지가 암호화되어 읽을 수 없습니다!**");
		else					// recv_flag가 0인 경우 ->  전체가 읽을 수 있음 -> 몰래보내기가 아니므로 삐삐암호해독 같이 출력 안함
			DisplayText("              [%s] : %s\r\n", chat_msg->name, chat_msg->msg);

		send_flag = 0;
		recv_flag = 0;

	}
	return 0;
}

// 소켓 통신 스레드 함수(3) - 데이터 송신
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// 서버와 데이터 통신
	while (1) {
		// 쓰기 완료 기다리기
		WaitForSingleObject(g_hWriteEvent, INFINITE);
		// 문자열 길이가 0이면 보내지 않음
		if (strlen(g_chatmsg.msg) == 0) {
			// [메시지 전송] 버튼 활성화
			EnableWindow(g_hBtnSendMsg, TRUE);
			// 읽기 완료 알리기
			SetEvent(g_hReadEvent);
			continue;
		}
		// 데이터 보내기
		retval = send(g_sock, (char*)&g_chatmsg, SIZE_TOT, 0);
		if (retval == SOCKET_ERROR) break;
		// [메시지 전송] 버튼 활성화
		EnableWindow(g_hBtnSendMsg, TRUE);
		// 읽기 완료 알리기
		SetEvent(g_hReadEvent);
	}
	return 0;
}

// 에디트 컨트롤 출력 함수
void DisplayText(const char* fmt, ...) // 출력 창에 송수신 메시지 출력하는 함수
{
	va_list arg;
	va_start(arg, fmt);
	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);
	va_end(arg);

	int nLength = GetWindowTextLength(g_hEditStatus1);
	SendMessage(g_hEditStatus1, EM_SETSEL, nLength, nLength);
	SendMessageA(g_hEditStatus1, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
}

void DisplayText2(const char* fmt, ...) // 출력 창에 전송가능 삐삐암호 출력하는 함수
{
	va_list arg;
	va_start(arg, fmt);
	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);
	va_end(arg);

	int nLength = GetWindowTextLength(g_hEditStatus2);
	SendMessage(g_hEditStatus2, EM_SETSEL, nLength, nLength);
	SendMessageA(g_hEditStatus2, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
}

void DisplayText3(const char* fmt, ...) // 출력 창에 공지내용 출력하는 함수
{
	va_list arg;
	va_start(arg, fmt);
	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);
	va_end(arg);

	int nLength = GetWindowTextLength(g_hEditNoticeView);
	SendMessage(g_hEditNoticeView, EM_SETSEL, nLength, nLength);
	SendMessageA(g_hEditNoticeView, EM_REPLACESEL, FALSE, (LPARAM)cbuf);
}
