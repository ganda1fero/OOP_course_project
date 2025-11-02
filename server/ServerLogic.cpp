#define SERVER_LOCAL_MODE true
#define PASSWORD_FOR_EXIT "Exit_now"
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

			std::thread t(ServerThread, connection, std::ref(logs), std::ref(server));	// создали поток
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



	// закрытие соединения (потока)
	server.del_connection(connection);
}

void ServerMenu(ServerData& server, EasyLogs& logs) {
	EasyMenu menu("Управление аккаунтами", "Просмотр логов", "Остановить сервер");
	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);
	menu.set_color(2, BLUE_COLOR);
	
	menu.set_notification(0, "(В разработке)");

	while (true) {
		switch (menu.easy_run())
		{
		case 0:	// упр. аккаунтами

			break;
		case 1:	// просмотр логов
			LogsMenu(logs);
			break;
		case 2:	// остановка сервера
			if (StopServerMenu(logs)) {
				logs.insert(EL_SYSTEM, "Введен пароль остановки сервера [SERVER]");
				server.set_state(-1);
				return;	// выход из меню
			}
			break;
		}
	}
}

bool StopServerMenu(EasyLogs& logs) {
	EasyMenu menu("Остановить", "Назад");
	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);
	menu.insert_text(0, "Для закрытия сервера необходимо ввести пароль");
	menu.set_color(0, RED_COLOR);
	menu.insert_advanced_cin(1, "пароль:");
	menu.set_advanced_cin_ban_not_allowed_on(1);
	menu.set_advanced_cin_new_allowed_chars(1, "qwertyuiopasdfghjklzxcvbnm1234567890QWERTYUIOPASDFGHJKLZXCVBNM1234567890_");
	menu.set_advanced_cin_secure_input_on(1);
	menu.set_color(2, YELLOW_COLOR);

	while (true) {
		switch (menu.easy_run())
		{
		case 1:	// ввод
			if (menu.get_advanced_cin_input(1) == PASSWORD_FOR_EXIT)
				return true;
			else {
				logs.insert(EL_SECURITY, EL_SYSTEM, "Введен неверный пароль для отключения сервера [SERVER]");
				menu.set_notification(1, "(неверный пароль)");
				menu.set_notification_color(1, YELLOW_COLOR);
			}
			break;
		case 2:	// назад
			return false;
			break;
		}
	}
}

