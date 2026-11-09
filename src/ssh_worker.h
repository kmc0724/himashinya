
#pragma once

#include "ssh_client.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class QTimer;

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

	void write_shell(const QByteArray &data);
	void resize_shell(int columns, int rows);

	void disconnect();

signals:
	void host_key_unknown(const QString &fingerprint);
	void host_key_verified();
	void authenticated();

	void shell_ready();
	void shell_output(const QByteArray &data);

	void failed(const QString &message);
	void disconnected();

private slots:
	void poll_shell();

private:
	ssh_client client;

	QString host;
	int port;
	QString username;

	QTimer *shell_timer = nullptr;
	bool stopping = false;
};
