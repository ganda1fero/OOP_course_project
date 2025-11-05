#include "ClientLogic.h"

// методы классов

Client_data::Client_data(){
	state_ = 0;
}

// функции

bool SetupClient(SOCKET& door_sock) {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA
		return false;	// выход
	}	// WSA запущен

	if ((door_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		// Ошибка открытия сокета
		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	return true;	// успешный запуск (+ подключение)
}

bool ConnectClient(SOCKET& door_sock) {
	sockaddr_in server_adress;
	server_adress.sin_family = AF_INET;
	server_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE)
		server_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		server_adress.sin_addr.s_addr = inet_addr("26.225.195.119"); // IP адресс RadminVPN

	u_long mode = 0; // 0 = блокирующий режим
	ioctlsocket(door_sock, FIONBIO, &mode);	// поставили сокет в блокирующий режим (чисто для удобного connect)

	if (connect(door_sock, (sockaddr*)&server_adress, sizeof(server_adress)) == SOCKET_ERROR) {
		// ошибка подключения
		std::cout << "Не удалось подключиться! ";
		
		closesocket(door_sock);

		return false;
	}

	u_long mode = 1; // 1 = неблокирующий режим
	ioctlsocket(door_sock, FIONBIO, &mode);	// поставили сокет в неблокирующий режим (асинхронный)

	return true;
}