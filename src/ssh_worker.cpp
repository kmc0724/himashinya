
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
	shell_timer->setInterval(20);

	connect(shell_timer, &QTimer::timeout, this, &ssh_worker::poll_shell);

	shell_timer->start();

	emit shell_ready();
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
