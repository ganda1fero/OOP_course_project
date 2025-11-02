#ifndef SERVERLOGIC_H
#define SERVERLOGIC_H

#include <iostream>
#include <chrono>

#include <WinSock2.h>
#include <ws2tcpip.h>  // дл€ getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <mutex>

#include "EasyMenu.h"
#include "EasyLogs.h"

//---------------------------------------------------------- объ€вление классов / структур

class ServerData {
public:
	// методы
	ServerData();
	~ServerData();

	int get_state();
	void set_state(int new_state);

	int get_count_of_connections();

	void add_new_connection(const SOCKET& socket, time_t connect_time);

private:
	struct serv_connection {
		SOCKET connection;
		time_t last_action;

	};

	std::mutex state_mutex;
	int state;	// состо€ние [0 - работает, 1 - открытие, -1 - закрытие]


	std::mutex connected_vect_mutex;
	std::vector<serv_connection*> connected_vect;
	
	// методы
	
};

class MsgHead {
public:
	MsgHead();
	bool read_from_char(char* ptr);
	int size();

	unsigned char first_code;
	unsigned char second_code;
	uint32_t third_code;
	uint32_t msg_length;
};

//---------------------------------------------------------- объ€вление функций

bool SetupServer(SOCKET& door_sock, EasyLogs& logs);

void ServerMain(SOCKET& door_sock, EasyLogs& logs, ServerData& server);
void ServerThread(SOCKET connection, EasyLogs& logs, ServerData& server);

void ServerMenu(ServerData& server, EasyLogs& logs);
bool StopServerMenu(EasyLogs& logs);
void LogsMenu(EasyLogs& logs);

#endif  
