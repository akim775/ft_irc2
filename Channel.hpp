/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/30 17:03:28 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 08:51:26 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Server.hpp"
#include "Client.hpp"

class Channel
{
private:
	std::string _name;
	std::string _topic;
	std::string _key;
	std::vector<Client *> _clients;
	std::vector<Client *> _operators;
	std::set<std::string> _invited;
	bool		_inviteOnly;
	bool		_topicRestricted;
	int			_userLimit;

public:
	Channel();
	Channel(std::string name, std::string key, Client *admin);
	~Channel();

	// Getters
	std::string	getName();
	std::string	getKey();
	std::string	getTopic() const;
    std::vector<Client *>	getClients() const;
	bool	isMember(Client *client);
	bool	isOperator(Client *client) const;
	bool	isInviteOnly() const;
	bool	isInvited(const std::string &nickname) const;
	bool	isTopicRestricted() const;
	int		getUserLimit() const;
	size_t	getClientCount() const;
	
	// Setters
	void	setKey(std::string key);
	void	setInviteOnly(bool value);
	void	setTopicRestricted(bool value);
	void	setUserLimit(int limit);
	void	setTopic(const std::string &topic);
	
	void	addClient(Client *newClient);
	void	addOperator(Client *newAdmin);
	void	removeClient(int fd);
	void	removeOperator(int fd);
	void	revokeInvite(const std::string &nickname);
	void	broadcast(const std::string &message, int excludeFd = -1);
	void	sendInvite(const std::string &nickname);
	void	clearUserLimit();
};

#endif