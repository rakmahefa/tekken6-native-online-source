// Tekken 6 Native Online launcher - synchronous HTTP bridge implemented with Qt Network.
// Qt's network stack is portable across Linux and Windows and continues to use
// the platform TLS backend for HTTPS requests.

#include "http.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {
int httpRequest(const QUrl& url, const QByteArray& verb, const QString& headers, const QByteArray& body,
                QByteArray& out, bool local,
                const std::function<void(qint64, qint64)>& progress = {})
{
	out.clear();
	if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")))
		return 0;

	QNetworkAccessManager manager;
	if (local)
		manager.setProxy(QNetworkProxy::NoProxy);

	QNetworkRequest request(url);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
	                     QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Tekken6NativeOnlineLauncher"));
	for (const QString& line : headers.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts)) {
		const qsizetype colon = line.indexOf(QLatin1Char(':'));
		if (colon <= 0)
			continue;
		request.setRawHeader(line.left(colon).trimmed().toLatin1(), line.mid(colon + 1).trimmed().toLatin1());
	}

	QNetworkReply* reply = nullptr;
	if (verb == QByteArrayLiteral("GET") && body.isEmpty())
		reply = manager.get(request);
	else if (verb == QByteArrayLiteral("POST"))
		reply = manager.post(request, body);
	else
		reply = manager.sendCustomRequest(request, verb, body);

	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	timeout.start(local ? 5000 : 90000);
	QByteArray received;
	bool tooLarge = false;
	QObject::connect(reply, &QIODevice::readyRead, &loop, [&] {
		received += reply->readAll();
		if (local && received.size() > (1 << 20)) {
			tooLarge = true;
			reply->abort();
		}
	});
	QObject::connect(reply, &QNetworkReply::downloadProgress, &loop,
	                 [&](qint64 got, qint64 total) { if (progress) progress(got, total > 0 ? total : -1); });
	QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	received += reply->readAll();
	const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	const bool networkFailed = reply->error() != QNetworkReply::NoError;
	out = received;
	reply->deleteLater();
	if (tooLarge || (networkFailed && status == 0))
		return 0;
	return status;
}
} // namespace

// GET over HTTPS (GitHub releases and assets), with progress updates for the UI.
int httpGet(const wchar_t* url, QByteArray& out, const std::function<void(qint64, qint64)>& progress)
{
	if (!url)
		return 0;
	return httpRequest(QUrl(QString::fromWCharArray(url)),
	                   QByteArrayLiteral("GET"),
	                   QStringLiteral("Accept: application/vnd.github+json, application/octet-stream\r\n"),
	                   {}, out, false, progress);
}

// Plain HTTP on the local network (router UPnP).
int httpLocal(const QString& url, const wchar_t* verb, const QString& headers, const QByteArray& body, QByteArray& out)
{
	if (!verb)
		return 0;
	return httpRequest(QUrl(url), QString::fromWCharArray(verb).toLatin1(), headers, body, out, true);
}
