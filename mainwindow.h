#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <vector>
#include <string>

#include "Library.hpp"
#include "Illumina.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QSpinBox;
class QFrame;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnAddBlock_clicked();
    void on_btnRefresh_clicked();
    void on_btnStart_clicked();
    void on_sim_tick();

private:
    struct BlockRow {
        QFrame* frame = nullptr;
        QSpinBox* percent = nullptr;
        QSpinBox* minLen = nullptr;
        QSpinBox* maxLen = nullptr;
    };

    enum class Phase { left, right, done };

    Ui::MainWindow* ui = nullptr;
    std::vector<BlockRow> blocks_;

    void ensure_layout_has_spacer();
    void add_row_widget(QFrame* frame);
    void reset_blocks();

    BlockRow create_row();
    int sum_percent() const;
    bool can_start() const;
    void update_state();

    LibraryParameters build_library_parameters() const;

    void show_black_screen();

    static std::string revcomp(const std::string& s);

    void start_phase(Phase p);
    bool phase_finished() const;

    void save_reads_to_txt_named(const QString& filename,
                                 const std::vector<std::string>& reads,
                                 const QString& label_prefix,
                                 bool open_after);

    void save_library_to_txt_named(const QString& filename,
                                   const std::vector<std::string>& lib,
                                   bool open_after);

    Phase phase_ = Phase::done;

    std::vector<std::string> last_reads_;
    std::vector<std::string> right_library_;

    std::vector<std::string> left_called_;
    std::vector<std::string> right_called_;

    NGS::Illumina sim_;
    QTimer* simTimer_ = nullptr;
};

#endif
