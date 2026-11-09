
#include "terminal_widget.h"

#include <QClipboard>
#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHash>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace
{

bool unicode_trace_enabled()
{
	return qEnvironmentVariableIsSet("HIMASHINYA_TRACE_UNICODE");
}

void write_unicode_trace(const QString &message)
{
	static bool first_write = true;
	QFile file(
	    QDir(QDir::tempPath())
		.filePath(QStringLiteral("himashinya-unicode-trace.log")));
	QIODevice::OpenMode mode = QIODevice::WriteOnly;
	mode |= first_write ? QIODevice::Truncate : QIODevice::Append;
	if (!file.open(mode))
		return;

	file.write(message.toUtf8());
	file.write("\r\n");
	first_write = false;
}

QString codepoint_trace(const QString &text)
{
	QStringList codepoints;
	for (uint codepoint : text.toUcs4())
		codepoints.append(QStringLiteral("U+%1").arg(codepoint, 4, 16,
							     QLatin1Char('0')));
	return codepoints.join(QLatin1Char(' '));
}

QFontDatabase::WritingSystem writing_system_for(const QString &text)
{
	for (uint codepoint : text.toUcs4()) {
		const auto script =
		    QChar::script(static_cast<char32_t>(codepoint));
		switch (script) {
		case QChar::Script_Common:
		case QChar::Script_Inherited:
		case QChar::Script_Unknown:
			continue;
		case QChar::Script_Latin:
			return QFontDatabase::Latin;
		case QChar::Script_Greek:
			return QFontDatabase::Greek;
		case QChar::Script_Cyrillic:
			return QFontDatabase::Cyrillic;
		case QChar::Script_Armenian:
			return QFontDatabase::Armenian;
		case QChar::Script_Hebrew:
			return QFontDatabase::Hebrew;
		case QChar::Script_Arabic:
			return QFontDatabase::Arabic;
		case QChar::Script_Syriac:
			return QFontDatabase::Syriac;
		case QChar::Script_Thaana:
			return QFontDatabase::Thaana;
		case QChar::Script_Devanagari:
			return QFontDatabase::Devanagari;
		case QChar::Script_Bengali:
			return QFontDatabase::Bengali;
		case QChar::Script_Gurmukhi:
			return QFontDatabase::Gurmukhi;
		case QChar::Script_Gujarati:
			return QFontDatabase::Gujarati;
		case QChar::Script_Oriya:
			return QFontDatabase::Oriya;
		case QChar::Script_Tamil:
			return QFontDatabase::Tamil;
		case QChar::Script_Telugu:
			return QFontDatabase::Telugu;
		case QChar::Script_Kannada:
			return QFontDatabase::Kannada;
		case QChar::Script_Malayalam:
			return QFontDatabase::Malayalam;
		case QChar::Script_Sinhala:
			return QFontDatabase::Sinhala;
		case QChar::Script_Thai:
			return QFontDatabase::Thai;
		case QChar::Script_Lao:
			return QFontDatabase::Lao;
		case QChar::Script_Tibetan:
			return QFontDatabase::Tibetan;
		case QChar::Script_Myanmar:
			return QFontDatabase::Myanmar;
		case QChar::Script_Georgian:
			return QFontDatabase::Georgian;
		case QChar::Script_Hangul:
			return QFontDatabase::Korean;
		case QChar::Script_Khmer:
			return QFontDatabase::Khmer;
		case QChar::Script_Hiragana:
		case QChar::Script_Katakana:
			return QFontDatabase::Japanese;
		case QChar::Script_Bopomofo:
			return QFontDatabase::TraditionalChinese;
		case QChar::Script_Han:
			switch (QLocale().language()) {
			case QLocale::Japanese:
				return QFontDatabase::Japanese;
			case QLocale::Korean:
				return QFontDatabase::Korean;
			case QLocale::Chinese:
				return QLocale().territory() ==
						   QLocale::Taiwan ||
					       QLocale().territory() ==
						   QLocale::HongKong ||
					       QLocale().territory() ==
						   QLocale::Macao
					   ? QFontDatabase::TraditionalChinese
					   : QFontDatabase::SimplifiedChinese;
			default:
				continue;
			}
		case QChar::Script_Nko:
			return QFontDatabase::Nko;
		default:
			return QFontDatabase::Other;
		}
	}

	return QFontDatabase::Any;
}

QString fallback_font_family(QFontDatabase::WritingSystem writing_system)
{
	static QHash<int, QString> family_cache;
	const int key = static_cast<int>(writing_system);
	const auto cached = family_cache.constFind(key);
	if (cached != family_cache.cend())
		return cached.value();

	const QStringList families = QFontDatabase::families(writing_system);
	QStringList preferred;
	switch (writing_system) {
	case QFontDatabase::Japanese:
		preferred = {QStringLiteral("Yu Gothic"),
			     QStringLiteral("Meiryo")};
		break;
	case QFontDatabase::Korean:
		preferred = {QStringLiteral("Malgun Gothic"),
			     QStringLiteral("Noto Sans KR")};
		break;
	case QFontDatabase::SimplifiedChinese:
		preferred = {QStringLiteral("Microsoft YaHei"),
			     QStringLiteral("Noto Sans SC")};
		break;
	case QFontDatabase::TraditionalChinese:
		preferred = {QStringLiteral("Microsoft JhengHei"),
			     QStringLiteral("Noto Sans TC")};
		break;
	default:
		break;
	}

	QString family;
	for (const QString &name : preferred) {
		for (const QString &candidate : families) {
			if (candidate.compare(name, Qt::CaseInsensitive) == 0) {
				family = candidate;
				break;
			}
		}
		if (!family.isEmpty())
			break;
	}

	if (family.isEmpty() && !families.isEmpty())
		family = families.first();

	family_cache.insert(key, family);
	return family;
}

QFont cell_font(const QFont &terminal_font,
		QFontDatabase::WritingSystem writing_system)
{
	QFont font = terminal_font;
	if (writing_system != QFontDatabase::Any &&
	    writing_system != QFontDatabase::Latin) {
		const QString family = fallback_font_family(writing_system);
		if (!family.isEmpty()) {
			font.setFamily(family);
			font.setStyleHint(QFont::SansSerif);
			font.setFixedPitch(false);
		}
	}

	return font;
}

} // namespace

