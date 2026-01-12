/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/30 17:03:31 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 08:52:09 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel() : _name(""), _key(""), _inviteOnly(false), _topicRestricted(false), _userLimit(0) {
	std::cout << GREEN << "Default Channel constructor called" << NC << std::endl;
}

Channel::Channel(std::string name, std::string key, Client *admin) : _name(name), _key(key), _inviteOnly(false), _topicRestricted(false), _userLimit(0) {
	std::cout << GREEN << "Channel constructor with parameters called" << NC << std::endl;
	this->addClient(admin);
	this->addOperator(admin);
}

Channel::~Channel() {
	_clients.clear();
	_operators.clear();
}

void	Channel::addClient(Client *newClient) {
	// Add client to the client list and check if he is already in the list
	for (size_t i = 0; i < _clients.size(); i++) {
		if (_clients[i]->getFd() == newClient->getFd())
			return;
	}
	_clients.push_back(newClient);
}

void	Channel::addOperator(Client *newOperator) {
	// Add client to the operator list and check if he is already in the list
	for (size_t i = 0; i < _operators.size(); i++) {
		if (_operators[i]->getFd() == newOperator->getFd())
			return;
	}
	_operators.push_back(newOperator);
}

void Channel::removeClient(int fd) {

	// Check if client is in the client list and remove it 
	for (std::vector<Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if ((*it)->getFd() == fd) {
			_clients.erase(it);
			break;
		}
	}
	//Check if client is in the operator list and remove it 
	for (std::vector<Client *>::iterator it = _operators.begin(); it != _operators.end(); ++it) {
		if ((*it)->getFd() == fd) {
			_operators.erase(it);
			break;
		}
	}
}

void	Channel::broadcast(const std::string &message, int excludeFd) {
	for (size_t i = 0; i < _clients.size(); i++) {
		Client	*client = _clients[i];

		if (client->getFd() != excludeFd) {
			send(client->getFd(), message.c_str(), message.size(), 0);
		}
	}
}

bool Channel::isMember(Client *client) {
	for (size_t i = 0; i < _clients.size(); i++) {
		if (_clients[i]->getFd() == client->getFd())
			return true;
	}
	
	return false;
}

std::string	Channel::getKey() {
	return _key;
}

void	Channel::setKey(std::string key) {
	_key = key;
}

std::string Channel::getTopic() const {
	return this->_topic;
}

std::vector<Client *> Channel::getClients() const {
	return this->_clients;
}

bool Channel::isOperator(Client *client) const {
	for (size_t i = 0; i < _operators.size(); i++) {
		if (_operators[i] == client) {
			return true;
		}
	}
	return false;
}

void Channel::sendInvite(const std::string &nickname) {
	if (!nickname.empty())
		_invited.insert(nickname);
}

bool Channel::isInvited(const std::string &nickname) const {
	return (_invited.find(nickname) != _invited.end());
}

void Channel::revokeInvite(const std::string &nickname) {
	std::set<std::string>::iterator it = _invited.find(nickname);
	if (it != _invited.end())
		_invited.erase(it);
}

bool Channel::isInviteOnly() const
{
	return (_inviteOnly);
}

void Channel::setInviteOnly(bool value)
{
	_inviteOnly = value;
}

void	Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

bool Channel::isTopicRestricted() const
{
	return _topicRestricted;
}

void Channel::setTopicRestricted(bool value) {
	_topicRestricted = value;
}

int Channel::getUserLimit() const {
	return _userLimit;
}

void Channel::setUserLimit(int limit) {
	_userLimit = limit;
}

void Channel::clearUserLimit() {
	_userLimit = 0;
}

void Channel::removeOperator(int fd) {
	for (std::vector<Client *>::iterator it = _operators.begin(); it != _operators.end(); ++it)
	{
		if ((*it)->getFd() == fd)
		{
			_operators.erase(it);
			return ;
		}
	}
}

size_t Channel::getClientCount() const {
	return _clients.size();
}
