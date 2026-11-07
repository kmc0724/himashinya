
#include "terminal_widget.h"

#include <QColor>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>

terminal_widget::terminal_widget(QWidget *parent) : QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_InputMethodEnabled);
	setAttribute(Qt::WA_OpaquePaintEvent);

	terminal_font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	terminal_font.setPointSize(11);

	QFontMetricsF metrics(terminal_font);

	cell_width = metrics.horizontalAdvance(QLatin1Char('M'));
	cell_height = metrics.height();

	vterm = vterm_new(rows, columns);

	if (!vterm)
		return;

	vterm_set_utf8(vterm, 1);

	screen = vterm_obtain_screen(vterm);
	state = vterm_obtain_state(vterm);

	vterm_screen_enable_altscreen(screen, 1);
	vterm_screen_set_damage_merge(screen, VTERM_DAMAGE_CELL);

	static const VTermScreenCallbacks callbacks = {
	    .damage = &terminal_widget::damage_callback,
	    .movecursor = &terminal_widget::cursor_callback,
	    .sb_pushline = &terminal_widget::scrollback_pushline,
	    .sb_popline = &terminal_widget::scrollback_popline,
	};

	vterm_screen_set_callbacks(screen, &callbacks, this);

	vterm_output_set_callback(vterm, &terminal_widget::output_callback,
				  this);

	/* Initialize terminal state before receiving SSH output. */
	vterm_screen_reset(screen, 1);

	setMinimumSize(320, 180);
}

terminal_widget::~terminal_widget()
{
	if (vterm)
		vterm_free(vterm);
}

int terminal_widget::damage_callback(VTermRect rect, void *user)
{
	auto *self = static_cast<terminal_widget *>(user);

	QRect area(static_cast<int>(rect.start_col * self->cell_width),
		   static_cast<int>(rect.start_row * self->cell_height),
		   static_cast<int>((rect.end_col - rect.start_col) *
				    self->cell_width),
		   static_cast<int>((rect.end_row - rect.start_row) *
				    self->cell_height));

	self->update(area);

	return 1;
}

int terminal_widget::cursor_callback(VTermPos pos, VTermPos, int visible,
				     void *user)
{
	auto *self = static_cast<terminal_widget *>(user);

	self->cursor_position = pos;
	self->cursor_visible = visible != 0;
	self->update();

	return 1;
}

void terminal_widget::output_callback(const char *data, size_t len, void *user)
{
	auto *self = static_cast<terminal_widget *>(user);

	emit self->send_data(QByteArray(data, static_cast<qsizetype>(len)));
}

void terminal_widget::feed_output(const QByteArray &data)
{
	if (!vterm || data.isEmpty())
		return;

	vterm_input_write(vterm, data.constData(),
			  static_cast<size_t>(data.size()));

	vterm_screen_flush_damage(screen);
}

QColor terminal_widget::cell_color(VTermColor color) const
{
	vterm_screen_convert_color_to_rgb(screen, &color);

	return QColor(color.rgb.red, color.rgb.green, color.rgb.blue);
}

QString terminal_widget::cell_text(const VTermScreenCell &cell) const
{
	int count = 0;

	while (count < VTERM_MAX_CHARS_PER_CELL && cell.chars[count] != 0)
		++count;

	if (count == 0)
		return QStringLiteral(" ");

	return QString::fromUcs4(reinterpret_cast<const char32_t *>(cell.chars),
				 count);
}

void terminal_widget::draw_cell(QPainter &painter, int row, int col,
				const VTermScreenCell &cell)
{
	if (cell.width == 0)
		return;

	QColor fg = cell_color(cell.fg);
	QColor bg = cell_color(cell.bg);

	if (cell.attrs.reverse)
		std::swap(fg, bg);

	qreal x = col * cell_width;
	qreal y = row * cell_height;

	QRectF rect(x, y,
		    cell_width * std::max(1, static_cast<int>(cell.width)),
		    cell_height);

	painter.fillRect(rect, bg);

	QString text = cell_text(cell);

	if (!text.trimmed().isEmpty()) {
		QFont font = terminal_font;
		font.setBold(cell.attrs.bold);
		font.setItalic(cell.attrs.italic);
		font.setUnderline(cell.attrs.underline != 0);
		font.setStrikeOut(cell.attrs.strike);

		painter.setFont(font);
		painter.setPen(fg);

		painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
	}
}

void terminal_widget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	painter.fillRect(rect(), QColor("#1e1e1e"));
	painter.setFont(terminal_font);

	if (!screen)
		return;

	for (int row = 0; row < rows; ++row) {
		int logical_row =
		    static_cast<int>(history.size()) - scroll_offset + row;

		for (int col = 0; col < columns; ++col) {
			VTermScreenCell cell = {};

			if (logical_row < static_cast<int>(history.size())) {
				if (logical_row < 0)
					continue;

				const auto &line = history[logical_row].cells;

				if (col >= line.size())
					continue;

				cell = line[col];
			} else {
				int screen_row =
				    logical_row -
				    static_cast<int>(history.size());

				VTermPos pos = {screen_row, col};

				if (!vterm_screen_get_cell(screen, pos, &cell))
					continue;
			}

			draw_cell(painter, row, col, cell);
		}
	}

	if (scroll_offset == 0 && cursor_visible) {
		QRectF cursor(cursor_position.col * cell_width,
			      cursor_position.row * cell_height, cell_width,
			      cell_height);

		painter.setPen(QColor("#d4d4d4"));
		painter.drawRect(cursor);
	}
}

