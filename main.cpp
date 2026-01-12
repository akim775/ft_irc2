/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ahamini <ahamini@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/12/02 12:05:44 by ahamini           #+#    #+#             */
/*   Updated: 2025/12/16 11:05:21 by ahamini          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include "Server.hpp"

# define GREEN "\033[0;32m"
# define YELLOW "\033[0;33m"
# define RED "\033[1;31m"
# define BLUE "\033[0;36m"
# define NC "\033[0m"
# define CYAN "\033[0;35m"
# define FLASH "\033[1;93m"
# define GREEN_FLASH "\033[38;5;46m"

static void	signalHandler(int sigNumber) {
	std::cout << std::endl;
	if (sigNumber == 2)
		std::cerr << RED << "Server shut down with ctrl+c" << NC << std::endl;
	if (sigNumber == 15)
		std::cerr << RED << "Server got killed" << NC << std::endl;
	if (sigNumber == 3)
		std::cerr << RED << "Server shut down with ctrl+\\" << NC << std::endl;
	g_signal = false;
}

bool	hasWhiteSpace(std::string& str) {
	for (size_t i = 0; i < str.size(); i++) {
		if (std::isspace(str[i]))
			return (true);
	}
	return (false);
}

int	main(int ac, char **av) {
	if (ac != 3) {
		std::cerr << RED << "Usage: " << av[0] << " <port number> <password>" << NC << std::endl;
		return (1);
	}

	std::string password = av[2];
	if (password.empty()) {
		std::cerr << RED << "Error: Password can't be empty !" << NC << std::endl;
		return (1);
	}
	if (hasWhiteSpace(password)) {
		std::cerr << RED << "Error: Password can't contain spaces !" << NC << std::endl;
		return (1);
	}

	char	*end;
	long	port_l = std::strtol(av[1], &end, 10);
	if (*end != '\0' || port_l < 1 || port_l > 65535) {
		std::cerr << RED << "Error: Invalid port number ! Please select a port between 1 and 65535." << NC << std::endl;
		return (1);
	}

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGPIPE, SIG_IGN);

	int	port = static_cast<int>(port_l);

	try {
		Server server(port, password);
		server.initialize_socket();
		server.setup_epoll();
		std::cout << YELLOW << "Server started successfully !" << NC << std::endl;
		server.start();

	} catch (const std::runtime_error &e) {
		std::cerr << RED << "Fatal error: " << e.what() << NC << std::endl;
		return (1);
	}
	std::cout << std::endl;
	std::cout << GREEN << "You have been successfully disconnected from your server !" << NC << std::endl;
	return (0);
}