terminal_widget::terminal_widget(QWidget *parent) : QWidget(parent)
{
	setFocusPolicy(Qt::StrongFocus);

	render_profiling_enabled =
	    qEnvironmentVariableIsSet("HIMASHINYA_PROFILE_RENDER");
	if (render_profiling_enabled) {
		static bool profile_log_initialized = false;
		render_profile_path =
		    QDir::temp().filePath("himashinya-render-profile.log");
		QFile profile(render_profile_path);
		QIODevice::OpenMode profile_mode =
		    QIODevice::WriteOnly | QIODevice::Text;
		profile_mode |= profile_log_initialized ? QIODevice::Append
							: QIODevice::Truncate;
		if (!profile.open(profile_mode)) {
			qWarning("Cannot open render profile log: %s",
				 qPrintable(render_profile_path));
			render_profiling_enabled = false;
		} else {
			if (!profile_log_initialized) {
				profile.write(
				    "Himashinya terminal render profile\n");
				profile_log_initialized = true;
			}
			paint_interval_clock.start();
			event_loop_clock.start();

			render_profile_timer = new QTimer(this);
			render_profile_timer->setInterval(1000);
			connect(render_profile_timer, &QTimer::timeout, this,
				&terminal_widget::report_render_metrics);
			render_profile_timer->start();

			event_loop_timer = new QTimer(this);
			event_loop_timer->setTimerType(Qt::PreciseTimer);
			event_loop_timer->setInterval(16);
			connect(
			    event_loop_timer, &QTimer::timeout, this, [this]() {
				    const qint64 gap_ns =
					event_loop_clock.nsecsElapsed();
				    event_loop_clock.restart();
				    ++event_loop_tick_count;
				    if (gap_ns > 20'000'000)
					    ++event_loop_late_tick_count;
				    event_loop_max_gap_ns =
					std::max(event_loop_max_gap_ns, gap_ns);
			    });
			event_loop_timer->start();
		}
	}

	selection_scroll_timer = new QTimer(this);
	selection_scroll_timer->setInterval(40);

	connect(selection_scroll_timer, &QTimer::timeout, this,
		&terminal_widget::scroll_selection);

	render_update_timer = new QTimer(this);
	render_update_timer->setSingleShot(true);
	render_update_timer->setTimerType(Qt::PreciseTimer);
	render_update_timer->setInterval(0);
	connect(render_update_timer, &QTimer::timeout, this,
		&terminal_widget::flush_terminal_updates);

	setAttribute(Qt::WA_InputMethodEnabled);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_StaticContents);

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
	    .sb_pushline4 = &terminal_widget::scrollback_pushline,
	};

	vterm_screen_set_callbacks(screen, &callbacks, this);
	vterm_screen_callbacks_has_pushline4(screen);
	vterm_screen_set_reflow(screen, 1);

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

QSizeF terminal_widget::cell_size() const
{
	return QSizeF(cell_width, cell_height);
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

	self->mark_terminal_dirty(area);

	return 1;
}

int terminal_widget::cursor_callback(VTermPos pos, VTermPos, int visible,
				     void *user)
{
	auto *self = static_cast<terminal_widget *>(user);
	const VTermPos old_pos = self->cursor_position;
	const bool was_visible = self->cursor_visible;
	self->cursor_position = pos;
	self->cursor_visible = visible != 0;
	if (was_visible)
		self->update(QRect(old_pos.col * self->cell_width,
				   old_pos.row * self->cell_height,
				   self->cell_width, self->cell_height));
	if (self->cursor_visible)
		self->update(QRect(pos.col * self->cell_width,
				   pos.row * self->cell_height,
				   self->cell_width, self->cell_height));

	return 1;
}

void terminal_widget::output_callback(const char *data, size_t len, void *user)
{
	auto *self = static_cast<terminal_widget *>(user);
	if (unicode_trace_enabled()) {
		const QByteArray bytes(data, static_cast<qsizetype>(len));
		if (std::any_of(bytes.cbegin(), bytes.cend(), [](char byte) {
			    return static_cast<unsigned char>(byte) >= 0x80;
		    }))
			write_unicode_trace(
			    QStringLiteral("terminal tx hex: %1")
				.arg(QString::fromLatin1(bytes.toHex(' '))));
	}

	emit self->send_data(QByteArray(data, static_cast<qsizetype>(len)));
}

void terminal_widget::feed_output(const QByteArray &data)
{
	if (!vterm || data.isEmpty())
		return;
	if (unicode_trace_enabled() &&
	    std::any_of(data.cbegin(), data.cend(), [](char byte) {
		    return static_cast<unsigned char>(byte) >= 0x80;
	    }))
		write_unicode_trace(
		    QStringLiteral("ssh rx hex: %1")
			.arg(QString::fromLatin1(data.toHex(' '))));

	vterm_input_write(vterm, data.constData(),
			  static_cast<size_t>(data.size()));

	vterm_screen_flush_damage(screen);
	if (history_view_dirty) {
		const int old_view_size = history_view.size();
		rebuild_history_view();
		if (scroll_offset > 0) {
			scroll_offset = std::clamp(
			    scroll_offset +
				static_cast<int>(history_view.size()) -
				old_view_size,
			    0, static_cast<int>(history_view.size()));
		}
		invalidate_terminal_buffer();
	}
}

