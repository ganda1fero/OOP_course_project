#ifndef CLIENTLOGIC_H
#define CLIENTLOGIC_H

#define SERVER_LOCAL_MODE false
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <chrono>

#include <WinSock2.h>
#include <ws2tcpip.h>  // для getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <functional>
#include <mutex>
#include <string>
#include <sstream>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

#include <locale>
#include <codecvt>

#include "EasyMenu.h"

// defines
#define NO_ROLE 0
#define STUDENT_ROLE 1
#define TEACHER_ROLE 2
#define ADMIN_ROLE 3

#define AUTHORISATION_MENUTYPE 0

//-----------------------------------------------------------------------------

// классы - структруры

class MsgHead {
public:
	MsgHead();
	bool read_from_char(const char* ptr);
	int size_of() const;

	unsigned char first_code;	// 1b
	unsigned char second_code;	// 1b
	uint32_t third_code;		// 4b
	uint32_t msg_length;		// 4b
};

struct screen_data {
	int32_t role{ NO_ROLE };	// роль (из server role)
	int32_t type{ AUTHORISATION_MENUTYPE };	// тип, например "авторизация"
	int32_t id{ 0 };	// доп id (на всякий)
};

class Client_data {
public:
	// методы
	Client_data();

	// поля
	SOCKET door_sock;
	std::thread connect_thread;

	std::mutex menu_mutex;
	EasyMenu menu_;

	std::mutex screen_info_mutex;
	screen_data screen_info_;

	std::mutex state_mutex;
	int32_t state_;

	std::mutex is_connected_mutex;
	bool is_connected_;
};

// функции

void ClientMenuLogic(Client_data& client_data);
bool AuthorisationMenuLogic(Client_data& client_data);
void ClientClickLogic(int32_t pressed_but, Client_data& client_data);

bool SetupClient(Client_data& client_data);
bool ConnectClient(Client_data& client_data);

void ClientThread(Client_data& client_data);	// только для чтения
bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

// для ProcessMessage
bool AccessDenied(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

bool AuthorisationAs(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data, const uint32_t role_id);

bool ConfirmCreateTask(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

bool GetAllTasksForTeacher(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);
bool GetAllTasksForStudent(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

bool GetTaskInfoForTeacher(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

bool GetInputFile(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);
bool GetOutputFile(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

bool GetChangeTaskMenu(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

// менюшки
void AuthorisationMenu(Client_data& client_data, std::string text);

void TeacherMenu(Client_data& client_data, std::string text);
bool TeacherCreateNewTask(Client_data& client_data);
void TeacherAlltasks(Client_data& client_data, std::vector<std::string> buttons);
void TeacherTaskInfo(Client_data& client_data, const std::string& name, const std::string& info, const uint32_t& count_of_completes, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb, const uint32_t& butt_index);
bool TeacherDeleteConfirmMenu();
bool TeacherChangeTaskMenu(Client_data& client_data, uint32_t& butt_index, std::string& name, std::string& info, std::string& input_file, std::string& output_file, uint32_t& time_limit_ms, uint32_t& memory_limit_kb, bool& is_del_tryes);

void StudentMenu(Client_data& client_data, std::string text);
void StudentAlltasks(Client_data& client_data, const std::vector<std::string>& buttons, const std::vector<bool>& buttons_status);

// запросы
bool SendTo(Client_data& client_data, const std::vector<char>& data);

void CreateGetAllTasksMessage(std::vector<char>& vect);

void CreateAuthorisationMessage(const std::string& login, const std::string& password, std::vector<char>& vect);

void CreateNewTaskMessage(const std::string& input, const std::string& output, const std::string& name, const std::string& info, std::vector<char>& vect, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb);

void CreateGetTaskInfoMessage(std::vector<char>& vect, const uint32_t& butt_index);

void CreateDeleteTaskMessage(std::vector<char>& vect, const uint32_t& butt_index);

void CreateGetIOFileShowMessage(std::vector<char>& vect, const uint32_t& butt_index, bool is_input);

void CreateChangeTaskMessage(std::vector<char>& vect, const uint32_t& butt_index);
void CreateChangeTaskMessage(std::vector<char>& vect, const uint32_t& butt_index, const std::string& name, const std::string& info, const std::string& input_file, const std::string& output_file, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb, const bool& is_del_tryes);

// перефирия
std::string WstrToStr(const std::wstring& wstr);
std::wstring OpenFileDialog();

std::string GetAppDirectory();
void OpenFileForUser(const std::string& file_path);


#endif