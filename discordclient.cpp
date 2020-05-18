#include "constants.h"
#include "discordclient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebSocket>

#include <json.hpp>

void DiscordClient::login(const QString& token)
{
	m_token = token;

	connect(m_networkManager, &QNetworkAccessManager::finished, this, [&](QNetworkReply* reply)
	{
		disconnect(m_networkManager, &QNetworkAccessManager::finished, this, nullptr);
		QString gatewayURI (nlohmann::json::parse(reply->readAll())["url"].get<std::string>().c_str());

		m_websocket->open(gatewayURI);
	});
	QNetworkRequest gatewayRequest(Constants::DiscordAPI::gatewayURI());
	gatewayRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	m_networkManager->get(gatewayRequest);
}

void DiscordClient::messageReceived(const QString& message)
{
	nlohmann::json payload = nlohmann::json::parse(message.toStdString());

	// Switch on OP
	switch (static_cast<Constants::Gateway::OpCode>(payload["op"].get<int>()))
	{
	case Constants::Gateway::OpCode::DISPATCH:
	{
		QString eventName = payload["t"].get<std::string>().c_str();
		if (eventName == "READY")
		{
			m_loggedIn = true;
			m_user.id = payload["d"]["user"]["id"].get<std::string>().c_str();
			m_user.username = payload["d"]["user"]["username"].get<std::string>().c_str();
			m_user.discriminator = payload["d"]["user"]["discriminator"].get<std::string>().c_str();
			emit onReady(m_user);
		}
		else if (eventName == "MESSAGE_CREATE")
		{
			User author;
			author.id = payload["d"]["author"]["id"].get<std::string>().c_str();
			author.username = payload["d"]["author"]["username"].get<std::string>().c_str();
			author.discriminator = payload["d"]["author"]["discriminator"].get<std::string>().c_str();

			Message message;
			message.author = author;
			message.content = payload["d"]["content"].get<std::string>().c_str();
			message.channelId = payload["d"]["channel_id"].get<std::string>().c_str();

			emit onMessage(message);
		}
		break;
	}
	case Constants::Gateway::OpCode::HELLO:
	{
		// Send heartbeats
		m_heartbeat->start(payload["d"]["heartbeat_interval"].get<int>());
		// Identify
		nlohmann::json identifyPayload;
		identifyPayload["op"] = static_cast<int>(Constants::Gateway::OpCode::IDENTIFY);
		identifyPayload["d"]["token"] = m_token.toStdString();
		identifyPayload["d"]["properties"]["$os"] = "Windows";
		identifyPayload["d"]["properties"]["$device"] = "";
		identifyPayload["d"]["properties"]["$browser"] = "Chrome";
		identifyPayload["d"]["intents"]
				= static_cast<int>(Constants::Gateway::Intents::GUILD_MESSAGES)
				  | static_cast<int>(Constants::Gateway::Intents::DIRECT_MESSAGES);
		m_websocket->sendTextMessage(identifyPayload.dump().c_str());
		break;
	}
	default:
		break;
	}
}

void DiscordClient::connectionClosed()
{
	qDebug() << "Connection closed (" << (m_loggedIn ? m_user.username + "#" + m_user.discriminator + ", " + m_user.id : "not logged in") << ")";
}

void DiscordClient::connectionError(QAbstractSocket::SocketError error)
{
	qDebug() << error;
}

void DiscordClient::sendHeartbeat()
{
	qDebug() << "Heartbeat sent";
	nlohmann::json payload;
	payload["op"] = static_cast<int>(Constants::Gateway::OpCode::HEARTBEAT);
	m_websocket->sendTextMessage(payload.dump().c_str());
}
