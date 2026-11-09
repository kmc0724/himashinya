
#pragma once

#include "server_profile.h"

#include <QByteArray>
#include <QObject>
#include <QThread>
#include <QVariantList>

class ssh_worker;
class sftp_worker;

class ssh_connection : public QObject
{
	Q_OBJECT

public:
	explicit ssh_connection(const server_profile &server,
				QObject *parent = nullptr);
	~ssh_connection() override;

	void start();
	void close();
	void shutdown();

	void trust_host_key();
	void authenticate(const QString &password);

	void write_shell(const QByteArray &data);
	void resize_shell(int columns, int rows);
	void list_sftp_directory(quint64 request_id, const QString &path);
	void download_sftp_file(quint64 request_id, const QString &remote_path,
				const QString &local_path);
	void upload_sftp_file(quint64 request_id, const QString &local_path,
			      const QString &remote_path);

	bool is_finished() const;

signals:
	void host_key_unknown(const QString &fingerprint);
	void host_key_verified();
	void authenticated();
	void failed(const QString &message);
	void disconnected();
	void finished();

	void trust_requested();
	void authentication_requested(const QString &password);
	void disconnect_requested();
	void sftp_authentication_requested(const QString &password);
	void sftp_disconnect_requested();

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

	void shell_write_requested(const QByteArray &data);
	void shell_resize_requested(int columns, int rows);
	void sftp_list_requested(quint64 request_id, const QString &path);
	void sftp_download_requested(quint64 request_id,
				     const QString &remote_path,
				     const QString &local_path);
	void sftp_upload_requested(quint64 request_id,
				   const QString &local_path,
				   const QString &remote_path);

private:
	QThread thread;
	QThread sftp_thread;
	ssh_worker *worker = nullptr;
	sftp_worker *file_worker = nullptr;

	bool started = false;
	bool closing = false;
	bool finished_state = false;
	bool shell_thread_finished = false;
	bool sftp_thread_finished = false;

	void handle_shell_thread_finished();
	void update_finished_state();
};
