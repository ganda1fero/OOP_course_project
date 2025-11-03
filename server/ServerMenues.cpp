#include "ServerMenues.h"

void ServerMenu(ServerData& server, EasyLogs& logs) {
	EasyMenu menu("Управление аккаунтами", "Просмотр логов", "Остановить сервер");
	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);
	menu.set_color(2, BLUE_COLOR);

	while (true) {
		switch (menu.easy_run())
		{
		case 0:	// упр. аккаунтами
			AccountsMenu(logs, server);
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

void AccountsMenu(EasyLogs& logs, ServerData& server) {
	EasyMenu menu("Поиск по ID", "Добавить аккаунт", "Назад");
	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);

	menu.set_notification(0, "(В разработке)");

	while (true) {
		switch (menu.easy_run()) {
		case 0:	// поиск
			
			break;
		case 1:	// добавить аккаунт
			AddAccountMenu(logs, server);
			break;
		case 2:	// назад
			return;
			break;
		}
	}
}

void AddAccountMenu(EasyLogs& logs, ServerData& server) {
	EasyMenu menu;

	menu.set_info("Сервер запущен");
	menu.set_info_main_color(GREEN_COLOR);

	menu.push_back_advanced_cin("ID:");
	menu.set_advanced_cin_new_allowed_chars(0, "1234567890");
	menu.set_advanced_cin_max_input_length(0, 8);
	menu.set_notification(0, "(8 символов!)");

	menu.push_back_advanced_cin("Начальный пароль:");
	menu.set_advanced_cin_new_allowed_chars(1, "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_1234567890");
	menu.set_advanced_cin_ban_not_allowed_on(1);

	menu.push_back_text("Роль пользователя");
	menu.set_notification(2, "(выберите 1 из вариантов)");

	menu.push_back_checkbox("Студент");

	menu.push_back_checkbox("Преподаватель");

	menu.push_back_checkbox("Админ");

	menu.push_back_advanced_cin("Имя:");
	menu.set_advanced_cin_new_allowed_chars(6, "йцукенгшщзхъфывапролджэячсмитьбюЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ ");

	menu.push_back_advanced_cin("Фамилия:");
	menu.set_advanced_cin_new_allowed_chars(7, "йцукенгшщзхъфывапролджэячсмитьбюЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ ");
	
	menu.push_back_advanced_cin("Отчество:");
	menu.set_advanced_cin_new_allowed_chars(8, "йцукенгшщзхъфывапролджэячсмитьбюЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ ");

	menu.push_back_advanced_cin("Факультет:");
	menu.set_advanced_cin_new_allowed_chars(9, "ЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮ");
	menu.set_advanced_cin_max_input_length(9, 5);

	menu.push_back_butt("Создать аккаунт");
	menu.set_notification_color(10, RED_COLOR);

	menu.push_back_butt("Назад");

	while (true) {
		switch (menu.easy_run()) {
		case 9:		// создать аккаунт
			if (menu.is_all_advanced_cin_correct() == false) {
				menu.set_notification(10, "(исправьте все ошибки ввода!)");
				break;
			}

			if (menu.get_advanced_cin_input(0).length() != 8) {
				menu.set_notification(10, "(Длина ввода должа быть = 8)");
				break;
			}

			if (menu.get_advanced_cin_input(0)[0] == '0') {
				menu.set_notification(10, "(ID не может начинаться с \'0\'!)");
				break;
			}

			if (menu.get_advanced_cin_input(1).length() < 5) {
				menu.set_notification(10, "(Мин длина пароля 5 символов!)");
				break;
			}

			{
				uint32_t counter{ 0 };
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++)
					counter += menu.get_all_checkbox_status()[i];

				if (counter == 0) {
					menu.set_notification(10, "(Выберите роль!)");
					break;
				} else if (counter > 1) {
					menu.set_notification(10, "(Выберите всего 1 роль!)");
					break;
				}
			}

			if (menu.get_advanced_cin_input(6).length() < 2) {
				menu.set_notification(10, "(Длина имени от 2-х символов!)");
				break;
			}

			if (menu.get_advanced_cin_input(7).length() < 2) {
				menu.set_notification(10, "(Длина фамилии от 2-х символов!)");
				break;
			}

			if (menu.get_advanced_cin_input(8).length() < 2) {
				menu.set_notification(10, "(Длина отчества от 2-х символов!)");
				break;
			}

			if (menu.get_advanced_cin_input(9).length() < 3) {
				menu.set_notification(10, "(Длина факльтета от 3-х символов!)");
				break;
			}

			// если дошли сюда => не было ошибок
			menu.set_notification(10, "");

			{
				uint32_t type{ 0 };
				for (uint32_t i{ 0 }; i < menu.get_all_checkbox_status().size(); i++)
					if (menu.get_all_checkbox_status()[i] == true)
						type = i + 1;

				if (server.insert_new_account(std::stoi(menu.get_advanced_cin_input(0)), type, menu.get_advanced_cin_input(1), menu.get_advanced_cin_input(6), menu.get_advanced_cin_input(7), menu.get_advanced_cin_input(8), menu.get_advanced_cin_input(9))) {
					logs.insert(EL_ACTION, EL_SECURITY, "Добавлен новый пользоатель ID:" + menu.get_advanced_cin_input(0) + " [SERVER]");
					return;
				}
				else {
					menu.set_notification(10, "ID занят!");
					logs.insert(EL_ERROR, EL_ACTION, EL_SECURITY, "Неудачная попытка добавления нового пользователя, ID занят:" + menu.get_advanced_cin_input(0) + " [SERVER]");
				}
			}
			
			break;
		case 10:	// назад
			return;
			break;
		}
	}
}