QColor terminal_widget::cell_color(VTermColor color) const
{
	vterm_screen_convert_color_to_rgb(screen, &color);

	return QColor(color.rgb.red, color.rgb.green, color.rgb.blue);
}

QString terminal_widget::cell_text(const VTermScreenCell &cell) const
{
	/* libvterm marks the second column of a wide glyph with UINT32_MAX.
	 * It is a layout sentinel, not a Unicode codepoint to render or copy.
	 */
	if (cell.chars[0] == UINT32_MAX)
		return {};

	int count = 0;

	while (count < VTERM_MAX_CHARS_PER_CELL && cell.chars[count] != 0)
		++count;

	if (count == 0)
		return QStringLiteral(" ");

	return QString::fromUcs4(reinterpret_cast<const char32_t *>(cell.chars),
				 count);
}

void terminal_widget::draw_cell(QPainter &painter, int row, int col,
				const VTermScreenCell &cell,
				bool draw_background)
{
	if (cell.width == 0)
		return;

	const bool wide_char_continuation = cell.chars[0] == UINT32_MAX;

	QColor fg = cell_color(cell.fg);
	QColor bg = cell_color(cell.bg);

	if (cell.attrs.reverse)
		std::swap(fg, bg);

	qreal x = col * cell_width;
	qreal y = row * cell_height;

	QRectF rect(x, y,
		    cell_width * std::max(1, static_cast<int>(cell.width)),
		    cell_height);

	if (draw_background && !wide_char_continuation)
		painter.fillRect(rect, bg);

	QString text = wide_char_continuation ? QString() : cell_text(cell);

	if (!text.trimmed().isEmpty()) {
		const auto writing_system = writing_system_for(text);
		const int style_key = (cell.attrs.bold ? 1 : 0) |
				      (cell.attrs.italic ? 2 : 0) |
				      (cell.attrs.underline ? 4 : 0) |
				      (cell.attrs.strike ? 8 : 0);
		const int font_key =
		    static_cast<int>(writing_system) * 16 + style_key;
		auto font_it = cell_font_cache.find(font_key);
		if (font_it == cell_font_cache.end()) {
			QFont font = cell_font(terminal_font, writing_system);
			font.setBold(cell.attrs.bold);
			font.setItalic(cell.attrs.italic);
			font.setUnderline(cell.attrs.underline != 0);
			font.setStrikeOut(cell.attrs.strike);
			font_it = cell_font_cache.insert(font_key, font);
		}
		const QFont &font = font_it.value();

		painter.setFont(font);
		painter.setPen(fg);

		if (cell.width > 1) {
			const QFontMetricsF metrics(font);
			const qreal text_width =
			    metrics.horizontalAdvance(text);
			const qreal text_x =
			    rect.x() +
			    std::max<qreal>(0, (rect.width() - text_width) / 2);
			const qreal baseline =
			    rect.y() + (rect.height() - metrics.height()) / 2 +
			    metrics.ascent();
			painter.drawText(QPointF(text_x, baseline), text);
		} else {
			painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter,
					 text);
		}
	}
}

void terminal_widget::ensure_terminal_buffer()
{
	const QSize buffer_size(
	    std::max(1, static_cast<int>(std::ceil(columns * cell_width))),
	    std::max(1, static_cast<int>(std::ceil(rows * cell_height))));
	if (terminal_buffer.size() == buffer_size)
		return;

	terminal_buffer =
	    QImage(buffer_size, QImage::Format_ARGB32_Premultiplied);
	if (terminal_buffer.isNull())
		return;

	terminal_buffer.fill(QColor("#1e1e1e"));
	terminal_dirty_region = QRegion(terminal_buffer.rect());
	render_update_timer->start();
}

void terminal_widget::mark_terminal_dirty(const QRect &area)
{
	if (area.isEmpty())
		return;

	terminal_dirty_region += area;
	if (!render_update_timer->isActive())
		render_update_timer->start();
}

void terminal_widget::flush_terminal_updates()
{
	ensure_terminal_buffer();
	render_update_timer->stop();
	if (terminal_buffer.isNull() || !screen)
		return;

	const QRegion pending_damage =
	    terminal_dirty_region.intersected(terminal_buffer.rect());
	terminal_dirty_region = QRegion();
	if (pending_damage.isEmpty())
		return;

	quint64 cells_drawn = 0;
	for (const QRect &area : pending_damage) {
		cells_drawn += render_terminal_region(area);
		update(area);
	}
	if (render_profiling_enabled) {
		++render_buffer_update_count;
		render_cell_count += cells_drawn;
	}
}

void terminal_widget::invalidate_terminal_buffer()
{
	const QRect terminal_area(
	    0, 0,
	    std::max(1, static_cast<int>(std::ceil(columns * cell_width))),
	    std::max(1, static_cast<int>(std::ceil(rows * cell_height))));
	mark_terminal_dirty(terminal_area);
}

