/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Commands.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/24 15:46:43 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 15:22:15 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

std::vector<std::string> Server::splitMessage(std::string input) {
	std::vector<std::string> args;
	size_t pos = 0;

	while (pos < input.length()) {
		// Skip spaces
		while (pos < input.length() && input[pos] == ' ') {
			pos++;
		}
		
		// Check if we are at the end of the string
		if (pos >= input.length()) {
			break;
		}

		// Push everything after the ':' sign 
		if (input[pos] == ':') {
			args.push_back(input.substr(pos + 1));
			// For IRC, ':' is the last args
			return args; 
		}

		// Find the next space
		size_t next_space = input.find(' ', pos);

		if (next_space == std::string::npos) {
			// If no more space, it's the last word
			args.push_back(input.substr(pos));
			break;
		} else {
			// Extract the word between the actual position and the next space
			args.push_back(input.substr(pos, next_space - pos));
			// Put the pos at the space position
			pos = next_space;
		}
	}
	return args;
}

void Server::sendResponse(int fd, std::string response) {
	// Verify if the client still exist
	if (_clients.find(fd) == _clients.end()) {
		std::cerr << "[ERROR] sendResponse: Client " << fd << " not found." << std::endl;
		return;
	}

	if (response.empty())
		return;

	size_t total_sent = 0;
	size_t bytes_left = response.length();
	const char *ptr = response.c_str();

	while (total_sent < response.length()) {
		// MSG_NOSIGNAL : Protect from crach when a client disconnect while sending message
		ssize_t sent = send(fd, ptr + total_sent, bytes_left, MSG_NOSIGNAL);

		if (sent == -1) {
			std::cerr << "[ERROR] Failed to send data to client " << fd << std::endl;
			break;
		}

		total_sent += sent;
		bytes_left -= sent;
	}
	// Log optionnel pour debug (très utile)
	// std::cout << ">> To Client " << fd << ": " << response; 
}

void Server::sendWelcome(int fd) {
	Client *client = &_clients[fd];

	std::string nick = client->getNickname();
	std::string user = client->getUsername();
	std::string host = client->getIPAdress(); 
	// Si host est vide (cas rare), on peut mettre "localhost" par sécurité
	if (host.empty()) host = "localhost";

	// Code 001 : Welcome
	// Résultat : :localhost 001 Tonio :Welcome ... Tonio!tonio@127.0.0.1
	std::string msg = ":localhost 001 " + nick + " :Welcome to the IRC Network, " + nick + "!" + user + "@" + host + "\r\n";
	sendResponse(fd, msg);

	std::cout << "[SUCCESS] Welcome message sent to " << nick << " (" << host << ")" << std::endl;
}

Client	*Server::getClientByNickname(const std::string &nickname) {
	std::map<int, Client>::iterator it;
	
	for (it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second.getNickname() == nickname) {
			return &(it->second);
		}
	}
	return (NULL);
}

void Server::cmd_ping(int fd, const std::vector<std::string> &args)
{
	if (args.empty())
		return;
	std::string response = "PONG " + args[0] + "\r\n";
	sendResponse(fd, response);
}

void Server::cmd_quit(int fd, const std::vector<std::string> &args)
{
	if (_clients.find(fd) == _clients.end())
		return ;
	Client *client = &_clients[fd];
	
	std::string reason = "Client Quit";
	if (!args.empty() && !args[0].empty())
		reason = args[0];
	std::string quitMsg = ":" + client->getNickname()
		+ "!" + client->getUsername()
		+ "@" + client->getIPAdress()
		+ " QUIT :" + reason + "\r\n";
		
	std::vector<std::string> chans = client->getJoinedChannels();
	for (size_t i = 0; i < chans.size(); i++)
	{
		Channel *ch = getChannel(chans[i]);
		if (!ch)
			continue;
	
		ch->broadcast(quitMsg, fd);
		ch->removeClient(fd);
		client->removeChannel(chans[i]);
		if (ch->getClientCount() == 0)
		{
			_channels.erase(chans[i]);
			delete ch;
		}
	}
	onClientDisconnect(fd);
}

