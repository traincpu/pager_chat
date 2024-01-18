#include "Common.h"
#include "PagerClient.h"
#include "resource.h"

/* ������ ���� ���� ���� */
static HINSTANCE     g_hInstance;     // ���α׷� �ν��Ͻ� �ڵ�
static HWND          g_hBtnSendMsg;   // [�޽��� ����] ��ư
static HWND          g_hEditStatus1;   // ���� �޽��� ��� ����
static HWND          g_hEditStatus2;	// ��밡���� �߻� ��ȣ ��� ����
static HWND          g_hEditNoticeView; // ���� ��� ����

/* ��� ���� ���� ���� */
static char          g_ipaddr[64];    // ���� IP �ּ�(���ڿ�)
static char           g_name;          // ������ ��� �г���
static char           g_selname;      // ���������� ������ �г���
static char           g_myname[10];   // �ڽ��� ������ �̸�
static HANDLE        g_hClientThread; // ������ �ڵ�
static volatile bool g_bCommStarted;  // ��� ���� ����
static SOCKET        g_sock;          // Ŭ���̾�Ʈ ����
static HANDLE        g_hReadEvent;    // �̺�Ʈ �ڵ�(1)
static HANDLE        g_hWriteEvent;   // �̺�Ʈ �ڵ�(2)

/* �޽��� ���� ���� ���� */
static CHAT_MSG      g_chatmsg;       // ä�� �޽���

