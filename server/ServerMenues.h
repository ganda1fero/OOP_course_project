#ifndef SERVERMENUES_H
#define SERVERMENUES_H

#define PASSWORD_FOR_EXIT "Exit_now"

#include <iostream>
#include <vector>

#include "ServerLogic.h"

#include "EasyMenu.h"
#include "EasyLogs.h"

//---------------------------------------------------------- объявление функций

void ServerMenu(ServerData& server, EasyLogs& logs);
bool StopServerMenu(EasyLogs& logs);
void LogsMenu(EasyLogs& logs);
void AccountsMenu(EasyLogs& logs, ServerData& server);
void AddAccountMenu(EasyLogs& logs, ServerData& server);
void FindAccountMenu(EasyLogs& logs, ServerData& server);
void ShowAccountMenu(EasyLogs& logs, ServerData& server, account_note finded_account);


#endif	