void Server::cmd_part(int fd, const std::vector<std::string> &args)
{
	Client *client = &_clients[fd];
	
	if (!client->isRegistered())
	{
		sendResponse(fd, ":localhost 451 :You have not registered\r\n");
		return ;
	}
	if (args.empty() || args[0].empty())
	{
		sendResponse(fd, ":localhost 461 PART :Not enough parameters\r\n");
		return ;
	}
	std::string channelName = args[0];
	Channel *channel = getChannel(channelName);
	if (!channel)
	{
		sendResponse(fd, ":localhost 403 " + client->getNickname() + " " + channelName + " :No such channel\r\n");
		return ;
	}
	if (!channel->isMember(client))
	{
		sendResponse(fd, ":localhost 442 " + client->getNickname() + " " + channelName + " :You're not on that channel\r\n");
		return ;
	}
	std::string reason = "";
	if (args.size() > 1 && !args[1].empty())
		reason = " :" + args[1];
	std::string partMsg = ":" + client->getNickname()
		+ "!" + client->getUsername()
		+ "@" + client->getIPAdress()
		+ " PART " + channelName + reason + "\r\n";
	channel->broadcast(partMsg, -1);
	channel->removeClient(fd);
	client->removeChannel(channelName);
	if (channel->getClientCount() == 0)
	{
		_channels.erase(channelName);
		delete channel;
	}
}

void Server::cmd_kick(int fd, const std::vector<std::string> &args) {
	Client *kicker = &_clients[fd];

	if (!kicker->isRegistered())
	{
		sendResponse(fd, ":localhost 451 :You have not registered\r\n");
		return ;
	}
	if (args.size() < 2 || args[0].empty() || args[1].empty())
	{
		sendResponse(fd, ":localhost 461 KICK :Not enough parameters\r\n");
		return ;
	}
	std::string channelName = args[0];
	std::string targetNick = args[1];
	Channel *channel = getChannel(channelName);
	if (!channel)
	{
		sendResponse(fd, ":localhost 403 " + kicker->getNickname() + " " + channelName + " :No such channel\r\n");
		return ;
	}
	if (!channel->isMember(kicker))
	{
		sendResponse(fd, "Localhost: 442 " + kicker->getNickname() + " " + channelName + " :You're not on that channel\r\n");
		return ;
	}
	if (!channel->isOperator(kicker))
	{
		sendResponse(fd, ":localhost 482 " + kicker->getNickname() + " " + channelName + " :You're not channel operator\r\n");
		return ;
	}
	Client *target = getClientByNickname(targetNick);
	if (!target)
	{
		sendResponse(fd, ":localhost 401 " + kicker->getNickname() + " " + targetNick + " :No such nick/channel\r\n");
		return ;
	}
	if (!channel->isMember(target))
	{
		sendResponse(fd, ":localhost 441 " + kicker->getNickname() + " " + targetNick + " " + channelName + " :They aren't on that channel\r\n");
		return ;
	}
	std::string reason = "Kicked";
	if (args.size() >= 3 && !args[2].empty())
		reason = args[2];
	std::string kickMsg = ":" + kicker->getNickname()
		+ "!" + kicker->getUsername()
		+ "@" + kicker->getIPAdress()
		+ " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
	channel->broadcast(kickMsg, -1);
	channel->removeClient(target->getFd());
	target->removeChannel(channelName);
	if (channel->getClientCount() == 0)
	{
		_channels.erase(channelName);
		delete channel;
	}
}

void Server::cmd_topic(int fd, const std::vector<std::string> &args)
{
	Client *client = &_clients[fd];
	
	if (!client->isRegistered())
	{
		sendResponse(fd, ":localhost 451 : You have not registered\r\n");
		return ;
	}
	if (args.empty() || args[0].empty())
	{
		sendResponse(fd, ":localhost 461 TOPIC : Not enough parameters\r\n");
		return ;
	}
	std::string channelName = args[0];
	Channel *channel = getChannel(channelName);
	if (!channel)
	{
		sendResponse(fd, ":localhost 403 " + client->getNickname() + " " + channelName + " :No such channel\r\n");
		return;
	}
	if (!channel->isMember(client))
	{
		sendResponse(fd, ":localhost 442 " + client->getNickname() + " " + channelName + " :You're not on that channel\r\n");
		return;
	}
	if (args.size() == 1)
	{
		std::string topic = channel->getTopic();
		if (topic.empty())
		{
			sendResponse(fd, ":localhost 331 " + client->getNickname() + " " + channelName + " :No topic is set\r\n");
		} else
		{
			sendResponse(fd, ":localhost 332 " + client->getNickname() + " " + channelName + " :" + topic + "\r\n");
		}
		return;
	}
	if (channel->isTopicRestricted() && !channel->isOperator(client))
	{
		sendResponse(fd, ":localhost 482 " + client->getNickname() + " " + channelName + " :You're not channel operator\r\n");
		return ;
	}
	std::string newTopic = args[1];
	channel->setTopic(newTopic);
	sendResponse(fd, ":localhost NOTICE " + client->getNickname()
		+ " :[DEBUG] topic set to: " + channel->getTopic() + "\r\n");
	std::string topicMsg = ":" + client->getNickname()
		+ "!" + client->getUsername()
		+ "@" + client->getIPAdress()
		+ " TOPIC " + channelName + " :" + newTopic + "\r\n";
	channel->broadcast(topicMsg, -1);
}

