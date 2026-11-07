#include "main_window.h"
#include "ssh_connection.h"
#include "ssh_worker.h"
#include "terminal_widget.h"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

main_window::main_window(QWidget *parent) : QMainWindow(parent)
{
	servers = repository.load();

	setup_ui();
	setup_style();
	refresh_servers();
}

void main_window::setup_ui()
{
	setWindowTitle("Himashinya");
	resize(1200, 760);
	setMinimumSize(800, 500);

	auto *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setChildrenCollapsible(false);

	/* Left sidebar */
	sidebar_tabs = new QTabWidget(splitter);

	auto *connections_page = new QWidget;
	auto *connections_layout = new QVBoxLayout(connections_page);

	auto *add_server = new QPushButton("+ Add Server");

	server_list = new QListWidget;
	server_list->setContextMenuPolicy(Qt::CustomContextMenu);

	connections_layout->addWidget(add_server);
	connections_layout->addWidget(server_list);

	sidebar_tabs->addTab(connections_page, "Connections");

	/* File browser */
	auto *files_page = new QWidget;
	auto *files_layout = new QVBoxLayout(files_page);

	auto *files_label = new QLabel("Connect to a server to browse files.");

	files_label->setWordWrap(true);
	files_layout->addWidget(files_label);
	files_layout->addStretch();

	sidebar_tabs->addTab(files_page, "Files");

	/* Terminal area */
	terminal_tabs = new QTabWidget(splitter);
	terminal_tabs->setTabsClosable(true);
	terminal_tabs->setMovable(true);

	auto *welcome_page = new QWidget;
	auto *welcome_layout = new QVBoxLayout(welcome_page);

	auto *welcome_label = new QLabel("Welcome to Himashinya\n\n"
					 "Select a connection to get started.");

	welcome_label->setAlignment(Qt::AlignCenter);
	welcome_layout->addWidget(welcome_label);

	terminal_tabs->addTab(welcome_page, "Welcome");

	connect(terminal_tabs, &QTabWidget::tabCloseRequested, this,
		&main_window::close_terminal_tab);

	/* Add server dialog */
	connect(add_server, &QPushButton::clicked, this,
		&main_window::add_server);

	connect(server_list, &QListWidget::itemDoubleClicked, this,
		[this](QListWidgetItem *) { connect_server(); });

	connect(server_list, &QWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) {
			auto *item = server_list->itemAt(pos);

			if (!item)
				return;

			server_list->setCurrentItem(item);

			QMenu menu(this);

			auto *connect_action = menu.addAction("Connect");
			menu.addSeparator();

			auto *edit = menu.addAction("Edit");
			auto *remove = menu.addAction("Delete");

			auto *selected = menu.exec(
			    server_list->viewport()->mapToGlobal(pos));

			if (selected == connect_action)
				connect_server();
			else if (selected == edit)
				edit_server();
			else if (selected == remove)
				delete_server();
		});

	splitter->addWidget(sidebar_tabs);
	splitter->addWidget(terminal_tabs);

	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({280, 920});

	setCentralWidget(splitter);
}

void main_window::setup_style()
{
	setStyleSheet(R"(
		QMainWindow, QWidget {
			background-color: #1e1e1e;
			color: #d4d4d4;
			font-family: "Segoe UI";
			font-size: 13px;
		}

		QTabWidget::pane {
			border: none;
			background: #1e1e1e;
		}

		QTabBar::tab {
			background: #252526;
			color: #969696;
			padding: 10px 18px;
			border-right: 1px solid #333333;
		}

		QTabBar::tab:selected {
			background: #1e1e1e;
			color: #ffffff;
			border-top: 2px solid #007acc;
		}

		QListWidget {
			background: #252526;
			border: none;
			outline: none;
		}

		QListWidget::item {
			padding: 9px;
		}

		QListWidget::item:selected {
			background: #37373d;
		}

		QPushButton {
			background: #0e639c;
			color: white;
			border: none;
			border-radius: 4px;
			padding: 9px;
		}

		QPushButton:hover {
			background: #1177bb;
		}

		QLineEdit, QSpinBox {
			background: #303030;
			color: #ffffff;
			border: 1px solid #555555;
			padding: 5px;
		}

		QSplitter::handle {
			background: #333333;
		}
	)");
}