quint64 terminal_widget::render_terminal_region(const QRect &area)
{
	const QRect clipped_area = area.intersected(terminal_buffer.rect());
	if (clipped_area.isEmpty() || !screen)
		return 0;

	QPainter painter(&terminal_buffer);
	painter.setClipRect(clipped_area);
	painter.fillRect(clipped_area, QColor("#1e1e1e"));

	const int first_row = std::clamp(
	    static_cast<int>(clipped_area.top() / cell_height), 0, rows - 1);
	const int last_row = std::clamp(
	    static_cast<int>(clipped_area.bottom() / cell_height), 0, rows - 1);
	const int first_col = std::clamp(
	    static_cast<int>(clipped_area.left() / cell_width), 0, columns - 1);
	const int last_col =
	    std::clamp(static_cast<int>(clipped_area.right() / cell_width), 0,
		       columns - 1);
	quint64 cells_drawn = 0;
	const int row_cell_count = last_col - first_col + 1;
	QVector<VTermScreenCell> row_cells(row_cell_count);
	QVector<QColor> row_backgrounds(row_cell_count);
	QVector<bool> row_cell_valid(row_cell_count);

	for (int row = first_row; row <= last_row; ++row) {
		const int logical_row =
		    static_cast<int>(history_view.size()) - scroll_offset + row;

		for (int col = first_col; col <= last_col; ++col) {
			const int index = col - first_col;
			VTermScreenCell cell = {};
			bool valid = false;

			if (logical_row <
			    static_cast<int>(history_view.size())) {
				if (logical_row >= 0) {
					const auto &line =
					    history_view[logical_row].cells;
					if (col < line.size()) {
						cell = line[col];
						valid = true;
					}
				}
			} else {
				const int screen_row =
				    logical_row -
				    static_cast<int>(history_view.size());
				const VTermPos pos = {screen_row, col};
				valid =
				    vterm_screen_get_cell(screen, pos, &cell);
			}

			row_cells[index] = cell;
			row_cell_valid[index] = valid;

			QColor background("#1e1e1e");
			if (valid) {
				QColor foreground = cell_color(cell.fg);
				background = cell_color(cell.bg);
				if (cell.attrs.reverse)
					std::swap(foreground, background);
				++cells_drawn;
			}
			row_backgrounds[index] = background;
		}

		int run_start = 0;
		while (run_start < row_cell_count) {
			int run_end = run_start + 1;
			while (run_end < row_cell_count &&
			       row_backgrounds[run_end] ==
				   row_backgrounds[run_start])
				++run_end;

			painter.fillRect(
			    QRectF((first_col + run_start) * cell_width,
				   row * cell_height,
				   (run_end - run_start) * cell_width,
				   cell_height),
			    row_backgrounds[run_start]);
			run_start = run_end;
		}

		for (int col = first_col; col <= last_col; ++col) {
			const int index = col - first_col;
			const VTermScreenCell &cell = row_cells[index];
			if (!row_cell_valid[index] || cell.width == 0 ||
			    cell.chars[0] == 0 || cell.chars[0] == UINT32_MAX)
				continue;

			draw_cell(painter, row, col, cell, false);
		}
	}

	painter.end();
	return cells_drawn;
}

void terminal_widget::draw_selection_overlay(QPainter &painter,
					     const QRect &area)
{
	if (!has_selection)
		return;

	QPoint start = selection_start;
	QPoint end = selection_end;
	if (start.y() > end.y() ||
	    (start.y() == end.y() && start.x() > end.x()))
		std::swap(start, end);

	const int first_row =
	    std::clamp(static_cast<int>(area.top() / cell_height), 0, rows - 1);
	const int last_row = std::clamp(
	    static_cast<int>(area.bottom() / cell_height), 0, rows - 1);
	const int first_col = std::clamp(
	    static_cast<int>(area.left() / cell_width), 0, columns - 1);
	const int last_col = std::clamp(
	    static_cast<int>(area.right() / cell_width), 0, columns - 1);

	for (int row = first_row; row <= last_row; ++row) {
		const int logical_row =
		    static_cast<int>(history_view.size()) - scroll_offset + row;
		if (logical_row < start.y() || logical_row > end.y())
			continue;

		const int left_col = std::max(
		    first_col, logical_row == start.y() ? start.x() : 0);
		const int right_col = std::min(
		    last_col, logical_row == end.y() ? end.x() : columns - 1);
		if (left_col > right_col)
			continue;

		painter.fillRect(QRectF(left_col * cell_width,
					row * cell_height,
					(right_col - left_col + 1) * cell_width,
					cell_height),
				 QColor(80, 140, 220, 100));
	}
}

