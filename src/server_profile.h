#pragma once

#include <QString>

struct server_profile {
	QString id;
	QString name;
	QString host;
	quint16 port = 22;
	QString username;
};
