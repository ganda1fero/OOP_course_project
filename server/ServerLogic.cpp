#define SERVER_LOCAL_MODE true
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "ServerLogic.h"

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

	u_long mode = 1; // 1 = неблокирующий режим
	ioctlsocket(door_sock, FIONBIO, &mode);

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

void ServerMain(SOCKET& door_sock, EasyLogs& logs, ServerData& server) {
	SOCKET connection;
	sockaddr_in connection_addr;
	int size_of_connection_addr = sizeof(connection_addr);

	while (true) {	// беск цикл (работа сервера)
		if (server.get_state() == -1)
			break;	// закрытие сервера

		connection = accept(door_sock, (sockaddr*)&connection_addr, &size_of_connection_addr); // неблокирующий accept

		if (connection == INVALID_SOCKET) {
			if (WSAGetLastError() == WSAEWOULDBLOCK)  // просто нет новых клиентов - ждем
				Sleep(50);
			else	// значит какая-то ошибка!
				logs.insert(EL_ERROR, EL_NETWORK, "Ошибка установки соединения");
		}
		else {	// есть новое подключение
			{
				std::string clientIP = inet_ntoa(connection_addr.sin_addr);
				logs.insert(EL_NETWORK, "Установлено соединение с " + clientIP + ", число потоков: " + std::to_string(server.get_count_of_connections() + 1));
			}

			u_long mode = 1;
			ioctlsocket(connection, FIONBIO, &mode);	// сделали поток неблокирующим

			server.add_new_connection(connection, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

			std::thread t(ServerThread, connection, logs, server);	// создали поток
			t.detach();	// отсоединили поток
		}
	}

	// остановка сервера
	logs.insert(EL_SYSTEM, EL_NETWORK, "Остановка сервера...");

	closesocket(door_sock);
	
	while (server.get_count_of_connections() > 0)
		Sleep(500);
	
	logs.insert(EL_NETWORK, "Все соединения закрыты");

	return;
}

void ServerThread(SOCKET connection, EasyLogs& logs, ServerData& server) {	// тело самого клиент-сервера

}

void ServerMenu(ServerData& server) {
	EasyMenu menu();

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

void ServerData::set_state(int new_state) {
	if (new_state > 1 || new_state < -1)
		new_state = -1;

	std::lock_guard<std::mutex> lock(state_mutex);
	state = new_state;
}

int ServerData::get_count_of_connections() {
	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	uint32_t tmp = connected_vect.size();
	return tmp;
}

void ServerData::add_new_connection(const SOCKET& socket, time_t connect_time) {
	serv_connection* tmp_ptr = new serv_connection;
	tmp_ptr->connection = socket;
	tmp_ptr->last_action = connect_time;

	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	connected_vect.push_back(tmp_ptr);
}

MsgHead::MsgHead() {
	first_code = 0;
	second_code = 0;
	third_code = 0;
	msg_length = 0;
}

bool MsgHead::read_from_char(char* ptr) {
	uint32_t ptr_index = 0;
	try {
		first_code = *reinterpret_cast<unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(first_code);

		second_code = *reinterpret_cast<unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(second_code);

		third_code = *reinterpret_cast<uint32_t*>(ptr + ptr_index);
		ptr_index += sizeof(third_code);

		msg_length = *reinterpret_cast<uint32_t*>(ptr + ptr_index);
	}
	catch (...) {
		first_code = 0;
		second_code = 0;
		third_code = 0;
		msg_length = 0;
		
		return false;
	}

	return true;
}