/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/12 16:00:24 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 16:18:54 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 1234
#define PASSWORD "1234"
#define BOT_NICK "Rafik"

void sendData(int sock, std::string data) {
	data += "\r\n";
	send(sock, data.c_str(), data.length(), 0);
	std::cout << ">> " << data;
}

int main() 
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		std::cerr << "socket() function failed : " << strerror(errno) << std::endl;
		return 1;
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = inet_addr(SERVER_IP);

	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		std::cerr << "Error: Failed to connect to the server." << strerror(errno) << std::endl;
		return 1;
	}
	std::cout << "Connected to the server !" << std::endl;

	sendData(sock, "PASS " + std::string(PASSWORD));
	sendData(sock, "NICK " + std::string(BOT_NICK));
	sendData(sock, "USER Rafik 0 * :Rafik le Bot");

	char buffer[513];
	while (true) {
		std::memset(buffer, 0, 513);
		int bytes = recv(sock, buffer, 513 - 1, 0);
		if (bytes <= 0)
			break;

		std::string raw(buffer);
		if (raw.find("PING") == 0) {
			std::string token = raw.substr(5);
			sendData(sock, "PONG " + token);
			continue;
		}
		size_t privmsgPos = raw.find(" PRIVMSG ");
		if (privmsgPos != std::string::npos) {
			std::string sender = raw.substr(1, raw.find("!") - 1);
			size_t chanStart = privmsgPos + 9;
			size_t chanEnd = raw.find(" ", chanStart);
			std::string target = raw.substr(chanStart, chanEnd - chanStart);
			size_t msgStart = raw.find(" :", chanEnd);
			if (msgStart != std::string::npos) {
				std::string message = raw.substr(msgStart + 2);
				if (message.length() > 0 && message[message.length()-1] == '\n') message.erase(message.length()-1);
				if (message.length() > 0 && message[message.length()-1] == '\r') message.erase(message.length()-1);
				if (message == "!hello") {
					sendData(sock, "PRIVMSG " + target + " :Salut " + sender + " ! Je suis un bot codé en C++98.");
				}
				
				else if (message == "!help") {
					sendData(sock, "PRIVMSG " + target + " :Commands available: !hello, !date, !ping, !rtfm");
				}

				else if (message == "!date") {
					 sendData(sock, "PRIVMSG " + target + " :Time to code !");
				}

				else if (message == "!rtfm") {
					sendData(sock, "PRIVMSG " + target + " :READ THE F*CK*NG  MANUEL!");
				}
				
				else if (message == "!ping") {
					sendData(sock, "PRIVMSG " + target + " :PONG !");
				}
			}
		}
	}
	close(sock);
	return 0;
}