void terminal_widget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	update_terminal_size();
}

void terminal_widget::update_terminal_size()
{
	if (!vterm || cell_width <= 0 || cell_height <= 0)
		return;

	int new_columns = std::max(1, static_cast<int>(width() / cell_width));

	int new_rows = std::max(1, static_cast<int>(height() / cell_height));

	if (new_columns == columns && new_rows == rows)
		return;

	columns = new_columns;
	rows = new_rows;

	vterm_set_size(vterm, rows, columns);

	emit resize_requested(columns, rows);
	update();
}

void terminal_widget::keyPressEvent(QKeyEvent *event)
{
	if (!vterm) {
		QWidget::keyPressEvent(event);
		return;
	}

	/* Return to the live terminal on keyboard input. */
	if (scroll_offset != 0) {
		scroll_offset = 0;
		update();
	}

	VTermModifier mod = VTERM_MOD_NONE;

	if (event->modifiers() & Qt::ShiftModifier)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);

	if (event->modifiers() & Qt::ControlModifier)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

	if (event->modifiers() & Qt::AltModifier)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);

	VTermKey key;

	switch (event->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
		key = VTERM_KEY_ENTER;
		break;

	case Qt::Key_Backspace:
		key = VTERM_KEY_BACKSPACE;
		break;

	case Qt::Key_Tab:
		key = VTERM_KEY_TAB;
		break;

	case Qt::Key_Escape:
		key = VTERM_KEY_ESCAPE;
		break;

	case Qt::Key_Up:
		key = VTERM_KEY_UP;
		break;

	case Qt::Key_Down:
		key = VTERM_KEY_DOWN;
		break;

	case Qt::Key_Left:
		key = VTERM_KEY_LEFT;
		break;

	case Qt::Key_Right:
		key = VTERM_KEY_RIGHT;
		break;

	case Qt::Key_Home:
		key = VTERM_KEY_HOME;
		break;

	case Qt::Key_End:
		key = VTERM_KEY_END;
		break;

	case Qt::Key_Insert:
		key = VTERM_KEY_INS;
		break;

	case Qt::Key_Delete:
		key = VTERM_KEY_DEL;
		break;

	case Qt::Key_PageUp:
		key = VTERM_KEY_PAGEUP;
		break;

	case Qt::Key_PageDown:
		key = VTERM_KEY_PAGEDOWN;
		break;

	default:
		if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
			key = static_cast<VTermKey>(
			    VTERM_KEY_FUNCTION(event->key() - Qt::Key_F1 + 1));
			vterm_keyboard_key(vterm, key, mod);
			event->accept();
			return;
		}

		QString text = event->text();

		if (text.isEmpty()) {
			QWidget::keyPressEvent(event);
			return;
		}

		for (uint codepoint : text.toUcs4()) {
			vterm_keyboard_unichar(vterm, codepoint, mod);
		}

		event->accept();
		return;
	}

	vterm_keyboard_key(vterm, key, mod);
	event->accept();
}

void terminal_widget::inputMethodEvent(QInputMethodEvent *event)
{
	const QString text = event->commitString();

	if (vterm && !text.isEmpty()) {
		for (uint codepoint : text.toUcs4()) {
			vterm_keyboard_unichar(vterm, codepoint,
					       VTERM_MOD_NONE);
		}
	}

	event->accept();
}

QVariant terminal_widget::inputMethodQuery(Qt::InputMethodQuery query) const
{
	if (query == Qt::ImEnabled)
		return true;

	return QWidget::inputMethodQuery(query);
}

int terminal_widget::scrollback_pushline(int cols, const VTermScreenCell *cells,
					 void *user)
{
	auto *self = static_cast<terminal_widget *>(user);

	history_line line;
	line.cells.resize(cols);

	std::copy_n(cells, cols, line.cells.begin());

	self->history.append(std::move(line));

	if (self->history.size() > max_history_lines)
		self->history.removeFirst();

	if (self->scroll_offset > 0) {
		self->scroll_offset =
		    std::min(self->scroll_offset + 1,
			     static_cast<int>(self->history.size()));
	}

	return 1;
}

int terminal_widget::scrollback_popline(int cols, VTermScreenCell *cells,
					void *user)
{
	auto *self = static_cast<terminal_widget *>(user);

	if (self->history.isEmpty())
		return 0;

	history_line line = self->history.takeLast();

	int count = std::min(cols, static_cast<int>(line.cells.size()));

	std::copy_n(line.cells.constBegin(), count, cells);

	for (int i = count; i < cols; ++i)
		cells[i] = {};

	self->scroll_offset = std::min(self->scroll_offset,
				       static_cast<int>(self->history.size()));

	return 1;
}

void terminal_widget::wheelEvent(QWheelEvent *event)
{
	int steps = event->angleDelta().y() / 120;

	if (steps == 0) {
		event->ignore();
		return;
	}

	scroll_history(steps * 3);
	event->accept();
}

void terminal_widget::scroll_history(int lines)
{
	scroll_offset = std::clamp(scroll_offset + lines, 0,
				   static_cast<int>(history.size()));

	update();
}