void terminal_widget::paintEvent(QPaintEvent *event)
{
	QElapsedTimer paint_clock;
	if (render_profiling_enabled)
		paint_clock.start();

	const QRect dirty_rect =
	    event->region().boundingRect().intersected(rect());
	if (dirty_rect.isEmpty()) {
		if (render_profiling_enabled)
			record_paint_metrics(paint_clock.nsecsElapsed());
		return;
	}

	QPainter painter(this);
	painter.fillRect(dirty_rect, QColor("#1e1e1e"));
	const QRect visible_buffer =
	    dirty_rect.intersected(terminal_buffer.rect());
	if (!visible_buffer.isEmpty())
		painter.drawImage(visible_buffer, terminal_buffer,
				  visible_buffer);

	draw_selection_overlay(painter, dirty_rect);

	if (scroll_offset == 0 && cursor_visible && ime_preedit.isEmpty()) {
		QRectF cursor(cursor_position.col * cell_width,
			      cursor_position.row * cell_height, cell_width,
			      cell_height);

		painter.setPen(QColor("#d4d4d4"));
		painter.drawRect(cursor);
	}

	if (scroll_offset == 0 && !ime_preedit.isEmpty()) {
		const int col = std::clamp(cursor_position.col, 0, columns - 1);
		const int row = std::clamp(cursor_position.row, 0, rows - 1);
		const qreal x = col * cell_width;
		const qreal y = row * cell_height;
		const qreal available_width = std::max<qreal>(0, width() - x);
		const QFont font =
		    cell_font(terminal_font, writing_system_for(ime_preedit));
		const QFontMetricsF metrics(font);
		const qreal text_width = metrics.horizontalAdvance(ime_preedit);
		const qreal preedit_width = std::min(
		    available_width, std::max<qreal>(cell_width, text_width));
		const QRectF preedit_rect(x, y, preedit_width, cell_height);

		painter.fillRect(preedit_rect, QColor(55, 55, 55, 220));
		painter.setFont(font);
		painter.setPen(QColor("#ffffff"));
		const qreal baseline =
		    y + (cell_height - metrics.height()) / 2 + metrics.ascent();
		painter.drawText(QPointF(x, baseline), ime_preedit);

		const qreal underline_width =
		    std::min(available_width, text_width);
		painter.drawLine(
		    QPointF(x, y + cell_height - 1),
		    QPointF(x + underline_width, y + cell_height - 1));

		if (ime_preedit_cursor_visible) {
			const QString prefix =
			    ime_preedit.left(ime_preedit_cursor);
			const qreal cursor_x =
			    x + std::min(available_width,
					 metrics.horizontalAdvance(prefix));
			painter.drawLine(
			    QPointF(cursor_x, y + 2),
			    QPointF(cursor_x, y + cell_height - 2));
		}
	}

	painter.end();
	if (render_profiling_enabled)
		record_paint_metrics(paint_clock.nsecsElapsed());
}

void terminal_widget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (render_profiling_enabled)
		++resize_event_count;
	update_terminal_size();
}

void terminal_widget::record_paint_metrics(qint64 elapsed_ns)
{
	++render_paint_count;
	render_paint_total_ns += elapsed_ns;
	render_paint_max_ns = std::max(render_paint_max_ns, elapsed_ns);
}

void terminal_widget::report_render_metrics()
{
	const qint64 interval_ns = paint_interval_clock.nsecsElapsed();
	paint_interval_clock.restart();
	if (interval_ns <= 0)
		return;

	const double interval_seconds = interval_ns / 1'000'000'000.0;
	const double average_paint_ms =
	    render_paint_count == 0
		? 0.0
		: render_paint_total_ns / 1'000'000.0 / render_paint_count;
	const QString line =
	    QStringLiteral("paint_fps=%1 buffer_fps=%2 "
			   "avg_paint_ms=%3 max_paint_ms=%4 "
			   "cells_per_sec=%5 resize_events=%6 "
			   "loop_ticks=%7 late_loop_ticks=%8 "
			   "max_loop_gap_ms=%9\n")
		.arg(render_paint_count / interval_seconds, 0, 'f', 1)
		.arg(render_buffer_update_count / interval_seconds, 0, 'f', 1)
		.arg(average_paint_ms, 0, 'f', 3)
		.arg(render_paint_max_ns / 1'000'000.0, 0, 'f', 3)
		.arg(qRound64(render_cell_count / interval_seconds))
		.arg(resize_event_count)
		.arg(event_loop_tick_count)
		.arg(event_loop_late_tick_count)
		.arg(event_loop_max_gap_ns / 1'000'000.0, 0, 'f', 3);

	QFile profile(render_profile_path);
	if (!profile.open(QIODevice::WriteOnly | QIODevice::Append |
			  QIODevice::Text)) {
		qWarning("Cannot write render profile log: %s",
			 qPrintable(render_profile_path));
		render_profile_timer->stop();
		event_loop_timer->stop();
		render_profiling_enabled = false;
		return;
	}
	QTextStream(&profile) << line;

	render_paint_count = 0;
	render_buffer_update_count = 0;
	render_cell_count = 0;
	render_paint_total_ns = 0;
	render_paint_max_ns = 0;
	resize_event_count = 0;
	event_loop_tick_count = 0;
	event_loop_late_tick_count = 0;
	event_loop_max_gap_ns = 0;
}

void terminal_widget::set_terminal_font(const QFont &font)
{
	terminal_font = font;
	terminal_font.setStyleHint(QFont::Monospace);
	terminal_font.setFixedPitch(true);
	cell_font_cache.clear();

	QFontMetricsF metrics(terminal_font);
	cell_width = metrics.horizontalAdvance(QLatin1Char('M'));
	cell_height = metrics.height();

	update_terminal_size();
	invalidate_terminal_buffer();
	update();
}

void terminal_widget::update_terminal_size()
{
	if (!vterm || cell_width <= 0 || cell_height <= 0) {
		return;
	}

	int new_columns = std::max(1, static_cast<int>(width() / cell_width));

	int new_rows = std::max(1, static_cast<int>(height() / cell_height));

	if (new_columns == columns && new_rows == rows) {
		return;
	}

	if (history_view_dirty)
		rebuild_history_view();
	const int old_top_row =
	    static_cast<int>(history_view.size()) - scroll_offset;
	const bool preserve_history_position =
	    scroll_offset > 0 && old_top_row >= 0 &&
	    old_top_row < history_view.size();
	const quint64 scroll_anchor =
	    preserve_history_position ? history_view[old_top_row].source_offset
				      : 0;
	auto restore_history_position = [this, preserve_history_position,
					 scroll_anchor]() {
		if (!preserve_history_position || history_view.isEmpty())
			return;

		int anchor_row = 0;
		for (int i = 1; i < history_view.size(); ++i) {
			if (history_view[i].source_offset > scroll_anchor)
				break;
			anchor_row = i;
		}
		scroll_offset =
		    static_cast<int>(history_view.size()) - anchor_row;
	};
	columns = new_columns;
	rows = new_rows;
	rebuild_history_view();
	restore_history_position();

	vterm_set_size(vterm, rows, columns);
	if (history_view_dirty) {
		rebuild_history_view();
		restore_history_position();
	}

	emit resize_requested(columns, rows);
	invalidate_terminal_buffer();
	update();
}

