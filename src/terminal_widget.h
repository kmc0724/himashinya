
#pragma once

#include <QByteArray>
#include <QFont>
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
	void draw_cell(QPainter &painter, int row, int col);
	QString cell_text(const VTermScreenCell &cell) const;
	QColor cell_color(VTermColor color) const;

	VTerm *vterm = nullptr;
	VTermScreen *screen = nullptr;
	VTermState *state = nullptr;

	QFont terminal_font;

	qreal cell_width = 0;
	qreal cell_height = 0;

	int columns = 80;
	int rows = 24;

	VTermPos cursor_position = {0, 0};
	bool cursor_visible = true;
};
