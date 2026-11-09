#pragma once

#include "ssh_client.h"

#include <QObject>
#include <QVariantList>

#include <atomic>

class sftp_worker : public QObject
{
	Q_OBJECT

public:
	sftp_worker(const QString &host, int port, const QString &username);
	~sftp_worker() override = default;

public slots:
	void connect_and_authenticate(const QString &password);
	void list_directory(quint64 request_id, const QString &path);
	void download_file(quint64 request_id, const QString &remote_path,
			   const QString &local_path);
	void upload_file(quint64 request_id, const QString &local_path,
			 const QString &remote_path);
	void disconnect();
	void request_stop();

signals:
	void ready(const QString &home_path, const QString &error);
	void directory_list(quint64 request_id, const QString &path,
			    const QVariantList &entries, const QString &error);
	void transfer_progress(quint64 request_id, quint64 transferred,
			       quint64 total);
	void transfer_finished(quint64 request_id, bool success,
			       const QString &error);
	void disconnected();

private:
	ssh_client client;
	QString host;
	int port;
	QString username;
	std::atomic_bool stop_requested = false;
	bool disconnect_handled = false;
	bool initialized = false;
};