void terminal_widget::keyPressEvent(QKeyEvent *event)
{
	if (!vterm) {
		QWidget::keyPressEvent(event);
		return;
	}

	const auto key = event->key();
	const auto modifiers = event->modifiers();

	const bool ctrl = modifiers.testFlag(Qt::ControlModifier);
	const bool shift = modifiers.testFlag(Qt::ShiftModifier);
	const bool alt = modifiers.testFlag(Qt::AltModifier);

	switch (key) {
	case Qt::Key_Control:
	case Qt::Key_Shift:
	case Qt::Key_Alt:
	case Qt::Key_Meta:
		event->accept();
		return;
	default:
		break;
	}

	/* Copy selected text, otherwise forward Ctrl+C to SSH. */
	if (ctrl && !alt && key == Qt::Key_C) {
		if (has_selection) {
			copy_selection();
			event->accept();
			return;
		}

		if (shift) {
			event->accept();
			return;
		}
	}

	/* Paste clipboard text. */
	if (ctrl && !alt && key == Qt::Key_V) {
		paste_clipboard();
		event->accept();
		return;
	}

	/* Return to the live terminal only for actual input. */
	if (scroll_offset != 0) {
		scroll_offset = 0;
		update();
	}

	if (has_selection) {
		has_selection = false;
		selecting = false;
		update();
	}

	VTermModifier mod = VTERM_MOD_NONE;

	if (shift)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);

	if (alt)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);

	if (ctrl)
		mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);

	VTermKey vkey = VTERM_KEY_NONE;

	switch (key) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
		vkey = VTERM_KEY_ENTER;
		break;
	case Qt::Key_Backspace:
		vkey = VTERM_KEY_BACKSPACE;
		break;
	case Qt::Key_Tab:
		vkey = VTERM_KEY_TAB;
		break;
	case Qt::Key_Escape:
		vkey = VTERM_KEY_ESCAPE;
		break;
	case Qt::Key_Up:
		vkey = VTERM_KEY_UP;
		break;
	case Qt::Key_Down:
		vkey = VTERM_KEY_DOWN;
		break;
	case Qt::Key_Left:
		vkey = VTERM_KEY_LEFT;
		break;
	case Qt::Key_Right:
		vkey = VTERM_KEY_RIGHT;
		break;
	case Qt::Key_Home:
		vkey = VTERM_KEY_HOME;
		break;
	case Qt::Key_End:
		vkey = VTERM_KEY_END;
		break;
	case Qt::Key_Insert:
		vkey = VTERM_KEY_INS;
		break;
	case Qt::Key_Delete:
		vkey = VTERM_KEY_DEL;
		break;
	case Qt::Key_PageUp:
		vkey = VTERM_KEY_PAGEUP;
		break;
	case Qt::Key_PageDown:
		vkey = VTERM_KEY_PAGEDOWN;
		break;
	default:
		if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
			vkey = static_cast<VTermKey>(
			    VTERM_KEY_FUNCTION(key - Qt::Key_F1 + 1));
		break;
	}

	if (vkey != VTERM_KEY_NONE) {
		vterm_keyboard_key(vterm, vkey, mod);
	} else if (ctrl && key >= Qt::Key_A && key <= Qt::Key_Z) {
		vterm_keyboard_unichar(vterm, 'a' + key - Qt::Key_A, mod);
	} else {
		const QString text = event->text();
		if (unicode_trace_enabled() && !text.isEmpty())
			write_unicode_trace(
			    QStringLiteral("key text codepoints: %1")
				.arg(codepoint_trace(text)));
		if (ime_composing && !text.isEmpty()) {
			event->accept();
			return;
		}

		/* Non-ASCII text from an input method must arrive through
		 * inputMethodEvent() as committed text. Forwarding key-event
		 * text here can leak intermediate IME composition into the
		 * remote shell.
		 */
		const auto codepoints = text.toUcs4();
		if (std::any_of(
			codepoints.cbegin(), codepoints.cend(),
			[](uint codepoint) { return codepoint > 0x7f; })) {
			event->accept();
			return;
		}

		for (uint codepoint : codepoints)
			vterm_keyboard_unichar(vterm, codepoint, mod);
	}

	event->accept();
}

void terminal_widget::inputMethodEvent(QInputMethodEvent *event)
{
	if (unicode_trace_enabled()) {
		write_unicode_trace(
		    QStringLiteral("IME preedit: %1 | commit: %2")
			.arg(codepoint_trace(event->preeditString()),
			     codepoint_trace(event->commitString())));
	}
	ime_preedit = event->preeditString();
	ime_composing = !ime_preedit.isEmpty();
	ime_preedit_cursor = ime_preedit.size();
	ime_preedit_cursor_visible = !ime_preedit.isEmpty();

	for (const QInputMethodEvent::Attribute &attribute :
	     event->attributes()) {
		if (attribute.type == QInputMethodEvent::Cursor) {
			ime_preedit_cursor =
			    static_cast<int>(std::clamp<qsizetype>(
				attribute.start, 0, ime_preedit.size()));
			ime_preedit_cursor_visible = attribute.length != 0;
			break;
		}
	}

	const QString text = event->commitString();

	if (vterm && !text.isEmpty()) {
		for (uint codepoint : text.toUcs4()) {
			vterm_keyboard_unichar(vterm, codepoint,
					       VTERM_MOD_NONE);
		}
	}

	if (QGuiApplication::inputMethod())
		QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
	update();
	event->accept();
}

