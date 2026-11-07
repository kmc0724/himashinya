#pragma once

#include <libssh/libssh.h>

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

	void disconnect();

	bool is_connected() const;
	bool is_authenticated() const;
	QString last_error() const;

	bool is_host_key_trusted();
	bool save_host_key();

private:
	ssh_session session = nullptr;
	bool authenticated = false;
	QString error_message;
};