/* ����������, ������, �Ϲݼ��ſ� ���� �޽��� ��°��� �ٸ��� �ϱ� ���� flag ���� */
//send_flag�� ���� 2������ ������. -> 0 : �������� ���� | 1 : ������ ������
//recv_flag�� ���� 3������ ������. -> 0 : ���������� �� ���� �� �Ϲ� ��� | 1 : ���������� �г��ӿ� �ش��Ͽ� ���� ���� �� ��� | 2 : ���������� �г��ӿ� �ش������ʾ� �� ��� �Ұ�
int send_flag = 0;
int recv_flag = 0;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// �̺�Ʈ ����(���� ��ȣ, ���ȣ ����)
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// ��ȭ���� ����
	g_hInstance = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hEditIPaddr;
	static HWND hEditName;
	static HWND hEditSelName;
	static HWND hEditNotice;
	static HWND hBtnConnect;
	static HWND hBtnSendMsg; // ���� �������� ����
	static HWND hEditMsg;
	static HWND hEditStatus1; // ���� �������� ����
	static HWND hEditStatus2;
	static HWND hEditNoticeView;

	switch (uMsg) {
	case WM_INITDIALOG:
		// ��Ʈ�� �ڵ� ���
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditName = GetDlgItem(hDlg, IDC_NAME);
		hEditName = GetDlgItem(hDlg, IDC_SELNAME);
		hEditNotice = GetDlgItem(hDlg, IDC_NOTICE);
		hBtnConnect = GetDlgItem(hDlg, IDC_CONNECT);
		hBtnSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		g_hBtnSendMsg = hBtnSendMsg; // ���� ������ ����
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		hEditStatus1 = GetDlgItem(hDlg, IDC_STATUS1);
		hEditStatus2 = GetDlgItem(hDlg, IDC_STATUS2);
		hEditNoticeView = GetDlgItem(hDlg, IDC_NOTICEVIEW);
		g_hEditStatus1 = hEditStatus1; // ���� ������ ����
		g_hEditStatus2 = hEditStatus2;
		g_hEditNoticeView = hEditNoticeView;

		// ��Ʈ�� �ʱ�ȭ
		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIP4);
		EnableWindow(g_hBtnSendMsg, FALSE);
		SendMessage(hEditMsg, EM_SETLIMITTEXT, SIZE_DAT / 2, 0);

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			SetDlgItemText(hDlg, IDC_IPADDR, SERVERIP4);
			return TRUE;

		case IDC_CONNECT: // Ŀ��Ʈ ���� ������ ���

			GetDlgItemTextA(hDlg, IDC_NAME, g_myname, sizeof(g_myname)); //������ �г������� g_myname�� ����
			if (strcmp(g_myname, "") != 0) { // ���� ������ �г��� ������ ������ �ƴҰ�� ����
				GetDlgItemTextA(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr)); //������ IP���� g_ipaddr�� ����

				// ���� ��� ������ ����
				g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
				if (g_hClientThread == NULL) exit(0);
				// ���� ���� ���� ��ٸ�
				while (g_bCommStarted == false);
				// ��Ʈ�� ���� ����
				EnableWindow(hEditIPaddr, FALSE);	//Ŀ��Ʈ ��ư ���� ���� IP�Է� �Ұ����ϵ��� ��Ȱ�����·� ����,
				EnableWindow(hBtnConnect, FALSE);                         //Ŀ��Ʈ��ư ���������� ��Ȱ�� ���·� ����,
				EnableWindow(g_hBtnSendMsg, TRUE);                        //�޽������۹�ư ���� �� �ֵ��� Ȱ�� ���·� ����

				/* ��밡���� �߻߾�ȣ ��� ��� */
				DisplayText2("%s <%s>\r\n", "8282", "��������");
				DisplayText2("%s <%s>\r\n", "0000", "����ʹ�");
				DisplayText2("%s <%s>\r\n", "0027", "������ ġ��");
				DisplayText2("%s <%s>\r\n", "0242", "���λ���");
				DisplayText2("%s <%s>\r\n", "045", "�����");
				DisplayText2("%s <%s>\r\n", "041004", "������ ����� �� ���� õ��");
				DisplayText2("%s <%s>\r\n", "075", "��ġ�� ����");
				DisplayText2("%s <%s>\r\n", "0925", "������ ����");
				DisplayText2("%s <%s>\r\n", "100", "���ƿ�");
				DisplayText2("%s <%s>\r\n", "1004", "����� õ��");
				DisplayText2("%s <%s>\r\n", "1010235", "������ ����մϴ�");
				DisplayText2("%s <%s>\r\n", "10288", "���� ����");
				DisplayText2("%s <%s>\r\n", "555555", "ȣ������~");
				DisplayText2("%s <%s>\r\n", "108108", "������ ��ν�����");
				DisplayText2("%s <%s>\r\n", "112", "��޻�Ȳ");
				DisplayText2("%s <%s>\r\n", "2848", "���ǻ���");
				DisplayText2("%s <%s>\r\n", "2929", "¥������");
				DisplayText2("%s <%s>\r\n", "337337", "������");
				DisplayText2("%s <%s>\r\n", "505", "SOS");
				DisplayText2("%s <%s>\r\n", "5114", "���ù㿡 ��ȭ�ض�");
				DisplayText2("%s <%s>\r\n", "515", "��~ �̷�");
				DisplayText2("%s <%s>\r\n", "5200", "������ ���� �� ���� ���Դϴ�");
				DisplayText2("%s <%s>\r\n", "5233", "�̾��ؿ�");
				DisplayText2("%s <%s>\r\n", "7115", "�����ؿ�");
				DisplayText2("%s <%s>\r\n", "7179", "ģ��ģ��");
				DisplayText2("%s <%s>\r\n", "79337", "ģ���� ����!");
				return TRUE;
			}
			else { //���� �г����� �����ϰ�� ���� �޽��� ���
				MessageBox(NULL, _T("�г����� �Է����ּ���!"), _T("�˸�"), MB_ICONERROR);
				return FALSE;
			}

		case IDC_SENDMSG: //���۹�ư ������ ���
			GetDlgItemTextA(hDlg, IDC_NAME, g_myname, sizeof(g_myname)); // �г��� ������ : �޽������۽� �г����Է¶��� �ִ� ���ڷ� �г��� ����
			if (strcmp(g_myname, "") != 0) { // �޾ƿ� �г����� ������ �ƴϸ� �Ʒ� ����
				// ������ ���� ä�� �޽��� �б� �ϷḦ ��ٸ�
				WaitForSingleObject(g_hReadEvent, INFINITE);
				GetDlgItemTextA(hDlg, IDC_NAME, g_chatmsg.name, 20); // �޽��� ������ �г�������
				GetDlgItemTextA(hDlg, IDC_SELNAME, g_chatmsg.selname, 20); // ���������� �г��� ����
				GetDlgItemTextA(hDlg, IDC_NOTICE, g_chatmsg.notice, 50); // �������� ����
				GetDlgItemTextA(hDlg, IDC_MSG, g_chatmsg.msg, SIZE_DAT);	// ���ο� ä�� �޽����� ��� ���� �ϷḦ �˸�
				SetDlgItemText(hDlg, IDC_MSG, BLANK);	   // ���� �� �޽��� �ؽ�Ʈ�ʵ� �������� ����          
				SetDlgItemText(hDlg, IDC_SELNAME, BLANK);  // ���� �� ���������� �ؽ�Ʈ�ʵ� �������� ����
				SetDlgItemText(hDlg, IDC_NOTICE, BLANK);   // ���� �� ���� �ؽ�Ʈ�ʵ� �������� ����
				SetEvent(g_hWriteEvent);
				// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
				SendMessage(hEditMsg, EM_SETSEL, 0, -1);
				send_flag = 1; // �������Ƿ� 1����
				return TRUE;
			}
			else {	// �г����� ������ ���
				MessageBox(NULL, _T("�г����� �Է����ּ���!"), _T("�˸�"), MB_ICONERROR);
				return FALSE;
			}

		case IDCANCEL: //������� X��ư ���� ��� : ����
			closesocket(g_sock);
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
	}
	return FALSE;
}

