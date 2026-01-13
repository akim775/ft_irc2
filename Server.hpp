/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 14:53:30 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 16:31:37 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <set>
#include "Client.hpp"
#include "Channel.hpp"
#include "colors.hpp"

#define MAX_EVENTS 128
#define BUFFER_SIZE 512
#define ERR_NEEDMOREPARAMS 461
#define ERR_ALREADYREGISTERED 462
#define ERR_PASSWDMISMATCH 464

class Channel;

class Server {
	private:
		int			_port;
		int			_server_fdsocket;
		int			_epoll_fd;
		std::string	_password;
		std::string	_serverName;
		std::map<int, Client> _clients;
		void	cmd_parsing(int fd, const std::string &command_line);
		void	cmd_password(int fd, const std::vector<std::string> &args);
		void	cmd_nickname(int fd, const std::vector<std::string> &args);
		void	cmd_username(int fd, const std::vector<std::string> &args);
		void	cmd_prvmsg(int fd, const std::vector<std::string> &args);
		void	cmd_join(int fd, const std::vector<std::string> &args);
		void	cmd_invite(int fd, const std::vector<std::string> &args);
		void	cmd_mode(int fd, const std::vector<std::string> &args);
		void	cmd_topic(int fd, const std::vector<std::string> &args);
		void	cmd_kick(int fd, const std::vector<std::string> &args);
		void	cmd_quit(int fd, const std::vector<std::string> &args);
		void	cmd_part(int fd, const std::vector<std::string> &args);
		void	cmd_ping(int fd, const std::vector<std::string> &args);

		Client *getClientByNickname(const std::string &nickname);
		
		std::map<std::string, void (Server::*)(int, const std::vector<std::string>&)> _cmds;
		std::map<std::string, Channel *> _channels;
		std::vector<std::string> splitMessage(std::string input);


	public:
		Server(int port, std::string &password);
		~Server();

		void	initialize_socket();
		void	setup_epoll();
		void	add_fd_to_epoll(int fd);
		void	start();
		private:
		void	accept_new_client();
		void	handle_client_data(int fd);
		void	onClientDisconnect(int fd);
		void	sendResponse(int fd, std::string response);
		bool	isNickInUse(std::string const &nick);
		bool	isValidNickname(const std::string& nick);
		void	sendWelcome(int fd);

		// Getters

		Channel	*getChannel(const std::string &name);
		std::string getServerName() const;
	};

extern volatile bool g_signal;

#endif