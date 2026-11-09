#include <QApplication>
#include <QDebug>

#include "main_window.h"

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

#include <cstdlib>

int main(int argc, char *argv[])
{
	if (ssh_threads_set_callbacks(ssh_threads_get_default()) != SSH_OK) {
		qCritical("Failed to configure libssh thread callbacks.");
		return EXIT_FAILURE;
	}

	if (ssh_init() != SSH_OK) {
		qCritical("Failed to initialize libssh.");
		return EXIT_FAILURE;
	}

	int exit_code = EXIT_SUCCESS;
	{
		QApplication app(argc, argv);

		main_window window;
		window.show();

		exit_code = app.exec();
	}

	if (ssh_finalize() != SSH_OK)
		qCritical("Failed to finalize libssh.");

	return exit_code;
}