void	Server::cmd_invite(int fd, const std::vector<std::string> & args) {
	Client *client = &_clients[fd];
	
	if (!client->isRegistered())
	{
		sendResponse(fd, ":localhost 451 : You have not registered\r\n");
		return ;
	}
	if (args.size() < 2)
	{
		sendResponse(fd, ":localhost 461 INVITE : Not enough parameters\r\n");
		return ;
	}
	std::string	targetNick = args[0];
	std::string channelName = args[1];

	Channel *channel = getChannel(channelName);
	if (!channel)
	{
		sendResponse(fd, ":localhost 403 " + client->getNickname()
			+ " " + channelName + " : No such channel\r\n");
		return ;
	}
	if (!channel->isMember(client))
	{
		sendResponse(fd, ":localhost 442 " + client->getNickname()
			+ " " + targetNick + " : You're not on that channel\r\n");
		return ;
	}
	if (channel->isInviteOnly() && !channel->isOperator(client)) {
		sendResponse(fd, ":localhost 482 " + client->getNickname() + " " + channelName + " :You're not channel operator\r\n");
		return ;
	}
	Client *target = getClientByNickname(targetNick);
	if (!target)
	{
		sendResponse(fd, ":localhost 401 " + client->getNickname()
			+ " " + targetNick + " : No such nickname\r\n");
		return ;
	}
	if (channel->isMember(target))
	{
		sendResponse(fd, ":localhost 443 " + client->getNickname()
			+ " " + targetNick + " " + channelName
			+ " : is already on channel\r\n");
		return ;
	}
	channel->sendInvite(targetNick);
	
	std::string inviteMsg = ":" + client->getNickname()
		+ "!" + client->getUsername()
		+ "@" + client->getIPAdress()
		+ " INVITE " + targetNick + " " + channelName + "\r\n";
	sendResponse(target->getFd(), inviteMsg);
	sendResponse(fd, ":localhost 341 " + client->getNickname()
		+ " " + targetNick + " " + channelName + "\r\n");
}

