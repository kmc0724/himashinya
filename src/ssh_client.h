#pragma once

#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include <QByteArray>
#include <QString>
#include <QVariantList>

#include <functional>

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

	bool initialize_sftp(QString &home_path);
	bool list_sftp_directory(const QString &path, QString &canonical_path,
				 QVariantList &entries);
	bool download_sftp_file(const QString &remote_path,
				const QString &local_path,
				const std::function<void(quint64, quint64)> &progress,
				const std::function<bool()> &cancelled);
	bool upload_sftp_file(const QString &local_path,
			      const QString &remote_path,
			      const std::function<void(quint64, quint64)> &progress,
			      const std::function<bool()> &cancelled);

	bool is_shell_open() const;
	bool is_connected() const;
	bool is_authenticated() const;

	void disconnect();

	QString last_error() const;

	bool is_shell_finished() const;

private:
	ssh_session session = nullptr;
	ssh_channel shell_channel = nullptr;
	sftp_session sftp = nullptr;

	bool authenticated = false;
	QString error_message;
};
