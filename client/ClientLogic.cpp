#include "ClientLogic.h"

// первый char код
#define FROM_SERVER 100	
#define FROM_CLIENT 55
// второй char код
#define ACCESS_DENIED 66
#define AUTHORISATION 2
#define CREATE_NEW_TASK 22
#define GET_ALL_TASKS 33

// методы классов

MsgHead::MsgHead() {
	first_code = UCHAR_MAX;
	second_code = UCHAR_MAX;
	third_code = UINT32_MAX;
	msg_length = 0;
}

int MsgHead::size_of() const {
	return 10;
}

bool MsgHead::read_from_char(const char* ptr) {
	uint32_t ptr_index = 0;
	try {
		first_code = *reinterpret_cast<const unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(first_code);

		second_code = *reinterpret_cast<const unsigned char*>(ptr + ptr_index);
		ptr_index += sizeof(second_code);

		third_code = *reinterpret_cast<const uint32_t*>(ptr + ptr_index);
		ptr_index += sizeof(third_code);

		msg_length = *reinterpret_cast<const uint32_t*>(ptr + ptr_index);
	}
	catch (...) {	// какая-то UB
		first_code = UCHAR_MAX;
		second_code = UCHAR_MAX;
		third_code = UINT32_MAX;
		msg_length = 0;

		return false;	// перевод неудачный
	}

	return true;	// удачный перевод
}

Client_data::Client_data(){
	state_ = 0;
	is_connected_ = false;
}

// функции

void ClientMenuLogic(Client_data& client_data) {
	int32_t last_pressed{ 0 };
	bool is_pressed{ false };

	AuthorisationMenu(client_data, "");

	while (true) {	// тело цикла
		int32_t tmp_type;
		{
			std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
			tmp_type = client_data.screen_info_.role;
		}

		if (tmp_type != NO_ROLE) {
			{
				std::lock_guard<std::mutex> lock(client_data.menu_mutex);

				client_data.menu_.advanced_tick();
				if (client_data.menu_.advanced_is_pressed()) {
					last_pressed = client_data.menu_.advanced_pressed_butt();
					is_pressed = true;
				}
			}

			if (is_pressed) {
				is_pressed = false;
				ClientClickLogic(last_pressed, client_data);
			}
			else
				Sleep(10);
		}
		else {	// значит меню авторизации
			if (AuthorisationMenuLogic(client_data) == false)
				break;

			auto start_time = std::chrono::steady_clock::now();

			while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
				bool tmp_bool;
				{
					std::lock_guard<std::mutex> lock1(client_data.screen_info_mutex);
					std::lock_guard<std::mutex> lock2(client_data.state_mutex);
					
					tmp_bool = client_data.state_ == -1 || client_data.screen_info_.role != NO_ROLE;
				}

				if (tmp_bool)
					break;
				else
					Sleep(5);
			}
		}
	}

	// завершение работы
	{
		std::lock_guard<std::mutex> lock(client_data.menu_mutex);
		client_data.menu_.advanced_clear_console();
		
		std::cout << "Завершение работы";
	}
	bool tmp_bool;
	{
		std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
		tmp_bool = client_data.is_connected_;
	}

	if (tmp_bool) {
		{
			std::lock_guard<std::mutex> lock(client_data.state_mutex);
			client_data.state_ = -1;
		}

		if (client_data.connect_thread.joinable())
			client_data.connect_thread.join();
	}
}

