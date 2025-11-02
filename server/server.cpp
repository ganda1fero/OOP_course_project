#define SERVER_LOCAL_MODE true
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>

#include <WinSock2.h>
#include <ws2tcpip.h>  // для getaddrinfo, freeaddrinfo
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <mutex>

#include "EasyMenu.h"
#include "EasyLogs.h"

//---------------------------------------------------------- объявление классов / структур

class ServerData {
public:
	ServerData();
	~ServerData();

	int get_state();
private:
	struct serv_connection {
		SOCKET connection;
		time_t last_action;

	};

	std::mutex state_mutex;
	int state;	// состояние [0 - работает, 1 - открытие, -1 - закрытие]

	std::vector<serv_connection*> connected_vect;

};

//---------------------------------------------------------- объявление функций

bool SetupServer(SOCKET& door_sock, EasyLogs& logs);

void ServerMain(const SOCKET& door_sock, EasyLogs& logs, ServerData& server);
void ServerThread(SOCKET connection, EasyLogs& logs, ServerData& server);

void ServerMenu(ServerData& server);



//---------------------------------------------------------- main

int main() {
	EasyLogs logs("serv");
	if (logs.is_open() == false) {
		logs.create("serv");
		logs.insert(EL_SYSTEM, EL_ERROR, "Ошибка открытия логов (binary), создан новый");
	}

	SOCKET door_sock;

	if (SetupServer(door_sock, logs) == false) {
		logs.insert(EL_SYSTEM, EL_ERROR, "Неудачный запус сервера, сервер закрыт");
		logs.save();
		std::cout << "Неудачный запус сервера, сервер закрыт";

		return 0;
	}

	// (сокет сервера запущен и поставлен в прослушку)
	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер успешно запущен");

	ServerData server;
	// [запуск самого слушающего]
	
	ServerMenu(server);

	// закрытие всего
	logs.insert(EL_SYSTEM, EL_NETWORK, "Закрытие сервера...");	

	WSACleanup();
	logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер закрыт");
	logs.save();
}

//---------------------------------------------------------- функции

bool SetupServer(SOCKET& door_sock, EasyLogs& logs) {
	logs.insert(EL_SYSTEM, EL_NETWORK, "Запуск сервера...");

	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA
		logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Ошибка запуска WSA");
		return false;	// выход
	}	// WSA запущен

	if ((door_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		// Ошибка открытия сокета
		logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Ошибка открытия сокета (door_sock)");
		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	sockaddr_in door_adress;
	door_adress.sin_family = AF_INET;
	door_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE)
		door_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		door_adress.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(door_sock, (sockaddr*)&door_adress, sizeof(door_adress)) == SOCKET_ERROR) { // привязка сокета к IP:порту
		// не удалось привязать адресс к сокету
		logs.insert(EL_ERROR, EL_SYSTEM, EL_NETWORK, "Ошибка привязки IP к сокету");
		WSACleanup();

		return false;	// выход
	}	// адресс привязан к сокету

	{
		if (door_adress.sin_addr.s_addr == htonl(INADDR_ANY)) {
			// Получаем локальный IP
			char hostname[256];
			if (gethostname(hostname, sizeof(hostname)) == 0) {
				addrinfo hints{}, * info = nullptr;
				hints.ai_family = AF_INET; // IPv4 только
				if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
					for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
						sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
						std::string ip = inet_ntoa(addr->sin_addr);
						logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер слушает на IP: " + ip + ":60888");
					}
					freeaddrinfo(info);
				}
				else {
					logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Не удалось получить IP через getaddrinfo()");
				}
			}
			else {
				logs.insert(EL_SYSTEM, EL_ERROR, EL_NETWORK, "Не удалось получить имя хоста через gethostname()");
			}
		}
		else {
			// Привязка к конкретному IP
			std::string tmp_str = inet_ntoa(door_adress.sin_addr);
			logs.insert(EL_SYSTEM, EL_NETWORK, "Сервер слушает на IP: " + tmp_str + ":60888");
		}
	}

	if (listen(door_sock, 10) == SOCKET_ERROR) {
		// не уадлось поставить сокет в прослушку
		logs.insert(EL_ERROR, EL_SYSTEM, EL_NETWORK, "Сокет не уадлось постваить в прослушку (listen)");
		WSACleanup();

		return false;	// выход
	}	// поставили сокет режим слушания

	return true;
}

void ServerMain(const SOCKET& door_sock, EasyLogs& logs, ServerData& server) {

}

void ServerMenu(ServerData& server) {
	EasyMenu menu ();

}

//---------------------------------------------------------- методы классов
ServerData::ServerData() {
	state = 1;
}

ServerData::~ServerData() {

}

int ServerData::get_state() {
	std::lock_guard<std::mutex> lock(state_mutex);	// залочили 
	int tmp = state;
	return tmp;
}