
#include "ssh_connection.h"
#include "ssh_worker.h"

ssh_connection::ssh_connection(const server_profile &server, QObject *parent)
    : QObject(parent)
{
	worker = new ssh_worker(server.host, server.port, server.username);
	worker->moveToThread(&thread);

	connect(&thread, &QThread::started, worker,
		&ssh_worker::connect_to_server);

	connect(this, &ssh_connection::trust_requested, worker,
		&ssh_worker::trust_host_key, Qt::QueuedConnection);

	connect(this, &ssh_connection::authentication_requested, worker,
		&ssh_worker::authenticate, Qt::QueuedConnection);

	connect(this, &ssh_connection::disconnect_requested, worker,
		&ssh_worker::disconnect, Qt::QueuedConnection);

	connect(worker, &ssh_worker::host_key_unknown, this,
		&ssh_connection::host_key_unknown);

	connect(worker, &ssh_worker::host_key_verified, this,
		&ssh_connection::host_key_verified);

	connect(worker, &ssh_worker::authenticated, this,
		&ssh_connection::authenticated);

	connect(worker, &ssh_worker::failed, this, &ssh_connection::failed);

	connect(worker, &ssh_worker::disconnected, this,
		&ssh_connection::disconnected);

	connect(worker, &ssh_worker::disconnected, &thread, &QThread::quit,
		Qt::DirectConnection);

	connect(&thread, &QThread::finished, worker, &QObject::deleteLater);

	connect(&thread, &QThread::finished, this, [this]() {
		finished_state = true;
		emit finished();
	});

	connect(this, &ssh_connection::shell_write_requested, worker,
		&ssh_worker::write_shell, Qt::QueuedConnection);

	connect(this, &ssh_connection::shell_resize_requested, worker,
		&ssh_worker::resize_shell, Qt::QueuedConnection);

	connect(worker, &ssh_worker::shell_ready, this,
		&ssh_connection::shell_ready);

	connect(worker, &ssh_worker::shell_output, this,
		&ssh_connection::shell_output);
}

ssh_connection::~ssh_connection()
{
	shutdown();
}

void ssh_connection::shutdown()
{
	if (!started)
		return;

	close();
	if (thread.isRunning())
		thread.wait();
}

void ssh_connection::start()
{
	if (started || closing)
		return;

	started = true;
	thread.start();
}

void ssh_connection::close()
{
	if (closing)
		return;

	closing = true;

	if (!started)
		return;

	emit disconnect_requested();
}

void ssh_connection::trust_host_key()
{
	if (!closing)
		emit trust_requested();
}

void ssh_connection::authenticate(const QString &password)
{
	if (!closing)
		emit authentication_requested(password);
}

void ssh_connection::write_shell(const QByteArray &data)
{
	if (!closing)
		emit shell_write_requested(data);
}

void ssh_connection::resize_shell(int columns, int rows)
{
	if (!closing)
		emit shell_resize_requested(columns, rows);
}

bool ssh_connection::is_finished() const
{
	return finished_state;
}