// ���� ��� ������ �Լ�(1) - ����
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

	MessageBox(NULL, _T("������ �����߽��ϴ�."), _T("�˸�"), MB_ICONINFORMATION);

	// �б� & ���� ������ ����
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) exit(1);
	g_bCommStarted = true;

	// ������ ���� ��� (�� �� �ϳ��� ������ ������)
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	MessageBox(NULL, _T("������ ������ϴ�."), _T("�˸�"), MB_ICONERROR);

	g_bCommStarted = false;
	closesocket(g_sock);
	return 0;
}

// ���� ��� ������ �Լ�(2) - ������ ����
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

		chat_msg = (CHAT_MSG*)&comm_msg; //������ ���Ź��� ���� chat_msg�� ����
		if (strcmp(chat_msg->notice, "") != 0) // ���� ������ ���� ���� ���� ���
			DisplayText3("%s : %s\r\n", chat_msg->name, chat_msg->notice);

		/* �޽��� ������� ���� */
		if (strcmp(g_myname, chat_msg->selname) == 0) // ������ �г����� ���������� �г��Ӱ� ������ 1����
			recv_flag = 1;
		else if (strcmp(chat_msg->selname, "") != 0 && strcmp(g_myname, chat_msg->selname) != 0) // ���������� �г��Ӱ��� �����ϰ�, ������ �г����� ���������� �г��Ӱ� �ٸ��� 2����
			recv_flag = 2;

		if (recv_flag == 1 || (send_flag == 1 && strcmp(chat_msg->selname, "") != 0)) { // ������ ���������� �г��ӿ� �ش� �Ǵ� (�ڽ��� �޽����� �����߰�, �������� ���� �����ϴ� ���)
			if (strcmp(chat_msg->msg, "8282") == 0)								// �߻߾�ȣ �ص��Ͽ� ����ϰ�, �� ������ �����ϰ� ���
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����",chat_msg->name, chat_msg->msg, "��������");
			else if (strcmp(chat_msg->msg, "0000") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "����ʹ�");
			else if (strcmp(chat_msg->msg, "0027") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ġ��");
			else if (strcmp(chat_msg->msg, "0242") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "���λ���");
			else if (strcmp(chat_msg->msg, "045") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "�����");
			else if (strcmp(chat_msg->msg, "041004") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ����� �� ������ õ��");
			else if (strcmp(chat_msg->msg, "075") == 0 || strcmp(chat_msg->msg, "07590") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "��ġ�� ����");
			else if (strcmp(chat_msg->msg, "0925") == 0 || strcmp(chat_msg->msg, "092590") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ����");
			else if (strcmp(chat_msg->msg, "100") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "���ƿ�");
			else if (strcmp(chat_msg->msg, "1004") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "����� õ��");
			else if (strcmp(chat_msg->msg, "1010235") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ����մϴ�");
			else if (strcmp(chat_msg->msg, "10288") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "���� ����");
			else if (strcmp(chat_msg->msg, "555555") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "ȣ������~");
			else if (strcmp(chat_msg->msg, "108108") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ��ν�����");
			else if (strcmp(chat_msg->msg, "112") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "��޻�Ȳ");
			else if (strcmp(chat_msg->msg, "2848") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "���ǻ���");
			else if (strcmp(chat_msg->msg, "2929") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "¥������");
			else if (strcmp(chat_msg->msg, "337337") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������");
			else if (strcmp(chat_msg->msg, "505") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "SOS");
			else if (strcmp(chat_msg->msg, "5114") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "���ù㿡 ��ȭ�ض�");
			else if (strcmp(chat_msg->msg, "515") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "��~ �̷�");
			else if (strcmp(chat_msg->msg, "5200") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "������ ���� �� ���� ���Դϴ�");
			else if (strcmp(chat_msg->msg, "5233") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "�̾��ؿ�");
			else if (strcmp(chat_msg->msg, "7115") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "�����ؿ�");
			else if (strcmp(chat_msg->msg, "7179") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "ģ��ģ��");
			else if (strcmp(chat_msg->msg, "79337") == 0)
				DisplayText("#%s# [%s] : %s <%s>\r\n", "����", chat_msg->name, chat_msg->msg, "ģ���� ����!");
			else  //�߻� �޽����� �ƴ� ���
				DisplayText("#%s# [%s] : %s\r\n", "����", chat_msg->name, chat_msg->msg);

		}
		else if (recv_flag == 2)	// �ڽ��� �г����� ���������� �г��ӿ� �ش����� �ʴ� ��� -> �޽����� ���� �� ����
			DisplayText("%s\r\n", "**�޽����� ��ȣȭ�Ǿ� ���� �� �����ϴ�!**");
		else					// recv_flag�� 0�� ��� ->  ��ü�� ���� �� ���� -> ���������Ⱑ �ƴϹǷ� �߻߾�ȣ�ص� ���� ��� ����
			DisplayText("              [%s] : %s\r\n", chat_msg->name, chat_msg->msg);

		send_flag = 0;
		recv_flag = 0;

	}
	return 0;
}

// ���� ��� ������ �Լ�(3) - ������ �۽�
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteEvent, INFINITE);
		// ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(g_chatmsg.msg) == 0) {
			// [�޽��� ����] ��ư Ȱ��ȭ
			EnableWindow(g_hBtnSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}
		// ������ ������
		retval = send(g_sock, (char*)&g_chatmsg, SIZE_TOT, 0);
		if (retval == SOCKET_ERROR) break;
		// [�޽��� ����] ��ư Ȱ��ȭ
		EnableWindow(g_hBtnSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}
	return 0;
}

// ����Ʈ ��Ʈ�� ��� �Լ�
void DisplayText(const char* fmt, ...) // ��� â�� �ۼ��� �޽��� ����ϴ� �Լ�
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

void DisplayText2(const char* fmt, ...) // ��� â�� ���۰��� �߻߾�ȣ ����ϴ� �Լ�
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

void DisplayText3(const char* fmt, ...) // ��� â�� �������� ����ϴ� �Լ�
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
