// Tekken 6 Native Online launcher - The lobby server's address and pinned certificate.

#include "server.h"

static QString serverAddress()
{
	const QString env = qEnvironmentVariable("T6_LOBBY_SERVER");
	return env.isEmpty() ? QString(kServer) : env;
}

ServerAddr serverAddr()
{
	ServerAddr a;
	QString srv = serverAddress();
	a.tls = srv.startsWith("tls://");
	if (a.tls)
		srv = srv.mid(6);
	const int colon = srv.lastIndexOf(':');
	a.host = colon > 0 ? srv.left(colon) : srv;
	if (colon > 0)
		a.port = srv.mid(colon + 1).toUShort();
	return a;
}

// The lobby server's leaf certificate is pinned exactly; never fall back to
// accepting arbitrary certificates. Public builds can provide the maintainer's
// private/test pin via T6_LOBBY_CERT without embedding it in the public tree.
const QSslCertificate& pinnedCert()
{
	static const QSslCertificate cert = [] {
		const QString overridePath = qEnvironmentVariable("T6_LOBBY_CERT").trimmed();
		QFile f(overridePath.isEmpty() ? QStringLiteral(":/tls/t6_cert.pem") : overridePath);
		return f.open(QIODevice::ReadOnly) ? QSslCertificate(&f, QSsl::Pem) : QSslCertificate();
	}();
	return cert;
}
