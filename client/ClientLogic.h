#ifndef CLIENTLOGIC_H
#define CLIENTLOGIC_H

#define SERVER_LOCAL_MODE true
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>

#include <WinSock2.h>
#include <ws2tcpip.h>  // для getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <functional>
#include <mutex>

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
	int32_t role{ 0 };	// роль (из server role)
	int32_t type{ 0 };	// тип, например "авторизация"
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

bool SetupClient(Client_data& client_data);
bool ConnectClient(Client_data& client_data);

void ClientThread(Client_data& client_data);	// только для чтения
bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

// для ProcessMessage
bool AccessDenied(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data);

// менюшки
void AuthorisationMenu(Client_data& client_data, std::string text);

#endif