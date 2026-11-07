#include "server_repository.h"

#include <QDebug>
#include <QSettings>

QList<server_profile> server_repository::load() const
{
	QSettings settings("Himashinya", "Himashinya");
	QList<server_profile> servers;

	int count = settings.beginReadArray("servers");

	for (int i = 0; i < count; ++i) {
		settings.setArrayIndex(i);

		server_profile server;
		server.id = settings.value("id").toString();
		server.name = settings.value("name").toString();
		server.host = settings.value("host").toString();
		server.port =
		    static_cast<quint16>(settings.value("port", 22).toUInt());
		server.username = settings.value("username").toString();

		servers.append(server);
	}

	settings.endArray();

	return servers;
}

void server_repository::save(const QList<server_profile> &servers) const
{
	QSettings settings("Himashinya", "Himashinya");

	settings.remove("servers");
	settings.beginWriteArray("servers");

	for (int i = 0; i < servers.size(); ++i) {
		settings.setArrayIndex(i);

		const auto &server = servers[i];

		settings.setValue("id", server.id);
		settings.setValue("name", server.name);
		settings.setValue("host", server.host);
		settings.setValue("port", server.port);
		settings.setValue("username", server.username);
	}

	settings.endArray();
	settings.sync();
}
