#pragma once

#include <libssh/libssh.h>

#include <QByteArray>
#include <QString>

class ssh_client
{
public:
	enum class host_key_status { trusted, unknown, changed, other, error };

	ssh_client() = default;
	~ssh_client();

	ssh_client(const ssh_client &) = delete;
	ssh_client &operator=(const ssh_client &) = delete;

	bool connect_to_server(const QString &host, int port,
			       const QString &username);

	host_key_status check_host_key();
	QString host_key_fingerprint() const;
	bool trust_host_key();

	bool authenticate_password(const QString &password);

	bool open_shell(int columns = 80, int rows = 24);
	bool write_shell(const QByteArray &data);
	int read_shell(QByteArray &data);
	bool resize_shell(int columns, int rows);
	void close_shell();

	bool is_shell_open() const;
	bool is_connected() const;
	bool is_authenticated() const;

	void disconnect();

	QString last_error() const;

	bool is_shell_finished() const;

private:
	ssh_session session = nullptr;
	ssh_channel shell_channel = nullptr;

	bool authenticated = false;
	QString error_message;
};
