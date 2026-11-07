#include "ssh_client.h"

#include <QByteArray>

#include <limits>

ssh_client::~ssh_client()
{
	disconnect();
}

bool ssh_client::connect_to_server(const QString &host, int port,
				   const QString &username)
{
	disconnect();
	error_message.clear();

	if (host.trimmed().isEmpty() || username.trimmed().isEmpty() ||
	    port < 1 || port > 65535) {
		error_message = "Invalid SSH connection parameters.";
		return false;
	}

	session = ssh_new();

	if (!session) {
		error_message = "Failed to create SSH session.";
		return false;
	}

	QByteArray host_bytes = host.toUtf8();
	QByteArray user_bytes = username.toUtf8();

	if (ssh_options_set(session, SSH_OPTIONS_HOST,
			    host_bytes.constData()) != SSH_OK ||
	    ssh_options_set(session, SSH_OPTIONS_PORT, &port) != SSH_OK ||
	    ssh_options_set(session, SSH_OPTIONS_USER,
			    user_bytes.constData()) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		disconnect();
		return false;
	}

	long timeout = 10;

	if (ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		disconnect();
		return false;
	}

	if (ssh_connect(session) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		disconnect();
		return false;
	}

	return true;
}

ssh_client::host_key_status ssh_client::check_host_key()
{
	if (!is_connected()) {
		error_message = "SSH session is not connected.";
		return host_key_status::error;
	}

	switch (ssh_session_is_known_server(session)) {
	case SSH_KNOWN_HOSTS_OK:
		return host_key_status::trusted;

	case SSH_KNOWN_HOSTS_NOT_FOUND:
	case SSH_KNOWN_HOSTS_UNKNOWN:
		return host_key_status::unknown;

	case SSH_KNOWN_HOSTS_CHANGED:
		error_message = "WARNING: The server host key has changed.";
		return host_key_status::changed;

	case SSH_KNOWN_HOSTS_OTHER:
		error_message = "A different host key type is already known.";
		return host_key_status::other;

	case SSH_KNOWN_HOSTS_ERROR:
	default:
		error_message = QString::fromUtf8(ssh_get_error(session));
		return host_key_status::error;
	}
}

QString ssh_client::host_key_fingerprint() const
{
	if (!is_connected())
		return {};

	ssh_key key = nullptr;

	if (ssh_get_server_publickey(session, &key) != SSH_OK)
		return {};

	unsigned char *hash = nullptr;
	size_t hash_length = 0;

	int result = ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256,
					    &hash, &hash_length);

	ssh_key_free(key);

	if (result != SSH_OK)
		return {};

	QByteArray bytes(reinterpret_cast<const char *>(hash),
			 static_cast<qsizetype>(hash_length));

	ssh_clean_pubkey_hash(&hash);

	return "SHA256:" + QString::fromLatin1(
			       bytes.toBase64(QByteArray::OmitTrailingEquals));
}

bool ssh_client::trust_host_key()
{
	if (!is_connected()) {
		error_message = "SSH session is not connected.";
		return false;
	}

	if (check_host_key() != host_key_status::unknown) {
		error_message =
		    "Host key cannot be added in the current state.";
		return false;
	}

	if (ssh_session_update_known_hosts(session) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		return false;
	}

	return true;
}

bool ssh_client::authenticate_password(const QString &password)
{
	authenticated = false;

	if (!is_connected()) {
		error_message = "SSH session is not connected.";
		return false;
	}

	if (check_host_key() != host_key_status::trusted) {
		error_message = "Server host key is not trusted.";
		return false;
	}

	QByteArray pass_bytes = password.toUtf8();

	int result =
	    ssh_userauth_password(session, nullptr, pass_bytes.constData());

	pass_bytes.fill('\0');

	if (result != SSH_AUTH_SUCCESS) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		return false;
	}

	authenticated = true;
	error_message.clear();
	return true;
}

void ssh_client::disconnect()
{
	authenticated = false;

	close_shell();

	if (!session)
		return;

	if (ssh_is_connected(session))
		ssh_disconnect(session);

	ssh_free(session);
	session = nullptr;
}

bool ssh_client::is_connected() const
{
	return session && ssh_is_connected(session);
}

bool ssh_client::is_authenticated() const
{
	return authenticated && is_connected();
}

QString ssh_client::last_error() const
{
	return error_message;
}

bool ssh_client::open_shell(int columns, int rows)
{
	if (!is_authenticated()) {
		error_message = "SSH session is not authenticated.";
		return false;
	}

	if (columns <= 0 || rows <= 0) {
		error_message = "Invalid terminal dimensions.";
		return false;
	}

	close_shell();

	shell_channel = ssh_channel_new(session);

	if (!shell_channel) {
		error_message = "Failed to create SSH channel.";
		return false;
	}

	if (ssh_channel_open_session(shell_channel) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		close_shell();
		return false;
	}

	if (ssh_channel_request_pty_size(shell_channel, "xterm-256color",
					 columns, rows) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		close_shell();
		return false;
	}

	if (ssh_channel_request_shell(shell_channel) != SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		close_shell();
		return false;
	}

	error_message.clear();
	return true;
}

bool ssh_client::write_shell(const QByteArray &data)
{
	if (!is_shell_open()) {
		error_message = "SSH shell is not open.";
		return false;
	}

	qsizetype offset = 0;

	while (offset < data.size()) {
		int written = ssh_channel_write(
		    shell_channel, data.constData() + offset,
		    static_cast<uint32_t>(data.size() - offset));

		if (written <= 0) {
			error_message =
			    QString::fromUtf8(ssh_get_error(session));
			return false;
		}

		offset += written;
	}

	return true;
}

int ssh_client::read_shell(QByteArray &data)
{
	data.clear();

	if (!is_shell_open()) {
		error_message = "SSH shell is not open.";
		return -1;
	}

	char buffer[4096];

	int received = ssh_channel_read_nonblocking(shell_channel, buffer,
						    sizeof(buffer), 0);

	if (received == SSH_ERROR) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		return -1;
	}

	if (received > 0)
		data.append(buffer, received);

	return received;
}

bool ssh_client::resize_shell(int columns, int rows)
{
	if (!is_shell_open()) {
		error_message = "SSH shell is not open.";
		return false;
	}

	if (columns <= 0 || rows <= 0) {
		error_message = "Invalid terminal dimensions.";
		return false;
	}

	if (ssh_channel_change_pty_size(shell_channel, columns, rows) !=
	    SSH_OK) {
		error_message = QString::fromUtf8(ssh_get_error(session));
		return false;
	}

	return true;
}

void ssh_client::close_shell()
{
	if (!shell_channel)
		return;

	if (ssh_channel_is_open(shell_channel))
		ssh_channel_close(shell_channel);

	ssh_channel_free(shell_channel);
	shell_channel = nullptr;
}

bool ssh_client::is_shell_open() const
{
	return shell_channel && ssh_channel_is_open(shell_channel) &&
	       !ssh_channel_is_closed(shell_channel);
}