QVariant terminal_widget::inputMethodQuery(Qt::InputMethodQuery query) const
{
	if (query == Qt::ImEnabled)
		return true;
	if (query == Qt::ImCursorRectangle) {
		const int col = std::clamp(cursor_position.col, 0, columns - 1);
		const int row = std::clamp(cursor_position.row, 0, rows - 1);
		qreal x = col * cell_width;
		qreal y = row * cell_height;
		qreal width = cell_width;

		if (!ime_preedit.isEmpty()) {
			const QFont font = cell_font(
			    terminal_font, writing_system_for(ime_preedit));
			const QFontMetricsF metrics(font);
			x += metrics.horizontalAdvance(
			    ime_preedit.left(ime_preedit_cursor));
			width = 1;
		}

		return QRectF(x, y, width, cell_height);
	}

	return QWidget::inputMethodQuery(query);
}

int terminal_widget::scrollback_pushline(int cols, const VTermScreenCell *cells,
					 bool continuation, void *user)
{
	auto *self = static_cast<terminal_widget *>(user);
	history_line line;
	line.cells.resize(cols);
	line.continuation = continuation;
	line.source_offset = self->next_history_source_offset;
	self->next_history_source_offset += static_cast<quint64>(cols);

	std::copy_n(cells, cols, line.cells.begin());

	self->history.append(std::move(line));

	if (self->history.size() > max_history_lines) {
		self->history.removeFirst();
		if (!self->history.isEmpty())
			self->history[0].continuation = false;
	}
	self->history_view_dirty = true;

	return 1;
}

void terminal_widget::rebuild_history_view()
{
	history_view.clear();
	history_view_dirty = false;
	if (columns <= 0 || history.isEmpty())
		return;

	QVector<VTermScreenCell> paragraph;
	QVector<quint64> paragraph_source_offsets;
	quint64 paragraph_start_offset = 0;
	auto add_blank_line = [this]() {
		history_line line;
		line.cells.resize(columns);
		for (VTermScreenCell &cell : line.cells) {
			cell = {};
			cell.width = 1;
		}
		history_view.append(std::move(line));
	};
	auto emit_paragraph = [this, &paragraph, &paragraph_source_offsets,
			       &paragraph_start_offset, &add_blank_line]() {
		if (paragraph.isEmpty()) {
			add_blank_line();
			history_view.last().source_offset =
			    paragraph_start_offset;
			return;
		}

		int source_col = 0;
		while (source_col < paragraph.size()) {
			history_line line;
			line.source_offset =
			    paragraph_source_offsets[source_col];
			line.cells.resize(columns);
			for (VTermScreenCell &cell : line.cells) {
				cell = {};
				cell.width = 1;
			}

			int dest_col = 0;
			while (source_col < paragraph.size() &&
			       dest_col < columns) {
				const VTermScreenCell &cell =
				    paragraph[source_col];
				if (cell.width == 0) {
					++source_col;
					continue;
				}
				if (cell.width == 2 &&
				    dest_col == columns - 1) {
					if (columns > 1)
						break;
					line.cells[dest_col] = cell;
					line.cells[dest_col].width = 1;
					++source_col;
					if (source_col < paragraph.size() &&
					    paragraph[source_col].width == 0)
						++source_col;
					++dest_col;
					continue;
				}

				line.cells[dest_col] = cell;
				++source_col;
				++dest_col;
				if (cell.width == 2 &&
				    source_col < paragraph.size()) {
					if (dest_col < columns) {
						line.cells[dest_col] =
						    paragraph[source_col];
						++dest_col;
					}
					if (paragraph[source_col].width == 0)
						++source_col;
				}
			}

			line.continuation = source_col < paragraph.size();
			history_view.append(std::move(line));
		}
		paragraph.clear();
		paragraph_source_offsets.clear();
	};

	for (int i = 0; i < history.size(); ++i) {
		const history_line &line = history[i];
		if (!line.continuation && !paragraph.isEmpty())
			emit_paragraph();
		if (paragraph.isEmpty())
			paragraph_start_offset = line.source_offset;

		const bool continues =
		    i + 1 < history.size() && history[i + 1].continuation;
		int end = line.cells.size();
		if (!continues) {
			while (end > 0 && line.cells[end - 1].chars[0] == 0)
				--end;
		}
		for (int col = 0; col < end; ++col) {
			paragraph.append(line.cells[col]);
			paragraph_source_offsets.append(line.source_offset +
							col);
		}

		if (!continues)
			emit_paragraph();
	}

	if (!paragraph.isEmpty())
		emit_paragraph();
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
				   static_cast<int>(history_view.size()));
	invalidate_terminal_buffer();

	update();
}

QPoint terminal_widget::mouse_to_cell(const QPoint &position) const
{
	int col = std::clamp(static_cast<int>(position.x() / cell_width), 0,
			     columns - 1);

	int row = std::clamp(static_cast<int>(position.y() / cell_height), 0,
			     rows - 1);

	int logical_row =
	    static_cast<int>(history_view.size()) - scroll_offset + row;

	return QPoint(col, logical_row);
}