void LogsMenu(EasyLogs& logs) {
	EasyMenu menu;
	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);
	menu.push_back_text("Выберите 1 или все из категорий");

	menu.push_back_checkbox("[ERROR]");
	menu.set_color(1, RED_COLOR);
	menu.push_back_checkbox("[SYSTEM]");
	menu.set_color(2, LIGHT_MAGENTA_COLOR);
	menu.push_back_checkbox("[SECURITY]");
	menu.set_color(3, RED_COLOR);
	menu.push_back_checkbox("[AUTH]");
	menu.set_color(4, LIGHT_BLUE_COLOR);
	menu.push_back_checkbox("[ACTION]");
	menu.set_color(5, CYAN_COLOR);
	menu.push_back_checkbox("[JUDGE]");
	menu.set_color(6, LIGHT_GREEN_COLOR);
	menu.push_back_checkbox("[NETWORK]");
	menu.set_color(7, LIGHT_CYAN_COLOR);

	menu.push_back_text("Выбор временных рамок, формат: HH:MM:SS dd.mm.yy");

	menu.push_back_advanced_cin("От: ");
	menu.set_advanced_cin_max_input_length(9, 17);
	menu.set_advanced_cin_new_allowed_chars(9, "1234567890.: ");
	menu.set_notification(9, "(если пусто => без рамок)");

	menu.push_back_advanced_cin("До: ");
	menu.set_advanced_cin_max_input_length(10, 17);
	menu.set_advanced_cin_new_allowed_chars(10, "1234567890.: ");
	menu.set_notification(10, "(если пусто => без рамок)");

	menu.push_back_text("Выбор максимальной длины выборки");
	menu.push_back_advanced_cin("макс. размер:", "5000");
	menu.set_advanced_cin_max_input_length(12, 4);
	menu.set_advanced_cin_new_allowed_chars(12, "1234567890");
	menu.set_notification(12, "[1...5000]");

	menu.push_back_butt("Поиск");
	menu.push_back_butt("Назад");
	
	menu.set_notification_color(13, RED_COLOR);

	// дальше временные переменные
	std::vector<bool> tmp_vect;
	std::vector<char> data;
	uint32_t tmp_uint32_t{ 0 };
	std::string tmp_str;
	bool tmp_bool;

	uint32_t count_of_logs{ 0 };
	time_t from;
	tm from_tm;
	time_t to;
	tm to_tm;
	EasyLogs tmp_logs;
	EasyMenu tmp_menu(" ");

	while (true) {
		switch (menu.easy_run())
		{
		case 10:	// поиск
			tmp_uint32_t = 0;
			tmp_vect = menu.get_all_checkbox_status();
			for (uint32_t i{ 0 }; i < tmp_vect.size(); i++)
				tmp_uint32_t += tmp_vect[i];

			if (tmp_uint32_t == 0 || tmp_uint32_t > 1 && tmp_uint32_t != tmp_vect.size()) {
				menu.set_notification(13, "(Выберите 1 или все типы!)");
				break;
			}	

			if (menu.is_all_advanced_cin_correct() == false) {
				menu.set_notification(13, "(ошибка ввода)");
				break;
			}

			if (menu.get_advanced_cin_input(9).length() > 0 && menu.get_advanced_cin_input(9).length() < 17) {
				menu.set_notification(13, "(ошибка длины ввода \"От:\"!)");
				break;
			}

			tmp_str = menu.get_advanced_cin_input(9);
			if (tmp_str.length() > 0) {
				if (tmp_str[2] != ':' || tmp_str[5] != ':' || tmp_str[8] != ' ' || tmp_str[11] != '.' || tmp_str[14] != '.') {
					menu.set_notification(13, "(ошибка формата в \"От:\"!)");
					break;
				}
					
				tmp_bool = true;

				for (uint32_t i{ 0 }; i <= 5; i += 3) {
					tmp_uint32_t = int(tmp_str[i] - '0') * 10;
					tmp_uint32_t += int(tmp_str[i + 1] - '0');

					if (tmp_uint32_t > 59) {
						tmp_bool = false;
						break;
					}
				}

				if (tmp_bool == false) {
					menu.set_notification(13, "(логическая ошибка в \"От:\"!)");
					break;
				}

				// месяц
				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');
				
				if (tmp_uint32_t > 12) {
					menu.set_notification(13, "(логическая ошибка в \"От:\"!)");
					break;
				}

				// день

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');

				if (tmp_uint32_t > 31) {
					menu.set_notification(13, "(логическая ошибка в \"От:\"!)");
					break;
				}
			}

			tmp_str = menu.get_advanced_cin_input(10);
			if (tmp_str.length() > 0) {
				if (tmp_str[2] != ':' || tmp_str[5] != ':' || tmp_str[8] != ' ' || tmp_str[11] != '.' || tmp_str[14] != '.') {
					menu.set_notification(13, "(ошибка формата в \"От:\"!)");
					break;
				}

				tmp_bool = true;

				for (uint32_t i{ 0 }; i <= 5; i += 3) {
					tmp_uint32_t = int(tmp_str[i] - '0') * 10;
					tmp_uint32_t += int(tmp_str[i + 1] - '0');

					if (tmp_uint32_t > 59) {
						tmp_bool = false;
						break;
					}
				}

				if (tmp_bool == false) {
					menu.set_notification(13, "(логическая ошибка в \"До:\"!)");
					break;
				}

				// месяц
				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');

				if (tmp_uint32_t > 12) {
					menu.set_notification(13, "(логическая ошибка в \"До:\"!)");
					break;
				}

				// день

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');

				if (tmp_uint32_t > 31) {
					menu.set_notification(13, "(логическая ошибка в \"До:\"!)");
					break;
				}
			}

			tmp_str = menu.get_advanced_cin_input(12);
			if (tmp_str.length() == 0) {
				menu.set_notification(13, "(заполните длину выборки!)");
				break;
			}
			else if (std::stoi(tmp_str) > 5000 || std::stoi(tmp_str) == 0) {
				menu.set_notification(13, "(логическая ошибка в длине выборки!)");
				break;
			}

			// все хорошо, если дошли сюда
			menu.set_notification(13, "");	// сброс ошибки

			count_of_logs = std::stoi(tmp_str);

			if (menu.get_advanced_cin_input(9).length() > 0 && menu.get_advanced_cin_input(10).length() > 0) {
				// оба
				from = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				localtime_s(&from_tm, &from);

				tmp_str = menu.get_advanced_cin_input(9);

				tmp_uint32_t = int(tmp_str[0] - '0') * 10;
				tmp_uint32_t += int(tmp_str[1] - '0');
				from_tm.tm_hour = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[3] - '0') * 10;
				tmp_uint32_t += int(tmp_str[4] - '0');
				from_tm.tm_min = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[6] - '0') * 10;
				tmp_uint32_t += int(tmp_str[7] - '0');
				from_tm.tm_sec = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');
				from_tm.tm_mday = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');
				from_tm.tm_mon = tmp_uint32_t - 1;

				tmp_uint32_t = int(tmp_str[15] - '0') * 10;
				tmp_uint32_t += int(tmp_str[16] - '0');
				from_tm.tm_year = tmp_uint32_t + 100;

				from = mktime(&from_tm);

				to = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				localtime_s(&to_tm, &to);

				tmp_str = menu.get_advanced_cin_input(10);

				tmp_uint32_t = int(tmp_str[0] - '0') * 10;
				tmp_uint32_t += int(tmp_str[1] - '0');
				to_tm.tm_hour = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[3] - '0') * 10;
				tmp_uint32_t += int(tmp_str[4] - '0');
				to_tm.tm_min = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[6] - '0') * 10;
				tmp_uint32_t += int(tmp_str[7] - '0');
				to_tm.tm_sec = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');
				to_tm.tm_mday = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');
				to_tm.tm_mon = tmp_uint32_t - 1;

				tmp_uint32_t = int(tmp_str[15] - '0') * 10;
				tmp_uint32_t += int(tmp_str[16] - '0');
				to_tm.tm_year = tmp_uint32_t + 100;

				to = mktime(&to_tm);

				tmp_uint32_t = 0;
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
					tmp_uint32_t += menu.get_all_checkbox_status()[i];
					if (tmp_uint32_t > 1)
						break;
				}
				if (tmp_uint32_t > 1) {
					logs.select_from_to(from, to, data);
				}
				else {
					for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
						if (menu.get_all_checkbox_status()[i] == true) {
							logs.select_from_to(i + 1, from, to, data);
							break;
						}
					}
				}
			}
			else if (menu.get_advanced_cin_input(9).length() > 0) {
				// только от
				from = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				localtime_s(&from_tm, &from);

				tmp_str = menu.get_advanced_cin_input(9);

				tmp_uint32_t = int(tmp_str[0] - '0') * 10;
				tmp_uint32_t += int(tmp_str[1] - '0');
				from_tm.tm_hour = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[3] - '0') * 10;
				tmp_uint32_t += int(tmp_str[4] - '0');
				from_tm.tm_min = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[6] - '0') * 10;
				tmp_uint32_t += int(tmp_str[7] - '0');
				from_tm.tm_sec = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');
				from_tm.tm_mday = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');
				from_tm.tm_mon = tmp_uint32_t - 1;

				tmp_uint32_t = int(tmp_str[15] - '0') * 10;
				tmp_uint32_t += int(tmp_str[16] - '0');
				from_tm.tm_year = tmp_uint32_t + 100;

				from = mktime(&from_tm);

				tmp_uint32_t = 0;
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
					tmp_uint32_t += menu.get_all_checkbox_status()[i];
					if (tmp_uint32_t > 1)
						break;
				}
				if (tmp_uint32_t > 1) {
					logs.select_from(from, data);
				}
				else {
					for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
						if (menu.get_all_checkbox_status()[i] == true) {
							logs.select_from(i + 1, from, data);
							break;
						}
					}
				}
			}
			else if (menu.get_advanced_cin_input(10).length() > 0) {
				// только до
				to = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				localtime_s(&to_tm, &to);

				tmp_str = menu.get_advanced_cin_input(10);

				tmp_uint32_t = int(tmp_str[0] - '0') * 10;
				tmp_uint32_t += int(tmp_str[1] - '0');
				to_tm.tm_hour = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[3] - '0') * 10;
				tmp_uint32_t += int(tmp_str[4] - '0');
				to_tm.tm_min = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[6] - '0') * 10;
				tmp_uint32_t += int(tmp_str[7] - '0');
				to_tm.tm_sec = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[9] - '0') * 10;
				tmp_uint32_t += int(tmp_str[10] - '0');
				to_tm.tm_mday = tmp_uint32_t;

				tmp_uint32_t = int(tmp_str[12] - '0') * 10;
				tmp_uint32_t += int(tmp_str[13] - '0');
				to_tm.tm_mon = tmp_uint32_t - 1;

				tmp_uint32_t = int(tmp_str[15] - '0') * 10;
				tmp_uint32_t += int(tmp_str[16] - '0');
				to_tm.tm_year = tmp_uint32_t + 100;

				to = mktime(&to_tm);

				tmp_uint32_t = 0;
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
					tmp_uint32_t += menu.get_all_checkbox_status()[i];
					if (tmp_uint32_t > 1)
						break;
				}
				if (tmp_uint32_t > 1) {
					logs.select_to(to, data);
				}
				else {
					for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
						if (menu.get_all_checkbox_status()[i] == true) {
							logs.select_to(i + 1, to, data);
							break;
						}
					}
				}
			}
			else {
				// любое
				tmp_uint32_t = 0;
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
					tmp_uint32_t += menu.get_all_checkbox_status()[i];
					if (tmp_uint32_t > 1)
						break;
				}
				if (tmp_uint32_t > 1) {
					logs.select_all(data);
				}
				else {
					for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++) {
						if (menu.get_all_checkbox_status()[i] == true) {
							logs.select_all(unsigned char(i + 1), data);
							break;
						}
					}
				}
			}

			tmp_logs.open_via_char(data);
			tmp_logs.print_all(count_of_logs);
			tmp_logs.close();

			data.clear();

			while (true) {
				tmp_menu.advanced_tick();
				if (tmp_menu.advanced_is_pressed()) {
					tmp_menu.advanced_pressed_butt();
					break;
				}
				else
					Sleep(50);
			}

			tmp_menu.advanced_clear_console();

			break;
		case 11:// назад
			return;
			break;
		}
	}
}

//---------------------------------------------------------- методы классов
//---------------------- ServerData
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

bool ServerData::del_connection(const SOCKET& socket) {
	std::lock_guard<std::mutex> lock(connected_vect_mutex);
	for (uint32_t i{ 0 }; i < connected_vect.size(); i++) {
		if (connected_vect[i]->connection == socket) {
			delete connected_vect[i];	// очистил динамическую память
			connected_vect.erase(connected_vect.begin() + i);	// удалил указатель

			return true;
		}
	}

	return false;
}

//---------------------- MsgHead

MsgHead::MsgHead() {
	first_code = 0;
	second_code = 0;
	third_code = 0;
	msg_length = 0;
}

int MsgHead::size() {
	return 10;
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