bool AuthorisationMenuLogic(Client_data& client_data) {
	bool idk;
	bool eye_flag{ false };
	
	client_data.menu_.advanced_clear_console();

	while (true) {	// главное тело
		switch (client_data.menu_.easy_run())
		{
		case 2:	// глазок
			eye_flag = !eye_flag;
			{
				std::lock_guard<std::mutex> lock(client_data.menu_mutex);

				if (eye_flag) { // показать 
					client_data.menu_.set_advanced_cin_secure_input_off(1);
					client_data.menu_.edit(2, "Скрыть пароль");
				}
				else {	// скрыть
					client_data.menu_.set_advanced_cin_secure_input_on(1);
					client_data.menu_.edit(2, "Показать пароль");
				}
			}
			break;
		case 3:	// авторизация
			idk = true;
			{
				std::lock_guard<std::mutex> lock(client_data.menu_mutex);

				if (client_data.menu_.is_all_advanced_cin_correct() == false) {
					client_data.menu_.set_notification(3, "(Исправьте ошибки в вводе)");
					break;
				}

				if (client_data.menu_.get_advanced_cin_input(0).length() != 8) {
					client_data.menu_.set_notification(0, "(Длина должна быть равна 8-ми)");
					break;
				}

				if (client_data.menu_.get_advanced_cin_input(0)[0] == '0') {
					client_data.menu_.set_notification(0, "(Логин не может начинаться с \'0\')");
					break;
				}

				if (client_data.menu_.get_advanced_cin_input(1).length() < 4) {
					client_data.menu_.set_notification(1, "(Длина должна быть от 3-х)");
					break;
				}

				client_data.menu_.delete_all_notifications();
			}	// проходим дальше если не было ошибок

			if (ConnectClient(client_data)) {
				std::vector<char> data;
				std::string tmp_login, tmp_password;

				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);

					tmp_login = client_data.menu_.get_advanced_cin_input(0);
					tmp_password = client_data.menu_.get_advanced_cin_input(1);
				}

				CreateAuthorisationMessage(tmp_login, tmp_password, data);

				if (SendTo(client_data, data) == false) {
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);

					client_data.menu_.set_notification(3, "(Ошибка отправки запроса)");
					break;
				}

				return true;
			}
			else {
				std::lock_guard<std::mutex> lock(client_data.menu_mutex);
				client_data.menu_.set_notification(3, "(Не удалось подключиться)");
			}
			break;
		case 4:	// выход
			return false;
			break;
		}
	}
}

void ClientClickLogic(int32_t pressed_but, Client_data& client_data) {
	screen_data tmp_screen_data;
	{
		std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
		tmp_screen_data = client_data.screen_info_;
	}

	switch (tmp_screen_data.role)
	{
	case STUDENT_ROLE:

		break;
	case TEACHER_ROLE:
		switch (client_data.screen_info_.type) {
		case 1:	// начальное меню 
			switch (pressed_but)
			{
			case 0:	// урп заданиями 
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(0, "(В обработке...)");
					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> tmp_data;
					CreateGetAllTasksMessage(tmp_data);
					SendTo(client_data, tmp_data);
				}
				break;
			case 1:	// создать новое задание
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(1, "(В обработке...)");
				}
				if (TeacherCreateNewTask(client_data)) {
					// был отправлен запрос на создание 
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.advanced_display_menu();
				}
				else {
					// просто выход (без запроса на создание)
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(1, "");
					client_data.menu_.advanced_display_menu();
				}
				break;
			case 2:	// выход из аккаунта
				{
					std::lock_guard<std::mutex> lock(client_data.state_mutex);
					client_data.state_ = -1;
				}

				break;
			}
			break;
		case 2:	// выбор задания
			if (pressed_but == client_data.menu_.get_count_of_buttons() - 1) { // значит нажат выход
				TeacherMenu(client_data, "");
			}
			else {	// выбрано какое-то задание

			}
			break;
		}

		break;

	case ADMIN_ROLE:

		break;
	}
}

bool SetupClient(Client_data& client_data) {
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);

	if (WSAStartup(wVersionRequested, &wsaData) == WSASYSNOTREADY) {
		// ошибка запуска WSA
		return false;	// выход
	}	// WSA запущен

	if ((client_data.door_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		// Ошибка открытия сокета
		WSACleanup();

		return false;	// выход
	}	// Сокет открыт

	return true;	// успешный запуск
}

