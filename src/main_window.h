#pragma once

#include "server_profile.h"
#include "server_repository.h"

#include <QByteArray>
#include <QFont>
#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>

class QListWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QListWidgetItem;
class QTabWidget;
class QThread;
class QWidget;
class ssh_connection;

class main_window : public QMainWindow
{
public:
	explicit main_window(QWidget *parent = nullptr);
	~main_window() override;

	void connect_server();

protected:
	bool nativeEvent(const QByteArray &event_type, void *message,
			 qintptr *result) override;

private:
	void setup_ui();
	void setup_style();

	void refresh_servers();
	void add_server();
	void edit_server();
	void delete_server();

	void close_terminal_tab(int index);
	void refresh_sftp_directory();
	void navigate_sftp_directory(const QString &path);
	void open_sftp_item(QListWidgetItem *item);
	void upload_sftp_file();
	void download_sftp_file();
	void remove_sftp_connection(ssh_connection *connection);
	void update_sftp_controls();
	void finish_sftp_transfer(quint64 request_id, bool success,
				  const QString &error);
	void setup_menu();
	void show_terminal_font_dialog();
	void apply_terminal_font(const QFont &font);

	QTabWidget *sidebar_tabs = nullptr;
	QTabWidget *terminal_tabs = nullptr;
	QListWidget *server_list = nullptr;
	QComboBox *sftp_connections = nullptr;
	QLineEdit *sftp_path = nullptr;
	QListWidget *sftp_file_list = nullptr;
	QPushButton *sftp_go_button = nullptr;
	QPushButton *sftp_refresh_button = nullptr;
	QPushButton *sftp_upload_button = nullptr;
	QPushButton *sftp_download_button = nullptr;
	QProgressBar *sftp_progress = nullptr;
	QLabel *sftp_status = nullptr;
	QPointer<ssh_connection> sftp_request_connection;
	quint64 next_sftp_request_id = 1;
	quint64 active_sftp_request_id = 0;
	bool sftp_transfer_active = false;
	bool sftp_transfer_is_upload = false;

	server_repository repository;
	QList<server_profile> servers;

	QFont terminal_font;
};