void Server::cmd_mode(int fd, const std::vector<std::string> &args)
{
	Client *client = &_clients[fd];

	if (!client->isRegistered()) {
		sendResponse(fd, ":localhost 451 :You have not registered\r\n");
		return;
	}
	if (args.size() < 1) {
		sendResponse(fd, ":localhost 461 MODE :Not enough parameters\r\n"); // Correction typo localhost
		return;
	}

	std::string target = args[0];

	// 1. GESTION DES MODES UTILISATEUR (Si la cible n'est pas un channel)
	// Note : Pour 42, on gère souvent juste le fait de ne pas crash.
	// Si ce n'est pas un channel (ne commence pas par # ou &), on regarde si c'est un user.
	if (getChannel(target) == NULL) {
		if (getClientByNickname(target) != NULL) {
			// C'est un user. Si c'est moi, j'envoie mes modes, sinon erreur 502
			if (target == client->getNickname())
				sendResponse(fd, ":localhost 221 " + client->getNickname() + " :+\r\n"); // Simplifié
			else
				sendResponse(fd, ":localhost 502 :Cannot change mode for other users\r\n");
			return;
		}
		sendResponse(fd, ":localhost 403 " + client->getNickname() + " " + target + " :No such channel\r\n");
		return;
	}

	Channel *channel = getChannel(target);

	// 2. CONSULTATION DES MODES (MODE #chan)
	if (args.size() == 1)
	{
		if (!channel->isMember(client)) {
			 // Techniquement on peut voir les modes sans être membre si le channel n'est pas secret,
			 // mais ta protection est acceptable pour le projet.
			sendResponse(fd, ":localhost 442 " + client->getNickname() + " " + target + " :You're not on that channel\r\n");
			return;
		}
		std::string modes = "+";
		std::string params = "";

		if (channel->isInviteOnly()) modes += "i";
		if (channel->isTopicRestricted()) modes += "t";
		if (!channel->getKey().empty()) {
			modes += "k";
			params += " " + channel->getKey();
		}
		if (channel->getUserLimit() > 0) {
			modes += "l";
			// Attention: std::to_string est C++11. Si tu es en C++98, utilise stringstream.
			std::stringstream ss;
			ss << channel->getUserLimit();
			params += " " + ss.str();
		}

		sendResponse(fd, ":localhost 324 " + client->getNickname() + " " + target + " " + modes + params + "\r\n");
		return;
	}

	// 3. MODIFICATION DES MODES (MODE #chan +...)
	if (!channel->isOperator(client)) {
		sendResponse(fd, ":localhost 482 " + client->getNickname() + " " + target + " :You're not channel operator\r\n");
		return;
	}

	std::string modeStr = args[1];
	char sign = '+'; // Par défaut, si pas précisé
	size_t argIndex = 2;
	
	std::string appliedModes = "";
	std::vector<std::string> appliedArgs;

	for (size_t i = 0; i < modeStr.size(); i++)
	{
		char c = modeStr[i];

		if (c == '+' || c == '-') {
			sign = c;
			continue; // On ne stocke pas le signe tout de suite, on l'appliquera si un mode valide suit
		}

		// --- Logique pour 'i' et 't' ---
		if (c == 'i' || c == 't') {
			bool isInvite = (c == 'i');
			bool currentVal = isInvite ? channel->isInviteOnly() : channel->isTopicRestricted();
			bool newVal = (sign == '+');

			// Petite optimisation : On n'applique (et on ne broadcast) que si ça change vraiment quelque chose
			if (currentVal != newVal) {
				if (isInvite) channel->setInviteOnly(newVal);
				else channel->setTopicRestricted(newVal);
				
				appliedModes += sign; // On ajoute le signe actuel (+ ou -)
				appliedModes += c;    // On ajoute la lettre
			}
		}
		// --- Logique pour 'k' ---
		else if (c == 'k') {
			if (sign == '+') {
				if (argIndex >= args.size()) {
					sendResponse(fd, ":localhost 461 " + client->getNickname() + " MODE :Not enough parameters\r\n");
					return;
				}
				std::string key = args[argIndex++];
				channel->setKey(key);
				appliedModes += "+k";
				appliedArgs.push_back(key);
			} else {
				// Remove key
				if (!channel->getKey().empty()) { // Seulement si une clé existait
					channel->setKey("");
					appliedModes += "-k";
				}
			}
		}
		// --- Logique pour 'o' ---
		else if (c == 'o') {
			if (argIndex >= args.size()) {
				sendResponse(fd, ":localhost 461 " + client->getNickname() + " MODE :Not enough parameters\r\n");
				return;
			}
			std::string targetNick = args[argIndex++];
			Client *targetUser = getClientByNickname(targetNick);

			if (targetUser && channel->isMember(targetUser)) {
				if (sign == '+') {
					if (!channel->isOperator(targetUser)) { // Évite doublon
						channel->addOperator(targetUser);
						appliedModes += "+o";
						appliedArgs.push_back(targetNick);
					}
				} else {
					if (channel->isOperator(targetUser)) {
						channel->removeOperator(targetUser->getFd());
						appliedModes += "-o";
						appliedArgs.push_back(targetNick);
					}
				}
			} else {
				sendResponse(fd, ":localhost 401 " + client->getNickname() + " " + targetNick + " :No such nick/channel\r\n");
			}
		}
		// --- Logique pour 'l' ---
		else if (c == 'l') {
			if (sign == '+') {
				if (argIndex >= args.size()) {
					sendResponse(fd, ":localhost 461 " + client->getNickname() + " MODE :Not enough parameters\r\n");
					return;
				}
				std::string limitStr = args[argIndex++];
				
				// Vérif numérique basique
				bool isNum = true;
				for (size_t j = 0; j < limitStr.size(); j++) {
					if (!isdigit(limitStr[j])) isNum = false;
				}
				
				if (isNum) {
					int limit = std::atoi(limitStr.c_str());
					channel->setUserLimit(limit);
					appliedModes += "+l";
					appliedArgs.push_back(limitStr);
				}
			} else {
				if (channel->getUserLimit() > 0) {
					channel->clearUserLimit();
					appliedModes += "-l";
				}
			}
		}
		else {
			sendResponse(fd, ":localhost 472 " + client->getNickname() + " " + c + " :is unknown mode char to me\r\n");
			// Pas de return ici ! On continue de traiter les autres lettres.
		}
	}

	// 4. BROADCAST FINAL (Seulement si des changements ont eu lieu)
	if (!appliedModes.empty()) {
		// Astuce : appliedModes contient des répétitions de signes (ex: "+i+t-o").
		// Les clients IRC le comprennent très bien, ou tu peux le nettoyer pour faire "+it-o".
		// Ton format actuel est valide.
		
		std::string msg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIPAdress() + " MODE " + target + " " + appliedModes;
		for (size_t i = 0; i < appliedArgs.size(); i++) {
			msg += " " + appliedArgs[i];
		}
		msg += "\r\n";
		channel->broadcast(msg, -1);
	}
}