bool ConnectClient(Client_data& client_data) {
	{
		bool tmp_flag{ false }; 
		{
			std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
			tmp_flag = client_data.is_connected_;
		}
		if (tmp_flag == true) {
			client_data.state_mutex.lock();
			client_data.state_ = -1;	// закрытие соединения
			client_data.state_mutex.unlock();

			if (client_data.connect_thread.joinable())
				client_data.connect_thread.join();	// ждем завершение потока
		}
	}

	client_data.door_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client_data.door_sock == INVALID_SOCKET) {
		std::cout << "Ошибка создания сокета!" << std::endl;
		return false;
	}

	sockaddr_in server_adress;
	server_adress.sin_family = AF_INET;
	server_adress.sin_port = htons(60888); // задаем порт сервера
	if (SERVER_LOCAL_MODE)
		server_adress.sin_addr.s_addr = inet_addr("127.0.0.1"); // задает IP сервера
	else
		server_adress.sin_addr.s_addr = inet_addr("26.225.195.119"); // IP адресс RadminVPN

	u_long mode = 0; // 0 = блокирующий режим
	ioctlsocket(client_data.door_sock, FIONBIO, &mode);	// поставили сокет в блокирующий режим (чисто для удобного connect)

	if (connect(client_data.door_sock, (sockaddr*)&server_adress, sizeof(server_adress)) == SOCKET_ERROR) {
		// ошибка подключения
		std::cout << "Не удалось подключиться! ";
		
		closesocket(client_data.door_sock);
		client_data.door_sock = INVALID_SOCKET;

		return false;
	}

	mode = 1; // 1 = неблокирующий режим
	ioctlsocket(client_data.door_sock, FIONBIO, &mode);	// поставили сокет в неблокирующий режим (асинхронный)
	
	{
		std::lock_guard<std::mutex> lock(client_data.is_connected_mutex);
		client_data.is_connected_ = true;
	}
	{
		std::lock_guard<std::mutex> lock(client_data.state_mutex);
		client_data.state_ = 0;
	}

	client_data.connect_thread = std::thread(ClientThread, std::ref(client_data));

	return true;
}

void ClientThread(Client_data& client_data) {
	char packet_buffer[1024];
	int32_t packet_size{ 0 };

	std::vector<char> recv_buffer;

	MsgHead msg_header;

	bool is_optimizated{ false };

	while (true) {	// основное тело

		packet_size = recv(client_data.door_sock, packet_buffer, 1024, 0);	// пытаемся прочитать

		if (packet_size > 0) {	// прочитали какие-то данные

			recv_buffer.insert(recv_buffer.end(), packet_buffer, packet_buffer + packet_size);	// добавили в общий массив

			while (true) {	// пытаемся прочиать все возможные запросы
				if (recv_buffer.size() < msg_header.size_of())
					break;	// не хвататет даже на заголовок

				if (msg_header.read_from_char(&recv_buffer[0]) == false) {
					// ошибка, закрываем соединение
					{
						std::lock_guard<std::mutex> lock(client_data.state_mutex);
						client_data.state_ = -1;
					}

					closesocket(client_data.door_sock);
					client_data.door_sock = INVALID_SOCKET;

					AuthorisationMenu(client_data, "Ошибка приема данных");

					return;
				}

				if (recv_buffer.size() < msg_header.size_of() + msg_header.msg_length)
					break;	// не хавтате чтобы прочиать 

				if (ProcessMessage(msg_header, recv_buffer, client_data) == false) {
					// ошибка, закрываем соединение
					{
						std::lock_guard<std::mutex> lock(client_data.state_mutex);
						client_data.state_ = -1;
					}

					closesocket(client_data.door_sock);
					client_data.door_sock = INVALID_SOCKET;

					AuthorisationMenu(client_data, "");

					return;
				}

				// очищаем использованные данные
				recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + msg_header.size_of() + msg_header.msg_length);
			}
		}
		else if (packet_size == 0) {	// сервер закрыл соединение через closesocket()
			break;
		}
		else if (WSAGetLastError() == WSAEWOULDBLOCK) {	// надо просто подождать
			Sleep(20);
			is_optimizated = true;
		}
		else {	// просто ошибка SOCKET_ERROR
			// закрываем соединение
			break;
		}

		{
			std::lock_guard<std::mutex> lock1(client_data.is_connected_mutex);
			std::lock_guard<std::mutex> lock2(client_data.state_mutex);

			if (client_data.state_ == -1) {

				closesocket(client_data.door_sock);
				client_data.door_sock = INVALID_SOCKET;

				AuthorisationMenu(client_data, "");

				return;
			}
			else if (is_optimizated == false)
				Sleep(20);
		}
	}

	// закрытие соединения
	{
		std::lock_guard<std::mutex> lock(client_data.state_mutex);
		client_data.state_ = -1;
	}

	closesocket(client_data.door_sock);
	client_data.door_sock = INVALID_SOCKET;

	AuthorisationMenu(client_data, "Неверный логин/пароль");

	return;
}

