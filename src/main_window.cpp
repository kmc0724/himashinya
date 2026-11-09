#include "main_window.h"
#include "ssh_connection.h"
#include "ssh_worker.h"
#include "terminal_widget.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <QtMath>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

main_window::main_window(QWidget *parent) : QMainWindow(parent)
{
	servers = repository.load();

	terminal_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	terminal_font.setPointSize(11);
	terminal_font.setStyleHint(QFont::Monospace);
	terminal_font.setFixedPitch(true);

	QSettings settings("Himashinya", "Himashinya");
	terminal_font.setFamily(settings.value("terminal/font_family",
					      terminal_font.family())
					.toString());
	terminal_font.setPointSize(
		qBound(6, settings.value("terminal/font_size", 11).toInt(), 72));

	setup_ui();
	setup_menu();
	setup_style();
	refresh_servers();
}

main_window::~main_window()
{
	const auto connections = findChildren<ssh_connection *>();
	for (auto *connection : connections)
		connection->shutdown();
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

	sftp_connections = new QComboBox;
	sftp_connections->addItem("Connect to a server first");
	sftp_connections->setEnabled(false);
	files_layout->addWidget(sftp_connections);

	auto *path_layout = new QHBoxLayout;
	sftp_path = new QLineEdit;
	sftp_path->setPlaceholderText("Remote directory");
	sftp_go_button = new QPushButton("Go");
	sftp_refresh_button = new QPushButton("Refresh");
	path_layout->addWidget(sftp_path, 1);
	path_layout->addWidget(sftp_go_button);
	path_layout->addWidget(sftp_refresh_button);
	files_layout->addLayout(path_layout);

	sftp_file_list = new QListWidget;
	files_layout->addWidget(sftp_file_list, 1);

	auto *transfer_layout = new QHBoxLayout;
	sftp_upload_button = new QPushButton("Upload");
	sftp_download_button = new QPushButton("Download");
	transfer_layout->addWidget(sftp_upload_button);
	transfer_layout->addWidget(sftp_download_button);
	files_layout->addLayout(transfer_layout);

	sftp_progress = new QProgressBar;
	sftp_progress->setRange(0, 100);
	sftp_progress->setValue(0);
	sftp_progress->setTextVisible(false);
	files_layout->addWidget(sftp_progress);

	sftp_status = new QLabel("Connect to a server to browse files.");
	sftp_status->setWordWrap(true);
	files_layout->addWidget(sftp_status);

	sidebar_tabs->addTab(files_page, "Files");

	connect(sftp_connections,
		qOverload<int>(&QComboBox::currentIndexChanged), this,
		[this](int) {
			const int index = sftp_connections->currentIndex();
			if (index < 0)
				return;
			const QString path =
			    sftp_connections
				->itemData(index, Qt::UserRole + 1)
				.toString();
			sftp_path->setText(path);
			update_sftp_controls();
			refresh_sftp_directory();
		});
	connect(sftp_go_button, &QPushButton::clicked, this, [this]() {
		navigate_sftp_directory(sftp_path->text());
	});
	connect(sftp_path, &QLineEdit::returnPressed, this, [this]() {
		navigate_sftp_directory(sftp_path->text());
	});
	connect(sftp_refresh_button, &QPushButton::clicked, this,
		&main_window::refresh_sftp_directory);
	connect(sftp_file_list, &QListWidget::currentItemChanged, this,
		[this]() { update_sftp_controls(); });
	connect(sftp_file_list, &QListWidget::itemDoubleClicked, this,
		&main_window::open_sftp_item);
	connect(sftp_upload_button, &QPushButton::clicked, this,
		&main_window::upload_sftp_file);
	connect(sftp_download_button, &QPushButton::clicked, this,
		&main_window::download_sftp_file);

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
	update_sftp_controls();
}