Channel *Server::getChannel(const std::string &name) {
	std::map<std::string, Channel *>::iterator it = _channels.find(name);
	if (it == _channels.end()) {
		return NULL;
	}
	return it->second;
}

void	Server::cmd_join(int fd, const std::vector<std::string> &args) {
	Client *client = &_clients[fd];
	
	if (!client->isRegistered()) {
		sendResponse(fd, ":localhost 451 :You have not registered\r\n");
		return;
	}
	if (args.empty()) {
		sendResponse(fd, ":localhost 461 :Not enough parameters\r\n");
		return;
	}

	std::vector<std::string>	channelNames;
	std::vector<std::string>	channelKeys;
	std::stringstream			ss(args[0]);
	std::string					token;

	while (std::getline(ss, token, ',')) {
		if (!token.empty() && token.size() < 200 && 
			token.find(' ') == std::string::npos &&
			token.find(',') == std::string::npos &&
			token.find('\x07') == std::string::npos)
			channelNames.push_back(token);
		else
			sendResponse(fd, "localhost 403 " + client->getNickname() + ":No such channel\r\n");
	}

	if (args.size() > 1) {
		std::stringstream	ssKey(args[1]);
		std::string			tokenKey;
		while (std::getline(ssKey, tokenKey, ','))
			channelKeys.push_back(tokenKey);
	}
	for (size_t i = 0; i < channelNames.size(); i++) {
		std::string	actualChannel = channelNames[i];
		std::string actualKey;
		if (i < channelKeys.size())
			actualKey = channelKeys[i];
		else
			actualKey = "";
		if (actualChannel[0] != '#' && actualChannel[0] != '&') {
			sendResponse(fd, "localhost 403 " + client->getNickname() + actualChannel + ":No such channel\r\n");
			continue;
		}

		Channel *channel = getChannel(actualChannel);

		if (channel && channel->isMember(client)) {
			sendResponse(fd, ":localhost 443 " + client->getNickname() + " " +actualChannel + " :is already on channel\r\n");
			continue;
		}
		if (client->getNbChannels() > 10) {
			std::string err = ":localhost 405 " + client->getNickname() + " " + actualChannel + " :You have joined too many channels\r\n";
			sendResponse(fd, err);
			continue;
		}
		if (!channel) {
			std::cout << "DEBUG JOIN: Création de " << actualChannel << " avec Key=[" << actualKey << "]" << std::endl;
			channel = new Channel(actualChannel, "", client);
			_channels[actualChannel] = channel;
			client->addChannel(actualChannel);
		} else
		{
			if (channel->isInviteOnly() && !channel->isInvited(client->getNickname()))
			{
				sendResponse(fd, ":localhost 473 " + client->getNickname() + " " + actualChannel + " :Cannot join channel (+i)\r\n");
				continue;
			}

			if (!channel->getKey().empty() && channel->getKey() != actualKey) {
				sendResponse(fd, ":localhost 475 " + client->getNickname() + " " + actualChannel + " :Cannot join channel (+k)\r\n");
				continue;
			}
			if (channel->getUserLimit() > 0 && (int)channel->getClients().size() >= channel->getUserLimit())
			{
				sendResponse(fd, ":localhost 471 " + client->getNickname() + " " + actualChannel + " :Cannot join channel (+l)\r\n");
				continue;
			}
			channel->addClient(client);
			channel->revokeInvite(client->getNickname());
			client->addChannel(actualChannel);
		}
		std::string joinMsg = ":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getIPAdress() + " JOIN " + actualChannel + "\r\n";
		channel->broadcast(joinMsg, -1);

		if (!channel->getTopic().empty())
			sendResponse(fd, ":localhost 332 " + client->getNickname() + " " + actualChannel + " :" + channel->getTopic() + "\r\n");
		else
			sendResponse(fd, ":localhost 331 " + client->getNickname() + " " + actualChannel + " :No topic is set\r\n");
		std::string userList = "";
		std::vector<Client *> clientsInChan = channel->getClients(); 
		for (size_t j = 0; j < clientsInChan.size(); j++) {
			if (channel->isOperator(clientsInChan[j]))
				userList += "@";
			userList += clientsInChan[j]->getNickname();
			if (j < clientsInChan.size() - 1)
				userList += " ";
		}
		
		sendResponse(fd, ":localhost 353 " + client->getNickname() + " = " + actualChannel + " :" + userList + "\r\n");
		sendResponse(fd, ":localhost 366 " + client->getNickname() + " " + actualChannel + " :End of /NAMES list.\r\n");
	}
}

