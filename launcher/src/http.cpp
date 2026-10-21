#if defined(_WIN32)
// Tekken 6 Native Online launcher - HTTP requests (WinHTTP).

#include "http.h"

// One WinHTTP request (system TLS for https, redirects followed). Returns the HTTP status, 0 when it failed.
// local: the router on this network (no proxy, 2-3 s timeouts, at most 1 MB of answer); else the internet through the
// system proxy. progress(got, total) after every part of the answer (total -1 = no Content-Length).
struct HttpOptions
{
	const wchar_t* verb = L"GET";
	std::wstring headers;
	QByteArray body;
	bool local = false;
	std::function<void(qint64, qint64)> progress;
};

static int httpRequest(const std::wstring& url, const HttpOptions& o, QByteArray& out)
{
	out.clear();
	URL_COMPONENTS u{};
	u.dwStructSize = sizeof(u);
	wchar_t host[256]{}, path[2048]{}, extra[2048]{};
	u.lpszHostName = host;
	u.dwHostNameLength = DWORD(std::size(host));
	u.lpszUrlPath = path;
	u.dwUrlPathLength = DWORD(std::size(path));
	u.lpszExtraInfo = extra;
	u.dwExtraInfoLength = DWORD(std::size(extra));
	if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u))
		return 0;
	const std::wstring object = std::wstring(path) + extra;
	int status = 0;
	HINTERNET s = WinHttpOpen(L"Tekken6NativeOnlineLauncher", o.local ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
	                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (s && o.local)
		WinHttpSetTimeouts(s, 2000, 2000, 3000, 3000);
	HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : nullptr;
	HINTERNET r = c ? WinHttpOpenRequest(c, o.verb, object.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
	                                     u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
	                : nullptr;
	if (r && WinHttpSendRequest(r, o.headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : o.headers.c_str(), o.headers.empty() ? 0 : DWORD(-1),
	                            o.body.isEmpty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(o.body.constData()), DWORD(o.body.size()),
	                            DWORD(o.body.size()), 0) &&
	    WinHttpReceiveResponse(r, nullptr))
	{
		DWORD code = 0, size = sizeof(code);
		WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);
		wchar_t len[32]{};
		DWORD lenSize = sizeof(len);
		const qint64 total = WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, len, &lenSize, WINHTTP_NO_HEADER_INDEX)
		                         ? QString::fromWCharArray(len).toLongLong()
		                         : -1;
		bool ok = true;
		for (DWORD avail = 0; ok && WinHttpQueryDataAvailable(r, &avail) && avail && (!o.local || out.size() < (1 << 20));)
		{
			const qsizetype at = out.size();
			out.resize(at + avail);
			DWORD read = 0;
			ok = WinHttpReadData(r, out.data() + at, avail, &read);
			out.resize(at + read);
			if (o.progress)
				o.progress(out.size(), total);
		}
		status = ok ? int(code) : 0;
	}
	for (HINTERNET h : {r, c, s})
		if (h)
			WinHttpCloseHandle(h);
	return status;
}

// GET over HTTPS (GitHub releases and assets)
int httpGet(const wchar_t* url, QByteArray& out, const std::function<void(qint64, qint64)>& progress)
{
	HttpOptions o;
	o.headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
	o.progress = progress;
	return httpRequest(url, o, out);
}

// Plain HTTP on the local network (the router's UPnP)
int httpLocal(const QString& url, const wchar_t* verb, const QString& headers, const QByteArray& body, QByteArray& out)
{
	HttpOptions o;
	o.verb = verb;
	o.headers = headers.toStdWString();
	o.body = body;
	o.local = true;
	return httpRequest(url.toStdWString(), o, out);
}

#else
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

#endif
