#pragma once
// Tekken 6 Native Online launcher - The lobby server's address and pinned certificate.

#include "common.h"

struct ServerAddr
{
	QString host;
	quint16 port = 7700;
	bool tls = false;
};
ServerAddr serverAddr();
const QSslCertificate& pinnedCert();
// False for the public placeholder endpoint or a TLS endpoint without its pinned certificate.
bool serverConfigured();
