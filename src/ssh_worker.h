
#pragma once

#include "ssh_client.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariantList>

class QTimer;

class ssh_worker : public QObject
{
	Q_OBJECT

public:
	explicit ssh_worker(const QString &host, int port,
			    const QString &username);
	~ssh_worker() override = default;

public slots:
	void connect_to_server();
	void trust_host_key();
	void authenticate(const QString &password);

	void write_shell(const QByteArray &data);
	void resize_shell(int columns, int rows);
	void list_sftp_directory(quint64 request_id, const QString &path);
	void download_sftp_file(quint64 request_id, const QString &remote_path,
				const QString &local_path);
	void upload_sftp_file(quint64 request_id, const QString &local_path,
			      const QString &remote_path);

	void disconnect();

signals:
	void host_key_unknown(const QString &fingerprint);
	void host_key_verified();
	void authenticated();

	void shell_ready();
	void shell_output(const QByteArray &data);
	void sftp_ready(const QString &home_path, const QString &error);
	void sftp_directory_list(quint64 request_id, const QString &path,
				 const QVariantList &entries,
				 const QString &error);
	void sftp_transfer_progress(quint64 request_id, quint64 transferred,
				    quint64 total);
	void sftp_transfer_finished(quint64 request_id, bool success,
				    const QString &error);

	void failed(const QString &message);
	void disconnected();

private slots:
	void poll_shell();

private:
	ssh_client client;

	QString host;
	int port;
	QString username;

	QTimer *shell_timer = nullptr;
	bool stopping = false;
};
