#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#define SERVER_LOCAL_MODE false
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define BANNED_ROLE 0
#define USER_ROLE 1
#define TEACHER_ROLE 2
#define ADMIN_ROLE 3

#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>
#include <string>
#include <algorithm>

#include <WinSock2.h>
#include <ws2tcpip.h>  // дл€ getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include "EasyLogs.h"

//---------------------------------------------------------- объ€вление классов / структур

struct account_note {
	uint32_t id{ 0 };
	uint32_t role{ 0 };
	std::string password{ "" };

	time_t last_action{ 0 };

	std::string first_name{ "" };
	std::string last_name{ "" };
	std::string surname{ "" };
	std::string faculty{ "" };
};

struct serv_connection {	// информаци€ о соединении
	SOCKET connection;
	sockaddr_in connection_addr;

	account_note* account_ptr{ nullptr };
	time_t last_action{ std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
};

//////////////////////////////////////////

struct cheacks
{
	bool is_good{ false };	// успешна€ попытка?

	time_t send_time{ 0 };	// врем€ отправки

	std::string info{ "" };	// доп инфа

	std::string cpp_file{ "" };	// путь к отправленному cpp файлу

	uint32_t memory_bytes{ 0 };	// количество пам€ти
	uint32_t cpu_time_ms{ 0 };	// количество времени
};

struct id_cheack
{
	std::mutex account_id_cheak_mutex;

	uint32_t account_id{ 0 };	// id аккаунта
	
	std::vector<cheacks*> all_tryes;	// все его попытки
};

struct task_note {
	std::string task_name;	// название

	std::string task_info;	// описание

	std::string input_file{ "" };	// значени€ input
	std::string output_file{ "" };	// значение€ output

	uint32_t time_limit_ms;	// ограничение времени ms
	uint32_t memory_limit_kb;	// ограничеик пам€ти kb

	std::vector<id_cheack*> checked_accounts;	// все аккаунты и их попытки
};

//////////////////////////////////////////
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

class ServerData {
public:
	// методы
	ServerData();	// конструтор по умолчанию
	~ServerData();	// деструктор

	// общее
	int get_state();
	void set_state(int new_state);

	int get_count_of_connections();

	serv_connection* add_new_connection(const SOCKET& socket, const sockaddr_in& socket_addr);
	bool del_connection(const SOCKET& socket);

	// ощие данные
	bool ReadFromFile();
	void SaveToFile();

	// методы (аккаунты)
	bool read_from_file_accounts();
	void save_to_file_accounts();
	void sort_accounts();
	bool insert_new_account(uint32_t id, uint32_t role, std::string password, std::string first_name, std::string last_name, std::string surname, std::string faculty);
	bool change_account_data(const uint32_t& nedded_id, uint32_t role, std::string first_name, std::string last_name, std::string surname, std::string faculty);
	std::vector<account_note> get_all_account_notes();
	account_note* get_account_ptr(uint32_t nedded_id);

	// методы (all_tasks)
	uint32_t get_count_of_all_tasks();
	void create_new_task(const std::string& name, const std::string& info, const std::string& input, const std::string& output, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb);

	std::mutex tasks_mutex;
	std::vector<task_note*> all_tasks;
	void __clear_all_tasks__();
	bool __read_from_file_all_tasks__();
	void __save_to_file_all_tasks__();

private:
	// пол€
	std::mutex state_mutex;
	int state_of_server;	// состо€ние [0 - работает, 1 - открытие, -1 - закрытие]

	std::mutex connected_vect_mutex;
	std::vector<serv_connection*> connected_vect;	// вектор всех подключений
	
	std::mutex accounts_mutex;
	std::vector<account_note*> accounts;
	void __clear_accounts__();
	bool __read_from_file_accounts__();
	void __save_to_file_accounts__();
	void __sort_accounts__();

	// методы

};

//---------------------------------------------------------- объ€вление функций

bool SetupServer(SOCKET& door_sock, EasyLogs& logs);

void ServerMain(SOCKET& door_sock, EasyLogs& logs, ServerData& server);
void ServerThread(serv_connection* connect_ptr, EasyLogs& logs, ServerData& server);
bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);

// ќтправка
bool SendTo(serv_connection* connect_ptr, const std::vector<char>& data, EasyLogs& logs);

//------------------(‘ункции составлени€ message)
void CreateAccessDeniedMessage(std::vector<char>& vect, std::string text);

//void CreateAuthorisationStudentMessage(std::vector<char>& vect, serv_connection* connection_ptr);
void CreateAuthorisationTeaherMessage(std::vector<char>& vect, serv_connection* connection_ptr);
//void CreateAuthorisationAdminMessage(std::vector<char>& vect, serv_connection* connection_ptr);

void CreateConfirmCreateTaskMessage(std::vector<char>& vect, serv_connection* connection_ptr);

void CreateGetAllTasksForTeacherMessage(std::vector<char>& vect, ServerData& server);
//void CreateGetAllTasksForUserMessage(std::vector<char>& vect, serv_connection* connection_ptr, ServerData& server);

void CreateGetTaskInfoForTeacherMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server);

void CreateGetInputFileMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server);
void CreateGetOutputFileMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server);

void CreateChangeTaskMessage(std::vector<char>& vect, uint32_t butt_index, ServerData& server);


//------------------(‘ункции чтени€ message)
bool ProcessAuthorisationMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessCreateNewTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessGetAllTasksMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessGetTaskInfoMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessDeleteTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessGetInputOutputFileMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessChangeTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessGetChangeTaskMenuMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);
bool ProcessChangeThatTaskMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, serv_connection* connection_ptr, ServerData& server, EasyLogs& logs);

#endif