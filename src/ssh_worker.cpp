
#include "ssh_worker.h"

#include <QTimer>

ssh_worker::ssh_worker(const QString &host, int port, const QString &username)
    : host(host), port(port), username(username)
{
}

void ssh_worker::connect_to_server()
{
	if (stopping)
		return;

	if (!client.connect_to_server(host, port, username)) {
		emit failed(client.last_error());
		return;
	}

	switch (client.check_host_key()) {
	case ssh_client::host_key_status::trusted:
		emit host_key_verified();
		break;

	case ssh_client::host_key_status::unknown:
		emit host_key_unknown(client.host_key_fingerprint());
		break;

	default:
		emit failed(client.last_error());
		break;
	}
}

void ssh_worker::trust_host_key()
{
	if (stopping)
		return;

	if (!client.trust_host_key()) {
		emit failed(client.last_error());
		return;
	}

	emit host_key_verified();
}

void ssh_worker::authenticate(const QString &password)
{
	if (stopping)
		return;

	if (!client.authenticate_password(password)) {
		emit failed(client.last_error());
		return;
	}

	emit authenticated();

	if (!client.open_shell()) {
		emit failed(client.last_error());
		return;
	}

	shell_timer = new QTimer(this);
	shell_timer->setInterval(5);

	connect(shell_timer, &QTimer::timeout, this, &ssh_worker::poll_shell);

	shell_timer->start();

	emit shell_ready();

	QString home_path;
	if (client.initialize_sftp(home_path))
		emit sftp_ready(home_path, {});
	else
		emit sftp_ready({}, client.last_error());
}

void ssh_worker::write_shell(const QByteArray &data)
{
	if (stopping || client.is_shell_finished())
		return;

	if (!client.write_shell(data)) {
		if (!client.is_shell_finished())
			emit failed(client.last_error());

		disconnect();
	}
}

void ssh_worker::resize_shell(int columns, int rows)
{
	if (stopping || client.is_shell_finished())
		return;

	if (!client.resize_shell(columns, rows)) {
		if (!client.is_shell_finished())
			emit failed(client.last_error());

		disconnect();
	}
}

void ssh_worker::list_sftp_directory(quint64 request_id,
				     const QString &path)
{
	if (stopping)
		return;

	QString canonical_path;
	QVariantList entries;
	const bool success = client.list_sftp_directory(
	    path, canonical_path, entries);
	emit sftp_directory_list(
	    request_id, canonical_path, entries,
	    success ? QString() : client.last_error());
}

void ssh_worker::download_sftp_file(quint64 request_id,
				    const QString &remote_path,
				    const QString &local_path)
{
	if (stopping)
		return;

	const bool success = client.download_sftp_file(
	    remote_path, local_path,
	    [this, request_id](quint64 transferred, quint64 total) {
		    emit sftp_transfer_progress(request_id, transferred,
						total);
	    });
	emit sftp_transfer_finished(
	    request_id, success,
	    success ? QString() : client.last_error());
}

void ssh_worker::upload_sftp_file(quint64 request_id,
				  const QString &local_path,
				  const QString &remote_path)
{
	if (stopping)
		return;

	const bool success = client.upload_sftp_file(
	    local_path, remote_path,
	    [this, request_id](quint64 transferred, quint64 total) {
		    emit sftp_transfer_progress(request_id, transferred,
						total);
	    });
	emit sftp_transfer_finished(
	    request_id, success,
	    success ? QString() : client.last_error());
}

void ssh_worker::poll_shell()
{
	if (stopping)
		return;

	if (client.is_shell_finished()) {
		disconnect();
		return;
	}

	QByteArray data;

	for (int i = 0; i < 16; ++i) {
		int received = client.read_shell(data);

		if (received < 0) {
			emit failed(client.last_error());
			disconnect();
			return;
		}

		if (received > 0)
			emit shell_output(data);

		if (client.is_shell_finished()) {
			disconnect();
			return;
		}

		if (received == 0)
			break;
	}
}

void ssh_worker::disconnect()
{
	if (stopping)
		return;

	stopping = true;

	if (shell_timer) {
		shell_timer->stop();
		shell_timer->deleteLater();
		shell_timer = nullptr;
	}

	client.disconnect();
	emit disconnected();
}
