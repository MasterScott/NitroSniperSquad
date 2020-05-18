#include <QCoreApplication>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include "constants.h"
#include "discordclient.h"
#include "json.hpp"
#include "utils.h"

using json = nlohmann::json;

QNetworkAccessManager *mgr = new QNetworkAccessManager();



int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	QVector<QString> tokens = readLines(a.applicationDirPath() + "/tokens.txt");
	if (tokens.empty())
	{
		qDebug().noquote().nospace() << "Please create " << a.applicationDirPath() << "/tokens.txt.";
		a.exit();
	}

	int accountsOnline = 0;
	QTimer* logger = new QTimer;
	a.connect(logger, &QTimer::timeout, &a, [&]
	{
		if (accountsOnline >= tokens.size() - 1)
			delete logger;

		DiscordClient* client = new DiscordClient(mgr);
		client->login(tokens[accountsOnline]);

		a.connect(client, &DiscordClient::onReady, &a, [](const User& user)
		{
			qDebug().noquote().nospace() << "Client connected : " <<  user.username << "#" << user.discriminator;
		});

		a.connect(client, &DiscordClient::onMessage, &a, [&](const Message& message)
		{
			if (!message.content.contains("discord.gift/")) return;

			a.connect(mgr, &QNetworkAccessManager::finished, &a, [&](QNetworkReply* reply)
			{
				a.disconnect(mgr, &QNetworkAccessManager::finished, &a, nullptr);
				qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
				QString replyText = reply->readAll();
				if (nlohmann::json::parse(replyText.toStdString())["code"] != nullptr)
					switch (static_cast<Constants::DiscordAPI::redeemResponseErrorCode>(nlohmann::json::parse(replyText.toStdString())["code"].get<int>()))
					{
					case Constants::DiscordAPI::redeemResponseErrorCode::INVALID:
						qDebug() << "Invalid";
						break;
					case Constants::DiscordAPI::redeemResponseErrorCode::ALREADY_REDEEMED:
						qDebug() << "Already redeemed";
						break;
					}
				else
					qDebug() << "Successfully redeemed";
			});

			qDebug() << "caca";
			QRegExp reg("(?:discord.gift/\\S+)");
			reg.indexIn(message.content);
			QString code = reg.cap().split("/").back();
			QString url = "https://discordapp.com/api/v6/entitlements/gift-codes/"+code+"/redeem";
			QNetworkRequest *req = new QNetworkRequest(QUrl(url));
			req->setRawHeader("Authorization", tokens[0].toUtf8());
			req->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
			req->setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.130 Safari/537.36 OPR/66.0.3515.115");
			QString data = "";
			mgr->post(*req, data.toUtf8());
			qDebug().noquote().nospace() << getTime() << " -> Nitro gift detected : https://discord.gift/" + code
										 << " ; Sent by " << message.author.username << "#" << message.author.discriminator << " in channel " << message.channelId;
		});

		accountsOnline++;
	});

	logger->start(5000);

	return a.exec();
}
