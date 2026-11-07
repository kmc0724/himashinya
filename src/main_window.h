#pragma once

#include "server_profile.h"
#include "server_repository.h"

#include <QList>
#include <QMainWindow>

class QListWidget;
class QTabWidget;

class main_window : public QMainWindow
{
public:
	explicit main_window(QWidget *parent = nullptr);

private:
	void setup_ui();
	void setup_style();

	void refresh_servers();
	void add_server();
	void edit_server();
	void delete_server();

	QTabWidget *sidebar_tabs = nullptr;
	QTabWidget *terminal_tabs = nullptr;
	QListWidget *server_list = nullptr;

	server_repository repository;
	QList<server_profile> servers;
};
