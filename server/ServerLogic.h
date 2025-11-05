#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#define SERVER_LOCAL_MODE true
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

	time_t last_action;

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

class MsgHead {
public:
	MsgHead();
	bool read_from_char(const char* ptr);
	int size_of();

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

//------------------(‘ункции чтени€ message)


#endif