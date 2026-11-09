
#pragma once

#include "server_profile.h"

#include <QByteArray>
#include <QObject>
#include <QThread>

class ssh_worker;

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

	void shell_ready();
	void shell_output(const QByteArray &data);

	void shell_write_requested(const QByteArray &data);
	void shell_resize_requested(int columns, int rows);

private:
	QThread thread;
	ssh_worker *worker = nullptr;

	bool started = false;
	bool closing = false;
	bool finished_state = false;
};
