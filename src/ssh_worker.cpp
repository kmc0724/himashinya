#include "ssh_worker.h"

ssh_worker::ssh_worker(const QString &host, int port, const QString &username)
    : host(host), port(port), username(username)
{
}

void ssh_worker::connect_to_server()
{
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
	if (!client.trust_host_key()) {
		emit failed(client.last_error());
		return;
	}

	emit host_key_verified();
}

void ssh_worker::authenticate(const QString &password)
{
	if (!client.authenticate_password(password)) {
		emit failed(client.last_error());
		return;
	}

	emit authenticated();
}

void ssh_worker::disconnect()
{
	client.disconnect();
	emit disconnected();
}

void ssh_worker::shutdown()
{
	client.disconnect();
	emit disconnected();
}