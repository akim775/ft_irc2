/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/03 12:01:18 by ahamini           #+#    #+#             */
/*   Updated: 2026/01/12 09:05:46 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

volatile bool g_signal = true;

Server::Server(int port, std::string &password) : _port(port), _password(password), _server_fdsocket(-1), _epoll_fd(-1) {
	std::cout << BLUE << "Server constructor with parameters called" << NC << std::endl;
	(void)_port;
	_cmds["PASS"] = &Server::cmd_password;
	_cmds["NICK"] = &Server::cmd_nickname;
	_cmds["USER"] = &Server::cmd_username;
	_cmds["PRIVMSG"] = &Server::cmd_prvmsg;
	_cmds["JOIN"] = &Server::cmd_join;
	_cmds["INVITE"] = &Server::cmd_invite;
	_cmds["MODE"] = &Server::cmd_mode;
	_cmds["KICK"] = &Server::cmd_kick;
	_cmds["TOPIC"] = &Server::cmd_topic;
	_cmds["QUIT"] = &Server::cmd_quit;
	_cmds["PART"] = &Server::cmd_part;
}

Server::~Server() {
	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
		delete it->second;
	}
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		
		int fd_client = it->first;
		close(fd_client);
		std::cout << "Client " << fd_client << " disconnected." << std::endl;
	}
	if (_server_fdsocket != -1)
		close(_server_fdsocket);
	if (_epoll_fd != -1)
		close(_epoll_fd);
	std::cout << BLUE << "Server destructor called" << NC << std::endl;
}

void Server::onClientDisconnect(int fd) {
	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_clients.erase(fd);
	std::cout << GREEN << "Client " << fd << " disconnected properly." << NC << std::endl;
}

void	Server::initialize_socket() {

	struct sockaddr_in address_serv;
	std::memset(&address_serv, 0, sizeof(address_serv));
	address_serv.sin_family = AF_INET;
	address_serv.sin_addr.s_addr = INADDR_ANY;
	address_serv.sin_port = htons(_port);
	int	optval = 1;
	
	_server_fdsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fdsocket == -1)
		throw std::runtime_error(std::string("socket() function failed : ") + strerror(errno));
	if (setsockopt(_server_fdsocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
		throw std::runtime_error(std::string("setsockopt() function failed : ") + strerror(errno));
	if (fcntl(_server_fdsocket, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error(std::string("fcntl() function failed to put O_NONBLOCK option: ") + strerror(errno));
	if (bind(_server_fdsocket, (struct sockaddr *)&address_serv, sizeof(address_serv)) == -1)
		throw std::runtime_error(std::string("bind() function failed : ") + strerror(errno));
	if (listen(_server_fdsocket, SOMAXCONN) == -1)
		throw std::runtime_error(std::string("listen() function failed : ") + strerror(errno));
}

void	Server::setup_epoll() {

	_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (_epoll_fd == -1)
		throw std::runtime_error(std::string("epoll_create1() function failed : ") + strerror(errno));
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = _server_fdsocket;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server_fdsocket, &event) == -1)
		throw std::runtime_error(std::string("epoll_ctl() function failed for _server_fdsocket : ") + strerror(errno));
}

void	Server::add_fd_to_epoll(int client_fd) {
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
		std::cerr << RED << "epoll_ctl() function failed for client_fd : " << client_fd << strerror(errno) << NC << std::endl;
		close(client_fd);
	}
}

void	Server::start() {
	struct epoll_event events[MAX_EVENTS];
	while (g_signal) {
		int nbr_fd = epoll_wait(_epoll_fd, events, MAX_EVENTS, 500);
		if (nbr_fd == -1) {
			if (errno == EINTR)
				continue;
			throw std::runtime_error(std::string("epoll_wait failed : ") + strerror(errno));
		}
		for (int i = 0; i < nbr_fd; ++i) {
			if (!g_signal)
				break;
			if (events[i].data.fd == _server_fdsocket) {
				accept_new_client();
			}
			else {
				handle_client_data(events[i].data.fd);
			}
		}
	}
}

void	Server::accept_new_client() {
	struct sockaddr_in address_client;
	socklen_t client_len = sizeof(address_client);

	std::memset(&address_client, 0, sizeof(address_client));
	
	int client_fd = accept(_server_fdsocket, (struct sockaddr *)&address_client, &client_len);

	if (client_fd == - 1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			std::cerr << RED << "Error : accept() function failed : " << strerror(errno) << std::endl;
		return;
	}
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
		std::cerr << RED << "Error: fcntl() function failed for client fd " << client_fd << " : " << strerror(errno) << NC << std::endl;
		close(client_fd);
		return;
	}
	_clients.insert(std::make_pair(client_fd, Client(client_fd, address_client)));
	add_fd_to_epoll(client_fd);
}

void	Server::handle_client_data(int fd) {
	// Verify if the client exists
	if (_clients.find(fd) == _clients.end()) {
		std::cerr << RED << "Error : function called on an unknown fd." << NC << std::endl;
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
		return;
	}

	Client	*client = &_clients[fd];
	char	buffer[BUFFER_SIZE + 1];
	std::memset(buffer, 0, sizeof(buffer));

	ssize_t bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read <= 0) {
		if (bytes_read == 0) {
			onClientDisconnect(fd);
		}
		else {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				std::cerr << RED << "Error : function recv failed: " << strerror(errno) << NC << std::endl;
			}
		}
		return;
	}
	buffer[bytes_read] = '\0';
	std::string &filled_data = client->getBuffer();
	filled_data.append(buffer, bytes_read);
	size_t pos;
	while ((pos = filled_data.find('\n')) != std::string::npos) {
		
		std::string cmd_line = filled_data.substr(0, pos);
		filled_data.erase(0, pos + 1);
		if (!cmd_line.empty() && cmd_line[cmd_line.size() - 1] == '\r') {
			cmd_line.erase(cmd_line.size() - 1);
		}
		if (!cmd_line.empty() && cmd_line.size() <= 510) {
			std::cout << "[CLIENT " << fd << "] : " << cmd_line << std::endl;
			cmd_parsing(fd, cmd_line);
		}
		if (_clients.find(fd) == _clients.end()) {
			return;
		}
	}
}

