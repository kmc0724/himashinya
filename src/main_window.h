#pragma once

#include "server_profile.h"
#include "server_repository.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>

class QListWidget;
class QTabWidget;
class QThread;
class QWidget;
class ssh_worker;

class main_window : public QMainWindow
{
public:
	explicit main_window(QWidget *parent = nullptr);
	~main_window() override;

	void connect_server();

private:
	struct ssh_connection {
		QPointer<QThread> thread;
		QPointer<ssh_worker> worker;
		QPointer<QWidget> page;
		bool closing = false;
	};

	void setup_ui();
	void setup_style();

	void refresh_servers();
	void add_server();
	void edit_server();
	void delete_server();

	void close_terminal_tab(int index);
	void close_ssh_connection(ssh_connection *connection);

	QTabWidget *sidebar_tabs = nullptr;
	QTabWidget *terminal_tabs = nullptr;
	QListWidget *server_list = nullptr;

	server_repository repository;
	QList<server_profile> servers;
	QList<ssh_connection *> connections;
};