bool main_window::nativeEvent(const QByteArray &event_type, void *message,
			      qintptr *result)
{
#ifdef Q_OS_WIN
	Q_UNUSED(event_type);
	auto *native_message = static_cast<MSG *>(message);
	if (native_message->message == WM_SIZING && terminal_tabs) {
		auto *terminal = qobject_cast<terminal_widget *>(
		    terminal_tabs->currentWidget());
		if (terminal) {
			const QSizeF cell = terminal->cell_size();
			const qreal device_pixel_ratio =
			    terminal->devicePixelRatioF();
			if (cell.width() > 0 && cell.height() > 0 &&
			    device_pixel_ratio > 0) {
				const qreal cell_width =
				    cell.width() * device_pixel_ratio;
				const qreal cell_height =
				    cell.height() * device_pixel_ratio;
				const qreal terminal_width =
				    terminal->width() * device_pixel_ratio;
				const qreal terminal_height =
				    terminal->height() * device_pixel_ratio;
				auto *rect = reinterpret_cast<RECT *>(
				    native_message->lParam);
				const LONG proposed_outer_width =
				    rect->right - rect->left;
				const LONG proposed_outer_height =
				    rect->bottom - rect->top;
				RECT actual_rect{};
				if (!GetWindowRect(native_message->hwnd,
						   &actual_rect)) {
					qWarning("GetWindowRect failed while snapping "
						 "terminal resize: %lu",
						 GetLastError());
					return QMainWindow::nativeEvent(
					    event_type, message, result);
				}

				const UINT edge =
				    static_cast<UINT>(native_message->wParam);
				const bool resize_left =
				    edge == WMSZ_LEFT ||
				    edge == WMSZ_TOPLEFT ||
				    edge == WMSZ_BOTTOMLEFT;
				const bool resize_right =
				    edge == WMSZ_RIGHT ||
				    edge == WMSZ_TOPRIGHT ||
				    edge == WMSZ_BOTTOMRIGHT;
				const bool resize_top =
				    edge == WMSZ_TOP ||
				    edge == WMSZ_TOPLEFT ||
				    edge == WMSZ_TOPRIGHT;
				const bool resize_bottom =
				    edge == WMSZ_BOTTOM ||
				    edge == WMSZ_BOTTOMLEFT ||
				    edge == WMSZ_BOTTOMRIGHT;

				if (resize_left || resize_right) {
					const int current_window_width =
					    actual_rect.right -
					    actual_rect.left;
					const qreal proposed_terminal_width =
					    terminal_width +
					    (proposed_outer_width -
					     current_window_width);
					const int minimum_cells = qMax(
					    1, qCeil((terminal_width +
						      (minimumWidth() -
						       width()) *
							  device_pixel_ratio) /
						     cell_width));
					const int cells = qMax(
					    minimum_cells,
					    qRound(proposed_terminal_width /
						   cell_width));
					const int target_width =
					    current_window_width +
					    qRound(cells * cell_width -
						   terminal_width);
					if (resize_left)
						rect->left =
						    rect->right - target_width;
					else
						rect->right =
						    rect->left + target_width;
				}

				if (resize_top || resize_bottom) {
					const int current_window_height =
					    actual_rect.bottom -
					    actual_rect.top;
					const qreal proposed_terminal_height =
					    terminal_height +
					    (proposed_outer_height -
					     current_window_height);
					const int minimum_cells = qMax(
					    1, qCeil((terminal_height +
						      (minimumHeight() -
						       height()) *
							  device_pixel_ratio) /
						     cell_height));
					const int cells = qMax(
					    minimum_cells,
					    qRound(proposed_terminal_height /
						   cell_height));
					const int target_height =
					    current_window_height +
					    qRound(cells * cell_height -
						   terminal_height);
					if (resize_top)
						rect->top =
						    rect->bottom - target_height;
					else
						rect->bottom =
						    rect->top + target_height;
				}

				*result = TRUE;
				return true;
			}
		}
	}
#else
	Q_UNUSED(event_type);
	Q_UNUSED(message);
	Q_UNUSED(result);
#endif
	return QMainWindow::nativeEvent(event_type, message, result);
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

void main_window::setup_menu()
{
	QMenu *settings_menu = menuBar()->addMenu("Settings");
	QAction *font_action = settings_menu->addAction("Terminal Font...");

	connect(font_action, &QAction::triggered, this,
		&main_window::show_terminal_font_dialog);
}

void main_window::show_terminal_font_dialog()
{
	QFontDialog dialog(terminal_font, this);
	dialog.setWindowTitle("Terminal Font");
	dialog.setOption(QFontDialog::DontUseNativeDialog);
	dialog.setOption(QFontDialog::MonospacedFonts);

	if (dialog.exec() != QDialog::Accepted)
		return;

	apply_terminal_font(dialog.selectedFont());
}

void main_window::apply_terminal_font(const QFont &font)
{
	QFont selected_font = font;
	selected_font.setStyleHint(QFont::Monospace);
	selected_font.setFixedPitch(true);

	selected_font.setPointSize(
		qBound(6, selected_font.pointSize() > 0
				 ? selected_font.pointSize()
				 : 11,
		       72));

	terminal_font = selected_font;

	QSettings settings("Himashinya", "Himashinya");
	settings.setValue("terminal/font_family", terminal_font.family());
	settings.setValue("terminal/font_size", terminal_font.pointSize());

	for (int index = 0; index < terminal_tabs->count(); ++index) {
		auto *terminal =
			qobject_cast<terminal_widget *>(terminal_tabs->widget(index));
		if (terminal)
			terminal->set_terminal_font(terminal_font);
	}
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
			terminal->set_terminal_font(terminal_font);

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

	connect(connection, &ssh_connection::sftp_ready, this,
		[this, connection, server](const QString &home_path,
					   const QString &error) {
			if (!error.isEmpty()) {
				sftp_status->setText(
				    QString("SFTP unavailable: %1").arg(error));
				return;
			}

			if (sftp_connections->count() == 1 &&
			    !sftp_connections->itemData(0).isValid())
				sftp_connections->clear();

			const QSignalBlocker blocker(sftp_connections);
			const int index = sftp_connections->count();
			sftp_connections->addItem(
			    server.name,
			    QVariant::fromValue(
				static_cast<QObject *>(connection)));
			sftp_connections->setItemData(index, home_path,
						      Qt::UserRole + 1);
			sftp_connections->setEnabled(true);
			sftp_connections->setCurrentIndex(index);
			sftp_path->setText(home_path);
			update_sftp_controls();
			refresh_sftp_directory();
		});

	connect(connection, &ssh_connection::sftp_directory_list, this,
		[this, connection](quint64 request_id,
				   const QString &canonical_path,
				   const QVariantList &entries,
				   const QString &error) {
			if (request_id != active_sftp_request_id ||
			    sftp_request_connection != connection)
				return;

			sftp_request_connection.clear();
			if (!error.isEmpty()) {
				sftp_status->setText(error);
				return;
			}

			sftp_path->setText(canonical_path);
			const int index = sftp_connections->currentIndex();
			if (index >= 0)
				sftp_connections->setItemData(
				    index, canonical_path, Qt::UserRole + 1);

			QVariantList sorted_entries = entries;
			std::stable_sort(
			    sorted_entries.begin(), sorted_entries.end(),
			    [](const QVariant &left, const QVariant &right) {
				    const QVariantMap left_entry =
					left.toMap();
				    const QVariantMap right_entry =
					right.toMap();
				    const bool left_is_directory =
					left_entry.value("directory").toBool();
				    const bool right_is_directory =
					right_entry.value("directory")
					    .toBool();
				    if (left_is_directory !=
					right_is_directory)
					    return left_is_directory;
				    return QString::localeAwareCompare(
					       left_entry.value("name")
						   .toString(),
					       right_entry.value("name")
						   .toString()) < 0;
			    });

			sftp_file_list->clear();
			for (const QVariant &value : sorted_entries) {
				const QVariantMap entry = value.toMap();
				const QString name =
				    entry.value("name").toString();
				const bool is_directory =
				    entry.value("directory").toBool();
				const qulonglong size =
				    entry.value("size").toULongLong();
				auto *item = new QListWidgetItem(
				    is_directory
					? QString("[DIR] %1").arg(name)
					: QString("%1    %2 bytes")
					      .arg(name)
					      .arg(size),
				    sftp_file_list);
				item->setData(Qt::UserRole, name);
				item->setData(Qt::UserRole + 1,
					      is_directory);
			}
			sftp_status->setText(
			    QString("%1 items").arg(entries.size()));
			update_sftp_controls();
		});

	connect(connection, &ssh_connection::sftp_transfer_progress, this,
		[this](quint64 request_id, quint64 transferred,
		       quint64 total) {
			if (request_id != active_sftp_request_id)
				return;

			const int percent =
			    total == 0
				? 100
				: static_cast<int>(
				      qMin(100.0,
					   static_cast<double>(transferred) *
					       100.0 /
					       static_cast<double>(total)));
			sftp_progress->setValue(percent);
			sftp_status->setText(
			    QString("%1 %2 / %3 bytes")
				.arg(sftp_transfer_is_upload ? "Uploading"
							     : "Downloading")
				.arg(transferred)
				.arg(total));
		});

	connect(connection, &ssh_connection::sftp_transfer_finished, this,
		&main_window::finish_sftp_transfer);

	connect(connection, &ssh_connection::finished, this,
		[this, connection]() {
			remove_sftp_connection(connection);
		});

	connect(connection, &ssh_connection::failed, this,
		[this, connection](const QString &message) {
			if (!message.isEmpty()) {
				QMessageBox::warning(
				    this, "SSH Connection Failed", message);
			}

			connection->close();
		});

	connection->start();
}

void main_window::refresh_sftp_directory()
{
	if (sftp_transfer_active)
		return;

	const int index = sftp_connections->currentIndex();
	if (index < 0) {
		sftp_status->setText(
		    "Connect to a server to browse files.");
		return;
	}

	auto *connection = qobject_cast<ssh_connection *>(
	    sftp_connections->itemData(index).value<QObject *>());
	if (!connection) {
		sftp_status->setText("The selected connection is unavailable.");
		return;
	}

	active_sftp_request_id = next_sftp_request_id++;
	sftp_request_connection = connection;
	sftp_file_list->clear();
	sftp_status->setText("Loading remote directory...");
	connection->list_sftp_directory(active_sftp_request_id,
					sftp_path->text());
}

void main_window::navigate_sftp_directory(const QString &path)
{
	if (sftp_transfer_active)
		return;
	sftp_path->setText(path);
	refresh_sftp_directory();
}

void main_window::open_sftp_item(QListWidgetItem *item)
{
	if (!item)
		return;

	const QString name = item->data(Qt::UserRole).toString();
	if (item->data(Qt::UserRole + 1).toBool()) {
		QString path = sftp_path->text();
		if (!path.endsWith('/'))
			path.append('/');
		navigate_sftp_directory(path + name);
		return;
	}

	download_sftp_file();
}

void main_window::upload_sftp_file()
{
	if (sftp_transfer_active)
		return;

	const int index = sftp_connections->currentIndex();
	if (index < 0)
		return;
	auto *connection = qobject_cast<ssh_connection *>(
	    sftp_connections->itemData(index).value<QObject *>());
	if (!connection)
		return;

	const QString local_path = QFileDialog::getOpenFileName(
	    this, "Select a file to upload");
	if (local_path.isEmpty())
		return;

	QString remote_path = sftp_path->text();
	if (!remote_path.endsWith('/'))
		remote_path.append('/');
	remote_path.append(QFileInfo(local_path).fileName());

	active_sftp_request_id = next_sftp_request_id++;
	sftp_request_connection = connection;
	sftp_transfer_active = true;
	sftp_transfer_is_upload = true;
	sftp_progress->setValue(0);
	update_sftp_controls();
	connection->upload_sftp_file(active_sftp_request_id, local_path,
				     remote_path);
}

void main_window::download_sftp_file()
{
	if (sftp_transfer_active)
		return;

	auto *item = sftp_file_list->currentItem();
	if (!item || item->data(Qt::UserRole + 1).toBool())
		return;

	const int index = sftp_connections->currentIndex();
	if (index < 0)
		return;
	auto *connection = qobject_cast<ssh_connection *>(
	    sftp_connections->itemData(index).value<QObject *>());
	if (!connection)
		return;

	const QString name = item->data(Qt::UserRole).toString();
	const QString local_path = QFileDialog::getSaveFileName(
	    this, "Save downloaded file", name);
	if (local_path.isEmpty())
		return;

	QString remote_path = sftp_path->text();
	if (!remote_path.endsWith('/'))
		remote_path.append('/');
	remote_path.append(name);

	active_sftp_request_id = next_sftp_request_id++;
	sftp_request_connection = connection;
	sftp_transfer_active = true;
	sftp_transfer_is_upload = false;
	sftp_progress->setValue(0);
	update_sftp_controls();
	connection->download_sftp_file(active_sftp_request_id, remote_path,
				       local_path);
}

void main_window::remove_sftp_connection(ssh_connection *connection)
{
	for (int index = sftp_connections->count() - 1; index >= 0; --index) {
		auto *candidate = qobject_cast<ssh_connection *>(
		    sftp_connections->itemData(index).value<QObject *>());
		if (candidate == connection)
			sftp_connections->removeItem(index);
	}

	if (sftp_request_connection == connection) {
		sftp_request_connection.clear();
		if (sftp_transfer_active) {
			sftp_transfer_active = false;
			sftp_status->setText("The connection was closed.");
			sftp_progress->setValue(0);
		}
	}

	if (sftp_connections->count() == 0) {
		sftp_connections->addItem("Connect to a server first");
		sftp_connections->setEnabled(false);
		sftp_path->clear();
		sftp_file_list->clear();
		sftp_status->setText(
		    "Connect to a server to browse files.");
	}
	update_sftp_controls();
}

void main_window::update_sftp_controls()
{
	const int index = sftp_connections->currentIndex();
	const bool has_connection =
	    index >= 0 &&
	    qobject_cast<ssh_connection *>(
		sftp_connections->itemData(index).value<QObject *>());
	const bool can_operate = has_connection && !sftp_transfer_active;
	sftp_connections->setEnabled(can_operate);
	sftp_path->setEnabled(can_operate);
	sftp_go_button->setEnabled(can_operate);
	sftp_refresh_button->setEnabled(can_operate);
	sftp_file_list->setEnabled(can_operate);
	sftp_upload_button->setEnabled(can_operate);
	sftp_download_button->setEnabled(
	    can_operate && sftp_file_list->currentItem() &&
	    !sftp_file_list->currentItem()
		 ->data(Qt::UserRole + 1)
		 .toBool());
}

void main_window::finish_sftp_transfer(quint64 request_id, bool success,
				       const QString &error)
{
	if (request_id != active_sftp_request_id)
		return;

	sftp_request_connection.clear();
	sftp_transfer_active = false;
	sftp_progress->setValue(success ? 100 : 0);
	if (success) {
		sftp_status->setText(
		    sftp_transfer_is_upload ? "Upload complete."
					    : "Download complete.");
	} else {
		sftp_status->setText(error);
		QMessageBox::warning(this, "SFTP Transfer Failed", error);
	}
	update_sftp_controls();

	if (success && sftp_transfer_is_upload)
		refresh_sftp_directory();
}

void main_window::close_terminal_tab(int index)
{
	QWidget *page = terminal_tabs->widget(index);

	if (!page)
		return;

	auto *connection = qobject_cast<ssh_connection *>(
	    page->property("ssh_connection").value<QObject *>());

	terminal_tabs->removeTab(index);
	page->deleteLater();

	if (!connection)
		return;
	remove_sftp_connection(connection);

	if (connection->is_finished()) {
		connection->deleteLater();
		return;
	}

	connect(connection, &ssh_connection::finished, connection,
		&QObject::deleteLater, Qt::UniqueConnection);

	connection->close();
}