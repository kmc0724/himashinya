#pragma once

#include "server_profile.h"

#include <QList>

class server_repository
{
public:
	QList<server_profile> load() const;
	void save(const QList<server_profile> &servers) const;
};
