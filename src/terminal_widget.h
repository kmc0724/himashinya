
#pragma once

#include <QByteArray>
#include <QFont>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

#ifdef small
#pragma push_macro("small")
#undef small
#define HIMASHINYA_RESTORE_SMALL_MACRO
#endif

#include <vterm.h>

#ifdef HIMASHINYA_RESTORE_SMALL_MACRO
#pragma pop_macro("small")
#undef HIMASHINYA_RESTORE_SMALL_MACRO
#endif

class QPainter;

class terminal_widget : public QWidget
{
	Q_OBJECT

public:
	explicit terminal_widget(QWidget *parent = nullptr);
	~terminal_widget() override;

	void feed_output(const QByteArray &data);
	void set_terminal_font(const QFont &font);

signals:
	void send_data(const QByteArray &data);
	void resize_requested(int columns, int rows);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void inputMethodEvent(QInputMethodEvent *event) override;
	QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
	static int damage_callback(VTermRect rect, void *user);
	static int cursor_callback(VTermPos pos, VTermPos oldpos, int visible,
				   void *user);
	static void output_callback(const char *data, size_t len, void *user);

	void update_terminal_size();
	void draw_cell(QPainter &painter, int row, int col, int logical_row,
		       const VTermScreenCell &cell);

	static int scrollback_pushline(int cols, const VTermScreenCell *cells,
				       void *user);

	static int scrollback_popline(int cols, VTermScreenCell *cells,
				      void *user);

	void wheelEvent(QWheelEvent *event) override;

	void scroll_history(int lines);

	QString cell_text(const VTermScreenCell &cell) const;
	QColor cell_color(VTermColor color) const;

	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

	QPoint mouse_to_cell(const QPoint &position) const;
	QString selected_text() const;
	void copy_selection();
	void paste_clipboard();

	QTimer *selection_scroll_timer = nullptr;
	QPoint selection_mouse_position;

	void update_selection_autoscroll();
	void scroll_selection();
	void update_selection_area(const QPoint &first, const QPoint &second);

	VTerm *vterm = nullptr;
	VTermScreen *screen = nullptr;
	VTermState *state = nullptr;

	QFont terminal_font;
	QString ime_preedit;
	bool ime_composing = false;
	int ime_preedit_cursor = 0;
	bool ime_preedit_cursor_visible = true;

	qreal cell_width = 0;
	qreal cell_height = 0;

	int columns = 80;
	int rows = 24;

	VTermPos cursor_position = {0, 0};
	bool cursor_visible = true;

	struct history_line {
		QVector<VTermScreenCell> cells;
	};

	QVector<history_line> history;

	int scroll_offset = 0;

	static constexpr int max_history_lines = 10000;

	bool selecting = false;
	bool has_selection = false;

	QPoint selection_start;
	QPoint selection_end;
};
