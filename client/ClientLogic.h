#ifndef CLIENTLOGIC_H
#define CLIENTLOGIC_H

#include <iostream>

#include "EasyMenu.h"

// defines
#define NO_ROLE 0
#define STUDENT_ROLE 1
#define TEACHER_ROLE 2
#define ADMIN_ROLE 3

// классы - структруры

struct screen_data {
	int32_t role{ 0 };	// роль (из server role)
	int32_t type{ 0 };	// тип, например "авторизация"
	int32_t id{ 0 };	// доп id (на всякий)
};

class client_data {
public:
	// методы
	client_data();

	// поля
	EasyMenu menu;
	screen_data screen_info;

};

// функции



#endif