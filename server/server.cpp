#define SERVER_LOCAL_MODE true
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>

#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib") // доподключаем реализацию библиотеку WSA

#include <thread>
#include <mutex>

#include "EasyMenu.h"
#include "EasyLogs.h"




bool SetupServer(SOCKET& door_sock);

int main() {
	EasyLogs test("test");
	if (test.is_open() == false)
		test.create("test");
	test.insert(EL_NETWORK, "привет");

	EasyLogs test_2;

	std::vector<char> data;

	test.select_all(EL_SYSTEM, data);

	test_2.open_via_char(data);

	test_2.print_all();
	return 0;
}

//----------------------------------------------------------

bool SetupServer(SOCKET& door_sock) {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA

		return false;	// выход
	}	// WSA запущен

	if (door_sock = socket(AF_INET, SOCK_STREAM, 0) == INVALID_SOCKET) {
		// Ошибка открытия сокета

		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	sockaddr_in door_adress;
	door_adress.sin_family = AF_INET;
	door_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE == true)
		door_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		door_adress.sin_addr.s_addr = htonl(ADDR_ANY);
	
	if (bind(door_sock, (sockaddr*)&door_adress, sizeof(door_adress)) == SOCKET_ERROR) { // привязка сокета к IP:порту
		// не удалось привязать адресс к сокету

		WSACleanup();

		return false;	// выход
	}	// адресс привязан к сокету

	if (listen(door_sock, 10) == SOCKET_ERROR) {
		// не уадлось поставить сокет в прослушку

		WSACleanup();

		return false;	// выход
	}	// поставили сокет режим слушания

	return true;
}