bool ProcessMessage(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	if (msg_header.first_code != FROM_SERVER)
		return false;

	switch (msg_header.second_code)
	{
	case ACCESS_DENIED:
		return AccessDenied(msg_header, recv_buffer, client_data);
		break;
	case AUTHORISATION:
		switch (msg_header.third_code)
		{
		case STUDENT_ROLE:

			break;
		case TEACHER_ROLE:
			return AuthorisationAsTeacher(msg_header, recv_buffer, client_data);
			break;
		case ADMIN_ROLE:

			break;
		default:
			return false;
			break;
		}
		break;
	case CREATE_NEW_TASK:
		return ConfirmCreateTask(msg_header, recv_buffer, client_data);
		break;
	case GET_ALL_TASKS:
		switch (msg_header.third_code)
		{
		case 1:	// для студента

			break;
		case 2:	// для препода
			return GetAllTasksForTeacher(msg_header, recv_buffer, client_data);
			break;
		default:
			return false;
			break;
		}
		break;
	default:
		return false;	// неизвестная команда
		break;
	}

	return true;
}

bool AccessDenied(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// буферы
	unsigned char uchar_buffer{ 0 };
	uint32_t uint32_t_buffer{ 0 };
	std::string str_buffer;

	// указатель  
	uint32_t index = msg_header.size_of();

	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t_buffer);

		str_buffer.resize(uint32_t_buffer);

		str_buffer.insert(str_buffer.end(), &recv_buffer[index], &recv_buffer[index] + str_buffer.size());
	}
	catch (...) {	// какая-то ошибка
		return false;
	}

	AuthorisationMenu(client_data, str_buffer);

	{
		std::lock_guard<std::mutex> lock(client_data.state_mutex);
		client_data.state_ = -1;
	}

	return true;
}

bool AuthorisationAsTeacher(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// буферы
	unsigned char uchar_buffer{ 0 };
	uint32_t uint32_t_buffer{ 0 };
	std::string str_buffer;

	// указатель  
	uint32_t index = msg_header.size_of();

	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		str_buffer.insert(str_buffer.begin(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
	}
	catch (...) {
		return false;
	}

	TeacherMenu(client_data, str_buffer);

	return true;
}

bool ConfirmCreateTask(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);

	if (msg_header.third_code == 2 && client_data.screen_info_.role == TEACHER_ROLE && client_data.screen_info_.type == 1) {
		std::lock_guard<std::mutex> lock2(client_data.menu_mutex);

		client_data.menu_.set_notification(1, "Успешно создано!");
		client_data.menu_.set_notification_color(1, GREEN_COLOR);

		client_data.menu_.advanced_clear_console();
		client_data.menu_.advanced_display_menu();
	}
	
	return true;
}

bool GetAllTasksForTeacher(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	std::vector<std::string> tmp_data;

	uint32_t uint32_t_buffer;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_data.resize(uint32_t_buffer);

		for (uint32_t i{ 0 }; i < tmp_data.size(); i++) {
			uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
			index += sizeof(uint32_t);

			tmp_data[i].insert(tmp_data[i].end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
			index += uint32_t_buffer;
		}
	}
	catch (...) {
		return false;
	}

	TeacherAlltasks(client_data, tmp_data);

	return true;
}

