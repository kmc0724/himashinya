#pragma once

#include "ssh_client.h"

#include <QObject>
#include <QString>

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
	void disconnect();
	void shutdown();

signals:
	void host_key_unknown(const QString &fingerprint);
	void host_key_verified();
	void authenticated();
	void failed(const QString &message);
	void disconnected();

private:
	ssh_client client;
	QString host;
	int port;
	QString username;
};