void terminal_widget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(event);
		return;
	}

	setFocus();

	if (has_selection)
		update_selection_area(selection_start, selection_end);

	selecting = true;
	has_selection = false;

	selection_start = mouse_to_cell(event->position().toPoint());
	selection_end = selection_start;

	update();
	event->accept();
}

void terminal_widget::mouseMoveEvent(QMouseEvent *event)
{
	if (!selecting)
		return;

	const QPoint previous_end = selection_end;
	selection_mouse_position = event->position().toPoint();

	selection_end = mouse_to_cell(selection_mouse_position);
	has_selection = selection_start != selection_end;

	update_selection_autoscroll();

	update_selection_area(previous_end, selection_end);
	event->accept();
}

void terminal_widget::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		QWidget::mouseReleaseEvent(event);
		return;
	}

	selection_scroll_timer->stop();

	if (selecting) {
		const QPoint previous_end = selection_end;
		selection_end = mouse_to_cell(event->position().toPoint());
		has_selection = selection_start != selection_end;
		selecting = false;
		update_selection_area(previous_end, selection_end);
	}

	event->accept();
}

QString terminal_widget::selected_text() const
{
	if (!has_selection || !screen)
		return {};

	QPoint start = selection_start;
	QPoint end = selection_end;

	if (start.y() > end.y() ||
	    (start.y() == end.y() && start.x() > end.x()))
		std::swap(start, end);

	QString result;

	for (int row = start.y(); row <= end.y(); ++row) {
		QString line;

		int first_col = (row == start.y()) ? start.x() : 0;
		int last_col = (row == end.y()) ? end.x() : columns - 1;

		for (int col = first_col; col <= last_col; ++col) {
			VTermScreenCell cell = {};

			if (row < history_view.size()) {
				const auto &cells = history_view[row].cells;

				if (col >= cells.size())
					continue;

				cell = cells[col];
			} else {
				VTermPos pos = {
				    row - static_cast<int>(history_view.size()),
				    col};

				if (!vterm_screen_get_cell(screen, pos, &cell))
					continue;
			}

			if (cell.width == 0)
				continue;

			line += cell_text(cell);
		}

		result += line.trimmed();

		if (row != end.y())
			result += '\n';
	}

	return result;
}

void terminal_widget::copy_selection()
{
	if (!has_selection)
		return;

	QGuiApplication::clipboard()->setText(selected_text());
}

void terminal_widget::paste_clipboard()
{
	if (!vterm)
		return;

	QString text = QGuiApplication::clipboard()->text();

	if (text.isEmpty())
		return;

	// Windows 줄바꿈을 터미널 입력 형식으로 정규화
	text.replace("\r\n", "\n");
	text.replace('\r', '\n');
	text.replace('\n', '\r');

	// 붙여넣기는 실제 입력이므로 최신 화면으로 이동
	scroll_offset = 0;
	update();

	emit send_data(text.toUtf8());
}

void terminal_widget::update_selection_autoscroll()
{
	if (!selecting) {
		selection_scroll_timer->stop();
		return;
	}

	if (selection_mouse_position.y() < 0 ||
	    selection_mouse_position.y() >= height()) {
		if (!selection_scroll_timer->isActive())
			selection_scroll_timer->start();
	} else {
		selection_scroll_timer->stop();
	}
}

void terminal_widget::scroll_selection()
{
	if (!selecting) {
		selection_scroll_timer->stop();
		return;
	}

	int direction = 0;

	if (selection_mouse_position.y() < 0)
		direction = 1;
	else if (selection_mouse_position.y() >= height())
		direction = -1;
	else {
		selection_scroll_timer->stop();
		return;
	}

	const int previous_offset = scroll_offset;
	const QPoint previous_end = selection_end;
	int distance = 0;

	if (selection_mouse_position.y() < 0)
		distance = -selection_mouse_position.y();
	else if (selection_mouse_position.y() >= height())
		distance = selection_mouse_position.y() - height() + 1;

	int speed = std::clamp(1 + distance / 20, 1, 12);

	scroll_history(direction * speed);

	selection_end = mouse_to_cell(
	    QPoint(selection_mouse_position.x(),
		   std::clamp(selection_mouse_position.y(), 0, height() - 1)));

	has_selection = selection_start != selection_end;

	if (scroll_offset == previous_offset)
		selection_scroll_timer->stop();

	if (scroll_offset != previous_offset)
		update();
	else
		update_selection_area(previous_end, selection_end);
}

void terminal_widget::update_selection_area(const QPoint &first,
					    const QPoint &second)
{
	const int top_row = std::min(first.y(), second.y());
	const int bottom_row = std::max(first.y(), second.y());
	int left_col = 0;
	int right_col = columns - 1;

	if (top_row == bottom_row) {
		left_col = std::max(0, std::min(first.x(), second.x()) - 1);
		right_col =
		    std::min(columns - 1, std::max(first.x(), second.x()) + 1);
	}

	const int first_visible_row =
	    static_cast<int>(history_view.size()) - scroll_offset;
	const int screen_top = std::max(top_row, first_visible_row);
	const int screen_bottom =
	    std::min(bottom_row, first_visible_row + rows - 1);

	if (screen_top > screen_bottom)
		return;

	const int top = (screen_top - first_visible_row) * cell_height;
	const int bottom =
	    (screen_bottom - first_visible_row + 1) * cell_height;
	const int left = static_cast<int>(left_col * cell_width);
	const int right = static_cast<int>(
	    std::min<qreal>(width(), (right_col + 1) * cell_width));

	update(QRect(left, top, std::max(1, right - left),
		     std::max(1, bottom - top)));
}