void	Server::cmd_prvmsg(int fd, const std::vector<std::string> &args) {
	Client *client = &_clients[fd];

	if (!client->isRegistered()) {
		sendResponse(fd, ":localhost 451 :You have not registered\r\n");
		return;
	}
	if (args.empty()) {
		sendResponse(fd, ":localhost 411 :No recipient given\r\n");
		return;
	}
	if (args.size() < 2) {
		sendResponse(fd, ":localhost 412 :No text to send\r\n");
		return;
	}

	std::string	clientList = args[0];
	std::string	message = args[1];
	if (message[0] == ':')
		message.erase(0, 1);
	for (size_t i = 2; i < args.size(); i++) {
		message += " " + args[i];
	}

	std::stringstream ss(clientList);
	std::string clientTarget;
	std::set<std::string> alreadySent;

	while (std::getline(ss, clientTarget, ',')) {
		if (alreadySent.count(clientTarget))
			continue;
		alreadySent.insert(clientTarget);
		if (clientTarget[0] == '#' || clientTarget[0] == '&') {
			Channel *chan = getChannel(clientTarget);

			if (!chan) {
				sendResponse(fd, ":localhost 403 " + client->getNickname() + " " + clientTarget + " :No such channel\r\n");
				continue;
			}
			if (!chan->isMember(client)) {
				sendResponse(fd, ":localhost 404 " + client->getNickname() + " " + clientTarget + " :Cannot send to channel\r\n");
				continue;
			}
			std::string fullMessage = ":" + client->getNickname() + "!" 
									+ client->getUsername() + "@" + client->getIPAdress() 
									+ " PRIVMSG " + clientTarget + " :" + message + "\r\n";
			chan->broadcast(fullMessage, fd);
		}
		else {
			Client	*recipient = getClientByNickname(clientTarget);
			if (recipient) {
				std::string fullMessage = ":" + client->getNickname() + "!" 
											+ client->getUsername() + "@" + client->getIPAdress() 
											+ " PRIVMSG " + clientTarget + " :" + message + "\r\n";
				sendResponse(recipient->getFd(), fullMessage);
			}
			else {
				std::string err = ":localhost 401 " + clientTarget + " :No such nick/channel\r\n";
				sendResponse(fd, err);
			}
		}
	}
}