void AuthorisationMenu(Client_data& client_data, std::string text) {
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	if (text.empty()) {
		client_data.menu_.set_info("Авторизация");
		client_data.menu_.set_info_main_color(DARK_GRAY_COLOR);
	}
	else {
		client_data.menu_.set_info(text);
		client_data.menu_.set_info_main_color(YELLOW_COLOR);
	}

	client_data.menu_.push_back_advanced_cin("Логин:");
	client_data.menu_.set_advanced_cin_new_allowed_chars(0, "1234567890");
	client_data.menu_.set_advanced_cin_max_input_length(0, 8);

	client_data.menu_.push_back_advanced_cin("Пароль:");
	client_data.menu_.set_advanced_cin_ban_not_allowed_on(1);
	client_data.menu_.set_advanced_cin_new_allowed_chars(1, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	client_data.menu_.set_advanced_cin_secure_input_on(1);

	client_data.menu_.push_back_butt("Показать пароль");
	client_data.menu_.set_color(2, MAGENTA_COLOR);

	client_data.menu_.push_back_butt("Войти");
	
	client_data.menu_.push_back_butt("Выход из программы");

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = AUTHORISATION_MENUTYPE;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = NO_ROLE;
}

void TeacherMenu(Client_data& client_data, std::string text) {
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	if (text.empty() == false)
		client_data.menu_.set_info("Здравствуйте, " + text);
	else
		client_data.menu_.set_info("Меню преподавателя");
	client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

	client_data.menu_.push_back_butt("Упр заданиями");

	client_data.menu_.push_back_butt("Добавить задание");

	client_data.menu_.push_back_butt("Выход из аккаунта");

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 1;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = TEACHER_ROLE;
}

bool TeacherCreateNewTask(Client_data& client_data){
	std::string input_file;
	std::string output_file;

	EasyMenu tmp_menu;
	tmp_menu.set_info("Создание нового задания");

	tmp_menu.push_back_advanced_cin("Название: ");
	tmp_menu.set_advanced_cin_new_allowed_chars(0, "йцукенгшщзхъфывапролджэячсмитьбю.ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮqwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890()[] ");
	tmp_menu.set_advanced_cin_max_input_length(0, 30);

	tmp_menu.push_back_advanced_cin("Краткое описание: ");
	tmp_menu.set_advanced_cin_new_allowed_chars(1, "йцукенгшщзхъфывапролджэячсмитьбю.ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮqwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890()[] ");
	tmp_menu.set_advanced_cin_max_input_length(1, 60);

	tmp_menu.push_back_butt("Вставить input файл");
	tmp_menu.set_notification(2, "(Файл не выбран!)");
	tmp_menu.set_color(2, CYAN_COLOR);

	tmp_menu.push_back_butt("Вставить output файл");
	tmp_menu.set_notification(3, "(Файл не выбран!)");
	tmp_menu.set_color(3, CYAN_COLOR);

	tmp_menu.push_back_butt("Создать");
	tmp_menu.set_notification_color(4, RED_COLOR);

	tmp_menu.push_back_butt("Назад");

	// дальше тело

	while (true) {
		switch (tmp_menu.easy_run())
		{
		case 2:	// выбрать input файл
			{
				std::string tmp_str = WstrToStr(OpenFileDialog());
				if (tmp_str.empty() == false) {
					std::ifstream file(tmp_str);
					
					if (file.is_open() == false) {
						tmp_menu.set_notification(2, "Не удалось открыть выбранный файл!");
						tmp_menu.set_notification_color(2, RED_COLOR);
					}
					else {
						std::ostringstream buffer;
						buffer << file.rdbuf(); // считывает весь поток

						input_file.clear();
						input_file = buffer.str();

						tmp_menu.set_notification(2, tmp_str + " -> " + std::to_string(input_file.length()) + " байтов");
						tmp_menu.set_notification_color(2, GREEN_COLOR);

						file.close();
					}
				}
			}
			break;
		case 3:	// выбрать output файл
			{
				std::string tmp_str = WstrToStr(OpenFileDialog());
				if (tmp_str.empty() == false) {
					std::ifstream file(tmp_str);

					if (file.is_open() == false) {
						tmp_menu.set_notification(3, "Не удалось открыть выбранный файл!");
						tmp_menu.set_notification_color(3, RED_COLOR);
					}
					else {
						std::ostringstream buffer;
						buffer << file.rdbuf(); // считывает весь поток

						output_file.clear();
						output_file = buffer.str();

						tmp_menu.set_notification(3, tmp_str + " -> " + std::to_string(output_file.length()) + " байтов");
						tmp_menu.set_notification_color(3, GREEN_COLOR);

						file.close();
					}
				}
			}
			break;
		case 4:	// создать
			if (tmp_menu.is_all_advanced_cin_correct() == false) {
				tmp_menu.set_notification(4, "(Исправьте все ошибки ввода!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(0).length() < 4) {
				tmp_menu.set_notification(4, "(Минимальная длина названия - 3 символа!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(1).length() < 4) {
				tmp_menu.set_notification(4, "(Минимальная длина описания - 3 символа!)");
				break;
			}

			if (input_file.empty()) {
				tmp_menu.set_notification(4, "(Файл input должен быть выбран и не быть пустым!)");
				break;
			}

			if (output_file.empty()) {
				tmp_menu.set_notification(4, "(Файл output должен быть выбран и не быть пустым!)");
				break;
			}

			{
				std::vector<char> tmp_data;
				std::string tmp_name = tmp_menu.get_advanced_cin_input(0);
				std::string tmp_info = tmp_menu.get_advanced_cin_input(1);
				
				CreateNewTaskMessage(tmp_name, tmp_info, input_file, output_file, tmp_data);

				if (SendTo(client_data, tmp_data) == false) {
					tmp_menu.set_notification(4, "(Ошибка отправки запроса!)");
					break;
				}
				return true;	// запрос успешно отправлен
			}
			break;
		case 5:	// выход
			return false;
			break;
		}
	}
}

void TeacherAlltasks(Client_data& client_data, std::vector<std::string> buttons) {
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	client_data.menu_.set_info("Выбор задания");
	client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

	for (uint32_t i{ 0 }; i < buttons.size(); i++)
		client_data.menu_.push_back_butt(buttons[i]);

	client_data.menu_.push_back_butt("Нахад");
	client_data.menu_.set_color(buttons.size(), BLUE_COLOR);

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 2;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = TEACHER_ROLE;
}

bool SendTo(Client_data& client_data, const std::vector<char>& data) {
	uint32_t sended_count{ 0 };

	int32_t tmp_count{ 0 };

	auto last_send_time{ std::chrono::steady_clock::now() };

	while (sended_count < data.size()) {	// выполняем пока количество реально отправленных меньше полной даты
		tmp_count = send(client_data.door_sock, &data[sended_count], data.size() - sended_count, 0);

		if (tmp_count > 0) {
			// значит что-то уже отправилось
			sended_count += tmp_count;	// добавили счетчик
			last_send_time = std::chrono::steady_clock::now();
		}
		else if (tmp_count == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				// просто ждем
				if (std::chrono::steady_clock::now() - last_send_time > std::chrono::seconds(5)) {
					return false;	// отправка не завершилась
				}
				Sleep(5);
			}
			else {
				// ошибка
				return false;	// отправка не завершилась
			}
		}
		else {	// видимо == 0
			// клиент закрыл соединение
			return false;	// отправка не завершилась
		}
	}

	return true;	// отправка успешно завершилась
}

void CreateGetAllTasksMessage(std::vector<char>& vect) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr{ nullptr };

	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_ALL_TASKS;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 0;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));
}

