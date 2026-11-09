#include "sftp_worker.h"

sftp_worker::sftp_worker(const QString &host, int port,
			 const QString &username)
    : host(host), port(port), username(username)
{
}

void sftp_worker::connect_and_authenticate(const QString &password)
{
	if (stop_requested.load(std::memory_order_acquire) || initialized)
		return;

	if (!client.connect_to_server(host, port, username)) {
		emit ready({}, client.last_error());
		return;
	}

	if (client.check_host_key() != ssh_client::host_key_status::trusted) {
		emit ready({}, client.last_error().isEmpty()
				    ? QString("SSH host key is not trusted.")
				    : client.last_error());
		client.disconnect();
		return;
	}

	if (!client.authenticate_password(password)) {
		emit ready({}, client.last_error());
		client.disconnect();
		return;
	}

	QString home_path;
	if (!client.initialize_sftp(home_path)) {
		emit ready({}, client.last_error());
		client.disconnect();
		return;
	}

	initialized = true;
	emit ready(home_path, {});
}

void sftp_worker::list_directory(quint64 request_id, const QString &path)
{
	if (stop_requested.load(std::memory_order_acquire))
		return;

	QString canonical_path;
	QVariantList entries;
	const bool success =
	    initialized &&
	    client.list_sftp_directory(path, canonical_path, entries);
	emit directory_list(
	    request_id, canonical_path, entries,
	    success ? QString()
		    : initialized
			  ? client.last_error()
			  : QString("SFTP connection is unavailable."));
}

void sftp_worker::download_file(quint64 request_id,
				const QString &remote_path,
				const QString &local_path)
{
	if (stop_requested.load(std::memory_order_acquire))
		return;

	const bool success =
	    initialized &&
	    client.download_sftp_file(
		remote_path, local_path,
		[this, request_id](quint64 transferred, quint64 total) {
			emit transfer_progress(request_id, transferred, total);
		},
		[this]() {
			return stop_requested.load(std::memory_order_acquire);
		});
	emit transfer_finished(
	    request_id, success,
	    success ? QString()
		    : initialized
			  ? client.last_error()
			  : QString("SFTP connection is unavailable."));
}

void sftp_worker::upload_file(quint64 request_id,
			      const QString &local_path,
			      const QString &remote_path)
{
	if (stop_requested.load(std::memory_order_acquire))
		return;

	const bool success =
	    initialized &&
	    client.upload_sftp_file(
		local_path, remote_path,
		[this, request_id](quint64 transferred, quint64 total) {
			emit transfer_progress(request_id, transferred, total);
		},
		[this]() {
			return stop_requested.load(
			    std::memory_order_acquire);
		});
	emit transfer_finished(
	    request_id, success,
	    success ? QString()
		    : initialized
			  ? client.last_error()
			  : QString("SFTP connection is unavailable."));
}

void sftp_worker::disconnect()
{
	if (disconnect_handled)
		return;

	disconnect_handled = true;
	request_stop();
	client.disconnect();
	emit disconnected();
}

void sftp_worker::request_stop()
{
	stop_requested.store(true, std::memory_order_release);
}
