
#pragma once

#include "server_profile.h"

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

	void trust_host_key();
	void authenticate(const QString &password);

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

private:
	QThread thread;
	ssh_worker *worker = nullptr;

	bool started = false;
	bool closing = false;
};
