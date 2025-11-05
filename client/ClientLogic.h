#ifndef CLIENTLOGIC_H
#define CLIENTLOGIC_H

#define SERVER_LOCAL_MODE true
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>

#include <WinSock2.h>
#include <ws2tcpip.h>  // для getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <mutex>

#include "EasyMenu.h"

// defines
#define NO_ROLE 0
#define STUDENT_ROLE 1
#define TEACHER_ROLE 2
#define ADMIN_ROLE 3

//-----------------------------------------------------------------------------

// классы - структруры

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
	std::mutex menu_mutex;
	EasyMenu menu_;

	std::mutex screen_info_mutex;
	screen_data screen_info_;

	std::mutex state_mutex;
	int32_t state_;
};

// функции

void ClientMenuLogic(SOCKET& door_sock, Client_data& client_data);

bool SetupClient(SOCKET& door_sock);
bool ConnectClient(SOCKET& door_sock);

void ClientThread(SOCKET& connection_sock, Client_data& client_data);

#endif