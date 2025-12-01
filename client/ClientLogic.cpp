#include "ClientLogic.h"

// первый char код
#define FROM_SERVER 100	
#define FROM_CLIENT 55
// второй char код
#define ACCESS_DENIED 66
#define AUTHORISATION 2
#define CREATE_NEW_TASK 22
#define GET_ALL_TASKS 33
#define GET_TASK_INFO 101
#define DELETE_TASK 44
#define GET_IO_FILE 102
#define CHANGE_TASK 103
#define CHANGE_PASSWORD 5

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
		switch (client_data.screen_info_.type)
		{
		case 1:	// начальное меню
			switch (pressed_but)
			{
			case 0:	// открытие списка заданий
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(0, "(В обработке...)");
					client_data.menu_.set_notification_color(0, YELLOW_COLOR);
					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> data;
					CreateGetAllTasksMessage(data);
					SendTo(client_data, data);
				}
				break;
			case 1:	// настройки
				SettingsMenu(client_data);
				break;
			case 2:	// выход (logout)
				{
					std::lock_guard<std::mutex> lock(client_data.state_mutex);
					client_data.state_ = -1;
				}

				break;
			}
			break;
		case 2:	// все задания (список)
			{
				bool is_exit{ false };
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					
					if (pressed_but == client_data.menu_.get_count_of_buttons() - 1)
						is_exit = true;
				}

				if (is_exit) {	// нажата кнопка выхода
					StudentMenu(client_data, "");
				}
				else {	// обрабатываем какую-то кнопку

				}
			}
			break;
		case 100:	// настройки
			switch (pressed_but)
			{
			case 0:	// изменение пароля
				if (ChangePassword(client_data)) {
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);

					client_data.menu_.set_notification(0, "(В обработке...)");
					client_data.menu_.set_notification_color(0, YELLOW_COLOR);
					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				else {
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);

					client_data.menu_.delete_all_notifications();

					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				break;
			case 1:	// выход
				StudentMenu(client_data, "");
				break;
			}
			break;
		}
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
					client_data.menu_.set_notification_color(0, YELLOW_COLOR);
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
					client_data.menu_.set_notification_color(1, YELLOW_COLOR);
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
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(pressed_but, "(В обработке...)");
					client_data.menu_.set_notification_color(pressed_but, YELLOW_COLOR);
					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				
				std::vector<char> tmp_data;
				CreateGetTaskInfoMessage(tmp_data, pressed_but);
				SendTo(client_data, tmp_data);
			}
			break;
		case 3:	// меню выбранного задания 
			switch (pressed_but)
			{
			case 0:	// просмотр input файла
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(5, "(В обработке...)");
					client_data.menu_.set_notification_color(5, YELLOW_COLOR);

					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> tmp_data;
					uint32_t tmp_butt_index;
					{
						std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
						tmp_butt_index = client_data.screen_info_.id;
					}

					CreateGetIOFileShowMessage(tmp_data, tmp_butt_index, true);
					
					SendTo(client_data, tmp_data);
				}
				break;
			case 1:	// просмотр output файла
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(6, "(В обработке...)");
					client_data.menu_.set_notification_color(6, YELLOW_COLOR);

					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> tmp_data;
					uint32_t tmp_butt_index;
					{
						std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
						tmp_butt_index = client_data.screen_info_.id;
					}

					CreateGetIOFileShowMessage(tmp_data, tmp_butt_index, false);

					SendTo(client_data, tmp_data);
				}
				break;
			case 2:	// просмотр заданий

				break;
			case 3:	// редактирование
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(8, "(В обработке...)");
					client_data.menu_.set_notification_color(8, YELLOW_COLOR);

					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> tmp_data;
					uint32_t butt_index;
					{
						std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
						butt_index = client_data.screen_info_.id;
					}

					CreateChangeTaskMessage(tmp_data, butt_index);

					SendTo(client_data, tmp_data);
				}
				break;
			case 4:	// удалить
				if (TeacherDeleteConfirmMenu()) { // удалить
					{
						std::lock_guard<std::mutex> lock(client_data.menu_mutex);
						client_data.menu_.set_notification(9, "(В обработке...)");
						client_data.menu_.set_notification_color(9, YELLOW_COLOR);
					}
					std::vector<char> tmp_data;
					
					uint32_t tmp_butt_index;
					{
						std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
						tmp_butt_index = client_data.screen_info_.id;
					}

					CreateDeleteTaskMessage(tmp_data, tmp_butt_index);
					
					SendTo(client_data, tmp_data);
				}
				else {
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);

					client_data.menu_.advanced_display_menu();
				}
				break;
			case 5:	// назад
				{
					std::lock_guard<std::mutex> lock(client_data.menu_mutex);
					client_data.menu_.set_notification(10, "(В обработке...)");
					client_data.menu_.set_notification_color(10, YELLOW_COLOR);
					client_data.menu_.advanced_clear_console();
					client_data.menu_.advanced_display_menu();
				}
				{
					std::vector<char> tmp_data;
					CreateGetAllTasksMessage(tmp_data);
					SendTo(client_data, tmp_data);
				}
				break;
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
			return AuthorisationAs(msg_header, recv_buffer, client_data, STUDENT_ROLE);
			break;
		case TEACHER_ROLE:
			return AuthorisationAs(msg_header, recv_buffer, client_data, TEACHER_ROLE);
			break;
		case ADMIN_ROLE:
			return AuthorisationAs(msg_header, recv_buffer, client_data, ADMIN_ROLE);
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
		case STUDENT_ROLE:	// для студента
			return GetAllTasksForStudent(msg_header, recv_buffer, client_data);
			break;
		case TEACHER_ROLE:	// для препода
			return GetAllTasksForTeacher(msg_header, recv_buffer, client_data);
			break;
		default:
			return false;
			break;
		}
		break;
	case GET_TASK_INFO:
		switch (msg_header.third_code)
		{
		case 1: // для студентов

			break;
		case 2:	// для преподов
			return GetTaskInfoForTeacher(msg_header, recv_buffer, client_data);
			break;
		default:
			return false;
			break;
		}
		break;
	case GET_IO_FILE:
		switch (msg_header.third_code)
		{
		case 3:	// ответ для input
			return GetInputFile(msg_header, recv_buffer, client_data);
			break;
		case 4:	// ответ для output
			return GetOutputFile(msg_header, recv_buffer, client_data);
			break;
		default:
			return false;	// неизветная команда
			break;
		}
		break;
	case CHANGE_TASK:
		switch (msg_header.third_code)
		{
		case 2:	// просто получение меню изменения тасков
			return GetChangeTaskMenu(msg_header, recv_buffer, client_data);
			break;
		default:
			return false;
			break;
		}
		break;
	case CHANGE_PASSWORD:
		switch (msg_header.third_code)
		{
		case 1:	// подтвеждение изменения пароля
			return AccessChangePassword(msg_header, recv_buffer, client_data);
			break;
		case 2:	// отказ изменения пароля
			return FailChangePassword(msg_header, recv_buffer, client_data);
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

bool AuthorisationAs(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data, const uint32_t role_id) {
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

	switch (role_id)
	{
	case STUDENT_ROLE:
		StudentMenu(client_data, str_buffer);
		break;
	case TEACHER_ROLE:
		TeacherMenu(client_data, str_buffer);
		break;
	case ADMIN_ROLE:
		//AdminMenu(client_data, str_buffer);
		break;
	default:
		return false;
		break;
	}

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

bool GetAllTasksForStudent(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	std::vector<std::string> names_vect;
	std::vector<bool> status_vect;

	uint32_t uint32_t_buffer;
	bool bool_buffer;

	// само чтение
	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		names_vect.resize(uint32_t_buffer);
		status_vect.resize(uint32_t_buffer);

		for (uint32_t i{ 0 }; i < names_vect.size(); i++) {
			uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
			index += sizeof(uint32_t);

			names_vect[i].insert(names_vect[i].end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
			index += uint32_t_buffer;

			status_vect[i] = recv_buffer[index];
			index += sizeof(bool);
		}
	}
	catch (...) {
		return false;
	}

	StudentAlltasks(client_data, names_vect, status_vect);

	return true;
}

bool GetTaskInfoForTeacher(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	uint32_t uint32_t_buffer;

	std::string tmp_name, tmp_info;
	uint32_t tmp_count_of_complited, tmp_time_limit_ms, tmp_memory_limit_kb, tmp_butt_index;

	uint32_t index = msg_header.size_of();
	try {
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_name.insert(tmp_name.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;
		
		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_info.insert(tmp_info.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		tmp_count_of_complited = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_time_limit_ms = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_memory_limit_kb = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		tmp_butt_index = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		return false;
	}

	// значит все удачно прочитали 

	TeacherTaskInfo(client_data, tmp_name, tmp_info, tmp_count_of_complited, tmp_time_limit_ms, tmp_memory_limit_kb, tmp_butt_index);

	return true;
}

bool GetInputFile(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	uint32_t tmp_screen_type;
	uint32_t tmp_screen_role;

	{
		std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
		tmp_screen_type = client_data.screen_info_.type;
		tmp_screen_role = client_data.screen_info_.role;
	}

	if (tmp_screen_role == TEACHER_ROLE && tmp_screen_type == 3) {	// находимся внутри нужного меню
		uint32_t uint32_t_buffer;
		std::string tmp_str;

		uint32_t index = msg_header.size_of();
		try {	// пытаемся прочиать 
			uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
			index += sizeof(uint32_t);

			tmp_str.insert(tmp_str.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		}
		catch (...) {
			return false;
		}

		// прочитали => дальше заносим во временный файл
		std::ofstream file(GetAppDirectory() + "\\show_io.txt", std::ios::trunc);
		
		if (file.is_open() == false)
			return false;

		file << tmp_str;	// записали все в файл

		file.close();

		//дальше очищаем "(В обработке...)" и открываем сам файл
		{
			std::lock_guard<std::mutex> lock(client_data.menu_mutex);
			client_data.menu_.set_notification(5, "");
			client_data.menu_.advanced_clear_console();
			client_data.menu_.advanced_display_menu();
		}

		std::string tmp_path = GetAppDirectory() + "\\show_io.txt";

		OpenFileForUser(tmp_path);
	}

	return true;
}

bool GetOutputFile(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	uint32_t tmp_screen_type;
	uint32_t tmp_screen_role;

	{
		std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);
		tmp_screen_type = client_data.screen_info_.type;
		tmp_screen_role = client_data.screen_info_.role;
	}

	if (tmp_screen_role == TEACHER_ROLE && tmp_screen_type == 3) {	// находимся внутри нужного меню
		uint32_t uint32_t_buffer;
		std::string tmp_str;

		uint32_t index = msg_header.size_of();
		try {	// пытаемся прочиать 
			uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
			index += sizeof(uint32_t);

			tmp_str.insert(tmp_str.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		}
		catch (...) {
			return false;
		}

		// прочитали => дальше заносим во временный файл
		std::ofstream file(GetAppDirectory() + "\\show_io.txt", std::ios::trunc);

		if (file.is_open() == false)
			return false;

		file << tmp_str;	// записали все в файл

		file.close();

		//дальше очищаем "(В обработке...)" и открываем сам файл
		{
			std::lock_guard<std::mutex> lock(client_data.menu_mutex);
			client_data.menu_.set_notification(6, "");
			client_data.menu_.advanced_clear_console();
			client_data.menu_.advanced_display_menu();
		}

		std::string tmp_path = GetAppDirectory() + "\\show_io.txt";

		OpenFileForUser(tmp_path);
	}

	return true;
}

bool GetChangeTaskMenu(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	// временные переменные
	uint32_t uint32_t_buffer;

	uint32_t butt_index, time_limit_ms, memory_limit_kb;
	std::string name, info, input_file, output_file;
	
	uint32_t index = msg_header.size_of();
	try {
		butt_index = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		name.insert(name.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		info.insert(info.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		input_file.insert(input_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		uint32_t_buffer = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		output_file.insert(output_file.end(), &recv_buffer[index], &recv_buffer[index] + uint32_t_buffer);
		index += uint32_t_buffer;

		time_limit_ms = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);

		memory_limit_kb = *reinterpret_cast<const uint32_t*>(&recv_buffer[index]);
		index += sizeof(uint32_t);
	}
	catch (...) {
		return false;
	}

	bool tmp_is_del_tryes;
	if (TeacherChangeTaskMenu(client_data, butt_index, name, info, input_file, output_file, time_limit_ms, memory_limit_kb, tmp_is_del_tryes)) {	// была нажата "подтвердить изменения"
		std::vector<char> tmp_data;

		CreateChangeTaskMessage(tmp_data, butt_index, name, info, input_file, output_file, time_limit_ms, memory_limit_kb, tmp_is_del_tryes);

		if (SendTo(client_data, tmp_data) == false) {
			std::lock_guard<std::mutex> lock(client_data.menu_mutex);
			client_data.menu_.set_notification(8, "(Ошибка отправки запроса)");
			client_data.menu_.set_notification_color(8, RED_COLOR);

			client_data.menu_.advanced_clear_console();
			client_data.menu_.advanced_display_menu();
		}
		else {
			std::lock_guard<std::mutex> lock(client_data.menu_mutex);
			client_data.menu_.set_notification(8, "(Успешно изменено)");
			client_data.menu_.set_notification_color(8, GREEN_COLOR);

			client_data.menu_.advanced_clear_console();
			client_data.menu_.advanced_display_menu();
		}
	}
	else {	// была нажата "Выход"
		std::lock_guard<std::mutex> lock(client_data.menu_mutex);
		client_data.menu_.set_notification(8, "");

		client_data.menu_.advanced_clear_console();
		client_data.menu_.advanced_display_menu();
	}
	
	return true;
}

bool AccessChangePassword(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	if (client_data.screen_info_.type != 100)
		return true;	// все равно возвращаем true, просто не используем (меню закрыто)

	std::lock_guard<std::mutex> lock(client_data.menu_mutex);
	
	client_data.menu_.set_notification(0, "Пароль изменен");
	client_data.menu_.set_notification_color(0, LIGHT_GREEN_COLOR);

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();
}

bool FailChangePassword(const MsgHead& msg_header, const std::vector<char>& recv_buffer, Client_data& client_data) {
	if (client_data.screen_info_.type != 100)
		return true;	// все равно возвращаем true, просто не используем (меню закрыто)

	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.set_notification(0, "Пароль не изменен, неверный изнач. пароль!");
	client_data.menu_.set_notification_color(0, RED_COLOR);

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();
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

	tmp_menu.push_back_advanced_cin("Лимит по времени:");
	tmp_menu.set_advanced_cin_max_input_length(4, 4);
	tmp_menu.set_advanced_cin_new_allowed_chars(4, "1234567890");
	tmp_menu.set_notification(4, "(Миллисекунды)");

	tmp_menu.push_back_advanced_cin("Лимит по памяти:");
	tmp_menu.set_advanced_cin_max_input_length(5, 6);
	tmp_menu.set_advanced_cin_new_allowed_chars(5, "1234567890");
	tmp_menu.set_notification(5, "(Кбайты)");

	tmp_menu.push_back_butt("Создать");
	tmp_menu.set_notification_color(6, RED_COLOR);

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
		case 6:	// создать
			if (tmp_menu.is_all_advanced_cin_correct() == false) {
				tmp_menu.set_notification(6, "(Исправьте все ошибки ввода!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(0).length() < 3) {
				tmp_menu.set_notification(6, "(Минимальная длина названия - 3 символа!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(1).length() < 3) {
				tmp_menu.set_notification(6, "(Минимальная длина описания - 3 символа!)");
				break;
			}

			if (input_file.empty()) {
				tmp_menu.set_notification(6, "(Файл input должен быть выбран и не быть пустым!)");
				break;
			}

			if (output_file.empty()) {
				tmp_menu.set_notification(6, "(Файл output должен быть выбран и не быть пустым!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(4).empty()) {
				tmp_menu.set_notification(6, "(Заполните лимит по времени!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(4)[0] == '0') {
				tmp_menu.set_notification(6, "(Лимит по времени не может начинаться с \'0\'!)");
				break;
			}

			if (std::stoi(tmp_menu.get_advanced_cin_input(4)) < 10) {
				tmp_menu.set_notification(6, "(Минимальный лимит времени - 10мс!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(5).empty()) {
				tmp_menu.set_notification(6, "(Заполинте лимит по памяти!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(5)[0] == '0') {
				tmp_menu.set_notification(6, "(Лимит по памяти не может начинаться с \'0\'!)");
				break;
			}

			if (std::stoi(tmp_menu.get_advanced_cin_input(5)) < 1024) {
				tmp_menu.set_notification(6, "(Минильное ограничение по памяти 1Мб (1024 Кб)!)");
				break;
			}

			{	// сам запрос
				std::vector<char> tmp_data;
				std::string tmp_name = tmp_menu.get_advanced_cin_input(0);
				std::string tmp_info = tmp_menu.get_advanced_cin_input(1);
				uint32_t tmp_time_limit = std::stoi(tmp_menu.get_advanced_cin_input(4));
				uint32_t tmp_memory_limit = std::stoi(tmp_menu.get_advanced_cin_input(5));
				
				CreateNewTaskMessage(tmp_name, tmp_info, input_file, output_file, tmp_data, tmp_time_limit, tmp_memory_limit);

				if (SendTo(client_data, tmp_data) == false) {
					tmp_menu.set_notification(6, "(Ошибка отправки запроса!)");
					break;
				}
				return true;	// запрос успешно отправлен
			}
			break;
		case 7:	// выход
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

	client_data.menu_.push_back_butt("Назад");
	client_data.menu_.set_color(buttons.size(), BLUE_COLOR);

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 2;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = TEACHER_ROLE;
}

void TeacherTaskInfo(Client_data& client_data, const std::string& name, const std::string& info, const uint32_t& count_of_completes, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb, const uint32_t& butt_index) {
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	client_data.menu_.set_info("Просмотр задания");
	client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

	client_data.menu_.push_back_text("Название: " + name);

	client_data.menu_.push_back_text("Описание: " + info);

	client_data.menu_.push_back_text("Ограничение времени: " + std::to_string(time_limit_ms) + " мс");

	client_data.menu_.push_back_text("Ограничение памяти: " + std::to_string(memory_limit_kb) + " Кб");

	client_data.menu_.push_back_text("Студентов, сдавших работы: " + std::to_string(count_of_completes));

	client_data.menu_.push_back_butt("Просмотр input файла");
	client_data.menu_.set_color(5, LIGHT_CYAN_COLOR);

	client_data.menu_.push_back_butt("Просмотр output файла");
	client_data.menu_.set_color(6, LIGHT_CYAN_COLOR);

	client_data.menu_.push_back_butt("Просмотр решений");

	client_data.menu_.push_back_butt("Редактирование");

	client_data.menu_.push_back_butt("Удалить");

	client_data.menu_.push_back_butt("Назад");

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 3;
	client_data.screen_info_.id = butt_index;
	client_data.screen_info_.role = TEACHER_ROLE;
}

bool TeacherDeleteConfirmMenu() {
	EasyMenu menu("Назад", "Подтвердить удаление");
	menu.set_info("Подтвеждение удаления");
	menu.set_info_main_color(LIGHT_YELLOW_COLOR);

	menu.insert_text(0, "Удаление задания требует подтвеждения!");
	menu.set_color(0, RED_COLOR);

	if (menu.easy_run() == 1)
		return true;

	return false;
}

bool TeacherChangeTaskMenu(Client_data& client_data, uint32_t& butt_index, std::string& name, std::string& info, std::string& input_file, std::string& output_file, uint32_t& time_limit_ms, uint32_t& memory_limit_kb, bool& is_del_tryes) {
	EasyMenu tmp_menu;
	tmp_menu.set_info("Изменение задания: (" + name + ')');
	tmp_menu.set_info_main_color(LIGHT_YELLOW_COLOR);

	tmp_menu.push_back_advanced_cin("Название: ", name);
	tmp_menu.set_advanced_cin_new_allowed_chars(0, "йцукенгшщзхъфывапролджэячсмитьбю.ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮqwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890()[] ");
	tmp_menu.set_advanced_cin_max_input_length(0, 30);

	tmp_menu.push_back_advanced_cin("Краткое описание: ", info);
	tmp_menu.set_advanced_cin_new_allowed_chars(1, "йцукенгшщзхъфывапролджэячсмитьбю.ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮqwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890()[] ");
	tmp_menu.set_advanced_cin_max_input_length(1, 60);

	tmp_menu.push_back_butt("Вставить input файл");
	tmp_menu.set_notification(2, "(Изначальный файл) " + std::to_string(input_file.size()) + " байт");
	tmp_menu.set_color(2, CYAN_COLOR);

	tmp_menu.push_back_butt("Вставить output файл");
	tmp_menu.set_notification(3, "(Изначальный файл) " + std::to_string(output_file.size()) + " байт");
	tmp_menu.set_color(3, CYAN_COLOR);

	tmp_menu.push_back_advanced_cin("Лимит по времени:", std::to_string(time_limit_ms));
	tmp_menu.set_advanced_cin_max_input_length(4, 4);
	tmp_menu.set_advanced_cin_new_allowed_chars(4, "1234567890");
	tmp_menu.set_notification(4, "(Миллисекунды)");

	tmp_menu.push_back_advanced_cin("Лимит по памяти:", std::to_string(memory_limit_kb));
	tmp_menu.set_advanced_cin_max_input_length(5, 6);
	tmp_menu.set_advanced_cin_new_allowed_chars(5, "1234567890");
	tmp_menu.set_notification(5, "(Кбайты)");

	tmp_menu.push_back_checkbox("Очистить все решения", true);
	tmp_menu.set_notification(6, "Очистит все сданные работы");
	tmp_menu.set_notification_color(6, DARK_GRAY_COLOR);

	tmp_menu.push_back_butt("Подтвердить изменения");
	tmp_menu.set_notification_color(7, RED_COLOR);

	tmp_menu.push_back_butt("Назад");

	// дальше тело
	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

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
		case 7:	// подтвердить изменения
			if (tmp_menu.is_all_advanced_cin_correct() == false) {
				tmp_menu.set_notification(7, "(Исправьте все ошибки ввода!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(0).length() < 3) {
				tmp_menu.set_notification(7, "(Минимальная длина названия - 3 символа!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(1).length() < 3) {
				tmp_menu.set_notification(7, "(Минимальная длина описания - 3 символа!)");
				break;
			}

			if (input_file.empty()) {
				tmp_menu.set_notification(7, "(Файл input должен быть выбран и не быть пустым!)");
				break;
			}

			if (output_file.empty()) {
				tmp_menu.set_notification(7, "(Файл output должен быть выбран и не быть пустым!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(4).empty()) {
				tmp_menu.set_notification(7, "(Заполните лимит по времени!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(4)[0] == '0') {
				tmp_menu.set_notification(7, "(Лимит по времени не может начинаться с \'0\'!)");
				break;
			}

			if (std::stoi(tmp_menu.get_advanced_cin_input(4)) < 10) {
				tmp_menu.set_notification(7, "(Минимальный лимит времени - 10мс!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(5).empty()) {
				tmp_menu.set_notification(7, "(Заполинте лимит по памяти!)");
				break;
			}

			if (tmp_menu.get_advanced_cin_input(5)[0] == '0') {
				tmp_menu.set_notification(7, "(Лимит по памяти не может начинаться с \'0\'!)");
				break;
			}

			if (std::stoi(tmp_menu.get_advanced_cin_input(5)) < 1024) {
				tmp_menu.set_notification(7, "(Минильное ограничение по памяти 1Мб (1024 Кб)!)");
				break;
			}

			{	// собираем сами изменения
				name = tmp_menu.get_advanced_cin_input(0);
				info = tmp_menu.get_advanced_cin_input(1);
				time_limit_ms = std::stoi(tmp_menu.get_advanced_cin_input(4));
				memory_limit_kb = std::stoi(tmp_menu.get_advanced_cin_input(5));
				is_del_tryes = tmp_menu.get_all_checkbox_status()[0];

				return true;	// подтвердили изменения
			}
			break;
		case 8:	// выход
			return false;
			break;
		}
	}

	return true;
}

void StudentMenu(Client_data& client_data, std::string text) {
	{
		std::lock_guard<std::mutex> lock(client_data.menu_mutex);

		client_data.menu_.clear();

		if (text.empty() == false)
			client_data.menu_.set_info("Здравствуйте, " + text);
		else
			client_data.menu_.set_info("Меню студентаы");
		client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

		client_data.menu_.push_back_butt("Задания");

		client_data.menu_.push_back_butt("Настройки");

		client_data.menu_.push_back_butt("Выход из аккаунта");

		client_data.menu_.advanced_clear_console();
		client_data.menu_.advanced_display_menu();
	}

	std::lock_guard<std::mutex> lock(client_data.screen_info_mutex);

	client_data.screen_info_.type = 1;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = STUDENT_ROLE;
}

void StudentAlltasks(Client_data& client_data, const std::vector<std::string>& buttons, const std::vector<bool>& buttons_status) {
	uint32_t done_counter{ 0 };

	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	client_data.menu_.set_info("Все задания");
	client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

	for (uint32_t i{ 0 }; i < buttons.size(); i++) {
		client_data.menu_.push_back_butt(buttons[i]);

		if (buttons_status[i]) {
			done_counter++;
			client_data.menu_.set_notification(i, "(Выполнено)");
			client_data.menu_.set_notification_color(i, LIGHT_GREEN_COLOR);
		}
		else 
			client_data.menu_.set_notification(i, "(Не выполнено)");
	}

	client_data.menu_.push_back_butt("Назад");
	client_data.menu_.set_color(buttons.size(), BLUE_COLOR);

	client_data.menu_.insert_text(0, (done_counter == buttons.size()) ? "Все задания выполнены :D" : ("Осталось выполнить: " + std::to_string(buttons.size() - done_counter) + " заданий"));

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 2;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = STUDENT_ROLE;
}

void SettingsMenu(Client_data& client_data) {

	std::lock_guard<std::mutex> lock(client_data.menu_mutex);

	client_data.menu_.clear();

	client_data.menu_.set_info("Настройки");
	client_data.menu_.set_info_main_color(LIGHT_YELLOW_COLOR);

	client_data.menu_.push_back_butt("Изменение пароля");
	
	client_data.menu_.push_back_text("В разработке...");

	client_data.menu_.push_back_butt("Назад");
	client_data.menu_.set_color(2, BLUE_COLOR);

	client_data.menu_.advanced_clear_console();
	client_data.menu_.advanced_display_menu();

	std::lock_guard<std::mutex> lock2(client_data.screen_info_mutex);

	client_data.screen_info_.type = 100;
	client_data.screen_info_.id = 0;
	client_data.screen_info_.role = STUDENT_ROLE;
}

bool ChangePassword(Client_data& client_data) {
	
	EasyMenu tmp_menu;
	tmp_menu.set_info("Изменение пароля");
	tmp_menu.set_info_main_color(LIGHT_YELLOW_COLOR);

	tmp_menu.push_back_advanced_cin("Изнач. пароль: ");
	tmp_menu.set_advanced_cin_new_allowed_chars(0, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	tmp_menu.set_advanced_cin_secure_input_on(0);
	tmp_menu.set_advanced_cin_ban_not_allowed_on(0);

	tmp_menu.push_back_advanced_cin("Новый  пароль: ");
	tmp_menu.set_advanced_cin_new_allowed_chars(1, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	tmp_menu.set_advanced_cin_secure_input_on(1);
	tmp_menu.set_advanced_cin_ban_not_allowed_on(1);

	tmp_menu.push_back_advanced_cin("Повтор пароль: ");
	tmp_menu.set_advanced_cin_new_allowed_chars(2, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	tmp_menu.set_advanced_cin_secure_input_on(2);
	tmp_menu.set_advanced_cin_ban_not_allowed_on(2);

	tmp_menu.push_back_butt("Показать пароль");
	tmp_menu.set_color(3, LIGHT_CYAN_COLOR);

	tmp_menu.push_back_butt("Изменить");

	tmp_menu.push_back_butt("Назад");
	tmp_menu.set_color(5, BLUE_COLOR);

	// переменные
	bool is_secured{ true };

	while (true) {
		switch (tmp_menu.easy_run())
		{
		case 3:	// показать/скрыть пароль
			if (is_secured) {
				tmp_menu.set_advanced_cin_secure_input_off(0);
				tmp_menu.set_advanced_cin_secure_input_off(1);
				tmp_menu.set_advanced_cin_secure_input_off(2);

				tmp_menu.set_advanced_cin_ban_not_allowed_off(0);
				tmp_menu.set_advanced_cin_ban_not_allowed_off(1);
				tmp_menu.set_advanced_cin_ban_not_allowed_off(2);

				tmp_menu.edit(3, "Скрыть пароль");
			}
			else {
				tmp_menu.set_advanced_cin_secure_input_on(0);
				tmp_menu.set_advanced_cin_secure_input_on(1);
				tmp_menu.set_advanced_cin_secure_input_on(2);

				tmp_menu.set_advanced_cin_ban_not_allowed_on(0);
				tmp_menu.set_advanced_cin_ban_not_allowed_on(1);
				tmp_menu.set_advanced_cin_ban_not_allowed_on(2);

				tmp_menu.edit(3, "Показать пароль");
			}

			is_secured = !is_secured;
			break;
		case 4:	// изменить пароль
			if (tmp_menu.is_all_advanced_cin_correct() == false) {
				tmp_menu.set_notification(4, "(Исправьте ошибки в вводе!)");
				tmp_menu.set_notification_color(4, RED_COLOR);

				break;
			}
			if (tmp_menu.get_advanced_cin_input(0).empty()) {
				tmp_menu.set_notification(4, "(Заполните изначальный пароль)");
				tmp_menu.set_notification_color(4, RED_COLOR);

				break;
			}
			if (tmp_menu.get_advanced_cin_input(1).empty()) {
				tmp_menu.set_notification(4, "(Заполните новый пароль)");
				tmp_menu.set_notification_color(4, RED_COLOR);

				break;
			}
			if (tmp_menu.get_advanced_cin_input(2).empty()) {
				tmp_menu.set_notification(4, "(Заполните повтор пароль)");
				tmp_menu.set_notification_color(4, RED_COLOR);

				break;
			}
			if (tmp_menu.get_advanced_cin_input(1) != tmp_menu.get_advanced_cin_input(2)) {
				tmp_menu.set_notification(4, "(Новый и повтор пароли не совпадают!)");
				tmp_menu.set_notification_color(4, RED_COLOR);

				break;
			}

			// все гуд => отправлем запрос
			{
				std::string prev_psw = tmp_menu.get_advanced_cin_input(0), new_psw = tmp_menu.get_advanced_cin_input(1);

				std::vector<char> data;

				CreateChangePasswordMessage(data, prev_psw, new_psw);

				SendTo(client_data, data);
			}

			return true;
			break;
		case 5:	// назад (выход)
			return false;
			break;
		}
	}
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

void CreateNewTaskMessage(const std::string& name, const std::string& info, const std::string& input, const std::string& output, std::vector<char>& vect, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb) {
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

	uint32_t_buffer = time_limit_ms;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = memory_limit_kb;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

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

void CreateGetTaskInfoMessage(std::vector<char>& vect, const uint32_t& butt_index) {
	// временные перменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_TASK_INFO;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 0;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = sizeof(uint32_t);	// вес полезной инфы
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));
}

void CreateDeleteTaskMessage(std::vector<char>& vect, const uint32_t& butt_index) {
	// временные перменные
	std::vector<char> main_data;

	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// перевод main_data
	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// составление самого запроса
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = DELETE_TASK;
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

void CreateGetIOFileShowMessage(std::vector<char>& vect, const uint32_t& butt_index, bool is_input) {
	// временные переменные
	std::vector<char> main_data;

	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// заполенние main_data
	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// заполенение всего сообщения
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = GET_IO_FILE;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	if (is_input)
		uint32_t_buffer = 1;
	else
		uint32_t_buffer = 2;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateChangeTaskMessage(std::vector<char>& vect, const uint32_t& butt_index) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	// заполеняем полезные данные
	std::vector<char> main_data;

	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	// заполнение всего запроса
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_TASK;
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

void CreateChangeTaskMessage(std::vector<char>& vect, const uint32_t& butt_index, const std::string& name, const std::string& info, const std::string& input_file, const std::string& output_file, const uint32_t& time_limit_ms, const uint32_t& memory_limit_kb, const bool& is_del_tryes) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	bool bool_buffer;
	char* tmp_ptr;

	// заполняем main_data
	std::vector<char> main_data;

	uint32_t_buffer = butt_index;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = name.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), name.begin(), name.end());

	uint32_t_buffer = info.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), info.begin(), info.end());

	uint32_t_buffer = input_file.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), input_file.begin(), input_file.end());

	uint32_t_buffer = output_file.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), output_file.begin(), output_file.end());

	uint32_t_buffer = time_limit_ms;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = memory_limit_kb;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	bool_buffer = is_del_tryes;
	tmp_ptr = reinterpret_cast<char*>(&bool_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(bool));

	// составление всего запроса
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_TASK;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uint32_t_buffer = 103;
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	uint32_t_buffer = main_data.size();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	vect.insert(vect.end(), main_data.begin(), main_data.end());
}

void CreateChangePasswordMessage(std::vector<char>& vect, const std::string& prev_password, const std::string& new_password) {
	// временные переменные
	uint32_t uint32_t_buffer;
	unsigned char uchar_buffer;
	char* tmp_ptr;

	std::vector<char> main_data;

	// main_data
	uint32_t_buffer = prev_password.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), prev_password.begin(), prev_password.end());

	uint32_t_buffer = new_password.length();
	tmp_ptr = reinterpret_cast<char*>(&uint32_t_buffer);
	main_data.insert(main_data.end(), tmp_ptr, tmp_ptr + sizeof(uint32_t));

	main_data.insert(main_data.end(), new_password.begin(), new_password.end());

	// вся data
	vect.clear();

	uchar_buffer = FROM_CLIENT;
	tmp_ptr = reinterpret_cast<char*>(&uchar_buffer);
	vect.insert(vect.end(), tmp_ptr, tmp_ptr + sizeof(unsigned char));

	uchar_buffer = CHANGE_PASSWORD;
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

std::string GetAppDirectory() {
	char path[MAX_PATH];
	GetModuleFileNameA(nullptr, path, MAX_PATH);
	std::string dir(path);
	size_t pos = dir.find_last_of("\\/");
	return dir.substr(0, pos);
}

void OpenFileForUser(const std::string& file_path)
{
	// Открывает файл в стандартной программе Windows
	HINSTANCE result = ShellExecuteA(
		nullptr,           // окно-владелец (nullptr — любое)
		"open",            // операция
		file_path.c_str(), // путь к файлу
		nullptr,           // параметры
		nullptr,           // рабочая директория
		SW_SHOW            // показать окно
	);

	// Проверим результат
	if ((int)result <= 32)
		MessageBoxA(nullptr, "Не удалось открыть файл!", "Ошибка", MB_ICONERROR);
}