static bool edit_profile_dialog(QWidget *parent, server_profile &server)
{
	QDialog dialog(parent);
	dialog.setWindowTitle(server.id.isEmpty() ? "Add Server"
						  : "Edit Server");

	QFormLayout layout(&dialog);

	QLineEdit name(server.name);
	QLineEdit host(server.host);
	QLineEdit username(server.username);
	QSpinBox port;

	port.setRange(1, 65535);
	port.setValue(server.port);

	layout.addRow("Name", &name);
	layout.addRow("Host", &host);
	layout.addRow("Port", &port);
	layout.addRow("Username", &username);

	QDialogButtonBox buttons(QDialogButtonBox::Ok |
				 QDialogButtonBox::Cancel);

	layout.addRow(&buttons);

	QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
		if (name.text().trimmed().isEmpty() ||
		    host.text().trimmed().isEmpty() ||
		    username.text().trimmed().isEmpty()) {
			QMessageBox::warning(
			    &dialog, "Invalid Server",
			    "Please fill in all required fields.");
			return;
		}

		dialog.accept();
	});

	QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog,
			 &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted)
		return false;

	server.name = name.text().trimmed();
	server.host = host.text().trimmed();
	server.port = static_cast<quint16>(port.value());
	server.username = username.text().trimmed();

	return true;
}

void main_window::refresh_servers()
{
	server_list->clear();

	for (const auto &server : servers) {
		auto *item = new QListWidgetItem(server.name);
		item->setToolTip(QString("%1@%2:%3")
				     .arg(server.username)
				     .arg(server.host)
				     .arg(server.port));

		server_list->addItem(item);
	}
}

void main_window::add_server()
{
	server_profile server;

	if (!edit_profile_dialog(this, server))
		return;

	server.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

	servers.append(server);
	repository.save(servers);
	refresh_servers();
}

void main_window::edit_server()
{
	int index = server_list->currentRow();

	if (index < 0 || index >= servers.size())
		return;

	server_profile updated = servers[index];

	if (!edit_profile_dialog(this, updated))
		return;

	servers[index] = updated;
	repository.save(servers);
	refresh_servers();
}

void main_window::delete_server()
{
	int index = server_list->currentRow();

	if (index < 0 || index >= servers.size())
		return;

	auto answer = QMessageBox::question(this, "Delete Server",
					    "Delete the selected server?",
					    QMessageBox::Yes | QMessageBox::No);

	if (answer != QMessageBox::Yes)
		return;

	servers.removeAt(index);
	repository.save(servers);
	refresh_servers();
}

void main_window::connect_server()
{
	int index = server_list->currentRow();

	if (index < 0 || index >= servers.size())
		return;

	const auto server = servers[index];
	auto *connection = new ssh_connection(server, this);

	connect(connection, &ssh_connection::finished, connection,
		&QObject::deleteLater);

	connect(connection, &ssh_connection::host_key_unknown, this,
		[this, connection, server](const QString &fingerprint) {
			auto answer = QMessageBox::question(
			    this, "Unknown SSH Host Key",
			    QString("Server: %1\n\n"
				    "Fingerprint:\n%2\n\n"
				    "Trust this server?")
				.arg(server.name, fingerprint),
			    QMessageBox::Yes | QMessageBox::No);

			if (answer == QMessageBox::Yes)
				connection->trust_host_key();
			else
				connection->close();
		});

	connect(connection, &ssh_connection::host_key_verified, this,
		[this, connection]() {
			bool ok = false;

			QString password = QInputDialog::getText(
			    this, "SSH Authentication",
			    "Password:", QLineEdit::Password, {}, &ok);

			if (!ok) {
				connection->close();
				return;
			}

			connection->authenticate(password);
		});

	connect(connection, &ssh_connection::authenticated, this,
		[this, connection, server]() {
			auto *terminal = new terminal_widget;

			terminal->setProperty(
			    "ssh_connection",
			    QVariant::fromValue(
				static_cast<QObject *>(connection)));

			connect(connection, &ssh_connection::shell_output,
				terminal, &terminal_widget::feed_output);

			connect(terminal, &terminal_widget::send_data,
				connection, &ssh_connection::write_shell);

			connect(terminal, &terminal_widget::resize_requested,
				connection, &ssh_connection::resize_shell);

			connect(connection, &ssh_connection::shell_ready,
				terminal,
				[terminal]() { terminal->setFocus(); });

			int tab_index =
			    terminal_tabs->addTab(terminal, server.name);
			terminal_tabs->setCurrentIndex(tab_index);
		});

	connect(connection, &ssh_connection::failed, this,
		[this, connection](const QString &message) {
			connection->close();

			QMessageBox::warning(this, "SSH Connection Failed",
					     message);
		});

	connection->start();
}

void main_window::close_terminal_tab(int index)
{
	QWidget *page = terminal_tabs->widget(index);

	if (!page)
		return;

	auto *connection = qobject_cast<ssh_connection *>(
	    page->property("ssh_connection").value<QObject *>());

	terminal_tabs->removeTab(index);

	if (connection)
		connection->close();

	page->deleteLater();
}
