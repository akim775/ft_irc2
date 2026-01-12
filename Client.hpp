/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/22 12:17:54 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/05 16:16:14 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Server.hpp"

class Client {
	private:
		int	_fd;
		bool		_hasGivenPassword;
		bool		_isRegistered;
		std::string	_nickname;
		std::string _realname;
		std::string	_username;
		std::string	_ip_address;
		std::string	_buffer;
		std::vector<std::string>	_joinedChannels;

	public:
		Client();
		Client(int fd, struct sockaddr_in addr);
		
		// Setters
		void	setHasGivenPassword(bool value);
		void	setIsRegistered(bool value);
		void	setNickname(std::string const &nickname);
		void	setUsername(std::string const &user);
		void	setRealname(std::string const &real);
		void	setFd(int fd);

		// Getters
		std::string	& getBuffer() { return _buffer; }
		std::string	getNickname() const;
		std::string	getIPAdress() const;
		std::string	getUsername() const;
		std::string	getRealname() const;
		bool		hasGivenPassword() const;
		bool		isRegistered() const;
		int			getFd() const;
		void		addChannel(const std::string& channelName);
		void		removeChannel(const std::string& channelName);
		size_t		getNbChannels() const;
		const std::vector<std::string>& getJoinedChannels() const;
};

#endif