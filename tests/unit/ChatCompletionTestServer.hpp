#pragma once

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <stdexcept>
#include <vector>

/** @brief Loopback HTTP fixture that records complete chat-completion requests. */
class ChatCompletionTestServer {
public:
    /** @brief Captured HTTP headers and parsed request body. */
    struct Request { QByteArray headers; QJsonObject json; };
    /** @brief HTTP status and response body returned by the fixture. */
    struct Reply { int status{200}; QByteArray body; QByteArray extra_headers; };
    std::vector<Request> requests;
    std::function<std::optional<Reply>(const Request&)> respond;

    /** @brief Listen on an ephemeral loopback port. */
    ChatCompletionTestServer()
    {
        if (!server_.listen(QHostAddress::LocalHost, 0)) {
            throw std::runtime_error("Could not start completion test server.");
        }
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this] {
            while (auto* socket = server_.nextPendingConnection()) {
                attach(socket);
            }
        });
    }

    /** @brief Return the local API base URL. */
    std::string url() const
    {
        return "http://127.0.0.1:" + std::to_string(server_.serverPort()) + "/v1";
    }

    /**
     * @brief Run a blocking API client off-thread while serving HTTP on the Qt thread.
     * @param task Callable to execute, including exception propagation.
     * @return Task result.
     */
    template<class Task> auto run(Task task)
    {
        auto future = std::async(std::launch::async, std::move(task));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (future.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (std::chrono::steady_clock::now() > deadline) {
                throw std::runtime_error("Completion test timed out.");
            }
        }
        return future.get();
    }

    /**
     * @brief Build a normal assistant reply.
     * @param text Assistant text.
     * @return Serialized chat-completion response.
     */
    static Reply answer(const QString& text)
    {
        const QJsonObject message{{"content", text}};
        const QJsonObject response{{"choices", QJsonArray{QJsonObject{{"message", message}}}}};
        return {200, QJsonDocument(response).toJson(QJsonDocument::Compact)};
    }

private:
    /** @brief Read one complete HTTP request and dispatch the fixture's reply.
     * @param socket Accepted loopback connection owned by the server.
     */
    void attach(QTcpSocket* socket)
    {
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, &server_,
            [this, socket, buffer = QByteArray(), handled = false]() mutable {
                buffer += socket->readAll();
                if (handled) return;
                const auto header_end = buffer.indexOf("\r\n\r\n");
                if (header_end < 0) return;
                qsizetype length = 0;
                for (const auto& line : buffer.left(header_end).split('\n')) {
                    if (line.toLower().startsWith("content-length:")) {
                        length = line.mid(sizeof("content-length:") - 1).trimmed().toLongLong();
                    }
                }
                constexpr qsizetype kHeaderSeparatorBytes = 4;
                const auto body_start = header_end + kHeaderSeparatorBytes;
                if (buffer.size() - body_start < length) return;
                handled = true;
                requests.push_back({buffer.left(header_end),
                    QJsonDocument::fromJson(buffer.mid(body_start, length)).object()});
                const auto reply = respond ? respond(requests.back()) : answer(QStringLiteral("test reply"));
                if (!reply) return;
                socket->write("HTTP/1.1 " + QByteArray::number(reply->status) +
                    " Test\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                    QByteArray::number(reply->body.size()) + "\r\n" + reply->extra_headers + "\r\n" + reply->body);
                socket->disconnectFromHost();
            });
    }
    QTcpServer server_;
};