void CreateAuthorisationMessage(const std::string& login, const std::string& password, std::vector<char>& vect) {
	unsigned char uchar_buffer{ 0 };
	uint32_t uint32_t_buffer{ 0 };
	char* tmp_ptr{ nullptr };
	
	// с начала всю полезную инфу
	std::vector<char> main_data;

	uint32_t_buffer = login.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), login.begin(), login.end());

	uint32_t_buffer = password.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), password.begin(), password.end());
	
	// дальше само сообщение
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = AUTHORISATION;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 0;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateNewTaskMessage(const std::string& name, const std::string& info, const std::string& input, const std::string& output, std::vector<char>& vect) {
	// временные переменные 
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	std::vector<char> main_data;

	// запись основной инфы
	uint32_t_buffer = name.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), name.begin(), name.end());

	uint32_t_buffer = info.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), info.begin(), info.end());

	uint32_t_buffer = input.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), input.begin(), input.end());

	uint32_t_buffer = output.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), output.begin(), output.end());

	// дальше совмещение
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CREATE_NEW_TASK;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 0;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

//перифирия-------------------------------------------------------
std::string WstrToStr(const std::wstring& wstr)
{
	if (wstr.empty()) return "";
	int size_needed = WideCharToMultiByte(1251, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size_needed - 1, 0);
	WideCharToMultiByte(1251, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
	return str;
}

std::wstring OpenFileDialog()
{
	OPENFILENAMEW ofn;
	WCHAR szFile[260] = { 0 };

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = nullptr;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameW(&ofn))
		return std::wstring(ofn.lpstrFile);

	return L"";
}