void	Server::cmd_username(int fd, const std::vector<std::string> &args) {
	Client *client = &_clients[fd];
	
	if (!client->hasGivenPassword()) {
		sendResponse(fd, ":localhost 431 :No password given\r\n"); // Code un peu custom ou 431
		return;
	}
	if (client->isRegistered()) {
		sendResponse(fd, ":localhost 462 :You may not reregister\r\n");
		return;
	}
	if (args.size() < 4) {
		sendResponse(fd, ":localhost 461 USER :Not enough parameters\r\n");
		return;
	}

	std::string username = args[0];
	std::string realname = args[3];
	for (size_t i = 4; i < args.size(); i++) {
		realname += " " + args[i];
	}
	if (!realname.empty() && realname[0] == ':') {
		realname.erase(0, 1);
	}
	client->setUsername(username);
	client->setRealname(realname);
	if (!client->getNickname().empty() && !client->isRegistered()) {
		client->setIsRegistered(true);
		sendWelcome(fd);
	}
}


bool Server::isNickInUse(std::string const &nickname) {
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second.getNickname() == nickname)
			return true;
	}
	return false;
}

bool Server::isValidNickname(const std::string& nickname) {
	if (nickname.length() > 9) {
		return false;
	}

	std::string special = "[]\\`_^{}|-";
	for (size_t i = 0; i < nickname.length(); i++) {
		char c = nickname[i];
		if (!isalnum(c) && special.find(c) == std::string::npos) {
			return false;
		}
	}
	return true;
}

void	Server::cmd_nickname(int fd, const std::vector<std::string> &args) {
	Client *client = &_clients[fd];

	if (!client->hasGivenPassword()) {
		sendResponse(fd, ":localhost 431 :No password given\r\n"); // Code un peu custom ou 431
		return;
	}
	if (args.empty() || args[0].empty()) {
		sendResponse(fd, ":localhost 431 :No nickname given\r\n");
		return;
	}

	std::string newNickname = args[0];
	if (!isValidNickname(newNickname)) {
		std::string err = ":localhost 432 " + newNickname + " :Erroneous nickname\r\n";
		sendResponse(fd, err);
		return;
	}
	if (isNickInUse(newNickname)) {
		std::string err = ":localhost 433 * " + newNickname + " :Nickname is already in use\r\n";
		sendResponse(fd, err);
		return;
	}
	if (client->isRegistered()) {
		std::string oldNick = client->getNickname();
		client->setNickname(newNickname);
		std::string identity = oldNick + "!" + client->getUsername() + "@" + client->getIPAdress();
		std::string msg = ":" + identity + " NICK :" + newNickname + "\r\n";
		sendResponse(fd, msg);
		// TODO: Plus tard, il faudra envoyer ce message aux channels communs aussi
	} else {
		client->setNickname(newNickname);
		std::cout << "[INFO] Client " << fd << " set nickname to " << newNickname << std::endl;
	}

	if (!client->isRegistered() && !client->getUsername().empty()) {
		client->setIsRegistered(true);
		sendWelcome(fd);
	}
}

void	Server::cmd_password(int fd, const std::vector<std::string> &args) {
	Client *client = &_clients[fd];

	if (args.empty()) {
		sendResponse(fd, ":localhost 461 PASS: Not enough parameters\r\n");
		return;
	}
	if (client->isRegistered() || client->hasGivenPassword()) {
		sendResponse(fd, ":localhost 462 :You may not reregister\r\n");
		return;
	}
	std::string password_given = args[0];
	if (password_given == _password) {
		client->setHasGivenPassword(true);
		std::cout << "[SUCCESS] Client " << fd << " password accepted." << std::endl;
	} 
	else {
		client->setHasGivenPassword(false);
		sendResponse(fd, ":localhost 464 :Password incorrect\r\n");
		std::cout << "[FAIL] Client " << fd << " wrong password: " << password_given << std::endl;
	}
}

void Server::cmd_parsing(int fd, const std::string &cmd_line) {
	if (cmd_line.empty())
		return;
	std::vector<std::string> args = splitMessage(cmd_line);
	if (args.empty())
		return;

	// Put the command in uppercase (ex: "nick" -> "NICK")
	std::string cmdName = args[0];
	for (size_t i = 0; i < cmdName.length(); i++)
		cmdName[i] = std::toupper(cmdName[i]);

	// On retire le nom de la commande des arguments
	args.erase(args.begin());

	// 3. Exécution via la Map
	if (_cmds.find(cmdName) != _cmds.end()) {
		try {
			// Appel de la fonction membre pointée
			(this->*_cmds[cmdName])(fd, args);
		} catch (std::exception &e) {
			std::cerr << "Error exec : " << e.what() << std::endl;
		}
	} else {
		std::cerr << "Unknown command : " << cmdName << std::endl;
	}
}