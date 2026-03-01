#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QTimer>

#include <opencv2/opencv.hpp>
#include <algorithm>

static QImage mat_to_qimage_bgr(const cv::Mat& mat) {
    QImage img(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_BGR888);
    return img.copy();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    ui->btnStart->setEnabled(false);

    ensure_layout_has_spacer();

    on_btnAddBlock_clicked();
    update_state();

    if (ui->spinLeftReadLen) {
        connect(ui->spinLeftReadLen, qOverload<int>(&QSpinBox::valueChanged), this, [this] { update_state(); });
    }
    if (ui->spinRightReadLen) {
        connect(ui->spinRightReadLen, qOverload<int>(&QSpinBox::valueChanged), this, [this] { update_state(); });
    }
    if (ui->spinTotalReads) {
        connect(ui->spinTotalReads, qOverload<int>(&QSpinBox::valueChanged), this, [this] { update_state(); });
    }

    QTimer::singleShot(0, this, [this] { show_black_screen(); });

    simTimer_ = new QTimer(this);
    connect(simTimer_, &QTimer::timeout, this, &MainWindow::on_sim_tick);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::show_black_screen() {
    if (!ui->labelScreen) return;

    const int w = std::max(1, ui->labelScreen->width());
    const int h = std::max(1, ui->labelScreen->height());

    QImage img(w, h, QImage::Format_RGB888);
    img.fill(Qt::black);
    ui->labelScreen->setPixmap(QPixmap::fromImage(img));
}

std::string MainWindow::revcomp(const std::string& s) {
    auto comp = [](char c) -> char {
        switch (c) {
        case 'A': return 'T';
        case 'T': return 'A';
        case 'C': return 'G';
        case 'G': return 'C';
        default:  return 'N';
        }
    };

    std::string out;
    out.reserve(s.size());
    for (auto it = s.rbegin(); it != s.rend(); ++it) out.push_back(comp(*it));
    return out;
}

void MainWindow::ensure_layout_has_spacer() {
    auto* lay = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    if (!lay) return;

    lay->setAlignment(Qt::AlignTop);

    for (int i = 0; i < lay->count(); ++i) {
        QLayoutItem* it = lay->itemAt(i);
        if (!it) continue;
        if (!it->widget() && !it->layout()) return;
    }

    lay->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void MainWindow::add_row_widget(QFrame* frame) {
    auto* lay = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    if (!lay) return;

    ensure_layout_has_spacer();
    const int insert_pos = std::max(0, lay->count() - 1);
    lay->insertWidget(insert_pos, frame);
}

MainWindow::BlockRow MainWindow::create_row() {
    BlockRow r;

    r.frame = new QFrame(ui->scrollAreaWidgetContents);
    r.frame->setFrameShape(QFrame::StyledPanel);
    r.frame->setFrameShadow(QFrame::Plain);
    r.frame->setLineWidth(2);

    auto* h = new QHBoxLayout(r.frame);
    h->setContentsMargins(12, 10, 12, 10);
    h->setSpacing(14);

    r.percent = new QSpinBox(r.frame);
    r.percent->setRange(0, 100);
    r.percent->setSuffix("%");
    r.percent->setFixedWidth(90);

    auto* minLabel = new QLabel("min:", r.frame);
    r.minLen = new QSpinBox(r.frame);
    r.minLen->setRange(0, 1000000);
    r.minLen->setFixedWidth(140);

    auto* maxLabel = new QLabel("max:", r.frame);
    r.maxLen = new QSpinBox(r.frame);
    r.maxLen->setRange(0, 1000000);
    r.maxLen->setFixedWidth(140);

    h->addWidget(r.percent);
    h->addWidget(minLabel);
    h->addWidget(r.minLen);
    h->addWidget(maxLabel);
    h->addWidget(r.maxLen);
    h->addStretch(1);

    auto on_change = [this]() { update_state(); };
    connect(r.percent, qOverload<int>(&QSpinBox::valueChanged), this, on_change);
    connect(r.minLen, qOverload<int>(&QSpinBox::valueChanged), this, on_change);
    connect(r.maxLen, qOverload<int>(&QSpinBox::valueChanged), this, on_change);

    return r;
}

int MainWindow::sum_percent() const {
    int sum = 0;
    for (const auto& r : blocks_) sum += r.percent->value();
    return sum;
}

bool MainWindow::can_start() const {
    int sum = 0;
    for (const auto& r : blocks_) {
        const int p = r.percent->value();
        if (p == 0) continue;

        const int mn = r.minLen->value();
        const int mx = r.maxLen->value();
        if (mn <= 0 || mx <= 0 || mn > mx) return false;

        sum += p;
        if (sum > 100) return false;
        if (sum == 100) return true;
    }
    return false;
}

void MainWindow::update_state() {
    const int sum = sum_percent();
    ui->labelSum->setText(QString("sum: %1%").arg(sum));

    const bool reads_ok = ui->spinTotalReads && ui->spinTotalReads->value() > 0;
    const int left_len = ui->spinLeftReadLen ? ui->spinLeftReadLen->value() : 0;
    const int right_len = ui->spinRightReadLen ? ui->spinRightReadLen->value() : 0;
    const bool any_len_ok = (left_len > 0) || (right_len > 0);

    ui->btnAddBlock->setEnabled(sum < 100);
    ui->btnStart->setEnabled(sum == 100 && can_start() && reads_ok && any_len_ok);
}

void MainWindow::on_btnAddBlock_clicked() {
    const int sum = sum_percent();
    if (sum >= 100) return;

    BlockRow r = create_row();
    add_row_widget(r.frame);

    blocks_.push_back(r);
    update_state();
}

void MainWindow::reset_blocks() {
    if (simTimer_) simTimer_->stop();
    phase_ = Phase::done;

    left_called_.clear();
    right_called_.clear();
    right_library_.clear();
    last_reads_.clear();

    for (auto& r : blocks_) delete r.frame;
    blocks_.clear();

    auto* lay = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    if (!lay) return;

    while (QLayoutItem* item = lay->takeAt(0)) {
        if (QWidget* w = item->widget()) delete w;
        delete item;
    }

    lay->setAlignment(Qt::AlignTop);
    lay->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    ui->labelSum->setText("sum: 0%");
    ui->btnStart->setEnabled(false);
    ui->btnAddBlock->setEnabled(true);

    show_black_screen();

    on_btnAddBlock_clicked();
    update_state();
}

void MainWindow::on_btnRefresh_clicked() {
    reset_blocks();
}

LibraryParameters MainWindow::build_library_parameters() const {
    LibraryParameters p;
    p.total_reads = static_cast<size_t>(ui->spinTotalReads->value());

    p.blocks.clear();
    p.blocks.reserve(blocks_.size());

    size_t sum = 0;
    for (const auto& r : blocks_) {
        const int pct = r.percent->value();
        if (pct <= 0) continue;

        ParametersBlock b;
        b.percent = static_cast<size_t>(pct);
        b.min_len = static_cast<size_t>(r.minLen->value());
        b.max_len = static_cast<size_t>(r.maxLen->value());

        p.blocks.push_back(b);

        sum += b.percent;
        if (sum >= 100) break;
    }

    return p;
}

static QString desktop_path_file(const QString& filename) {
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    return QDir(desktop).filePath(filename);
}

void MainWindow::save_reads_to_txt_named(const QString& filename,
                                         const std::vector<std::string>& reads,
                                         const QString& label_prefix,
                                         bool open_after) {
    const QString path = desktop_path_file(filename);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "error", QString("cannot write file:\n%1").arg(path));
        return;
    }

    QTextStream out(&f);
    for (int i = 0; i < static_cast<int>(reads.size()); ++i) {
        out << label_prefix << '.' << (i + 1) << '\n';
        out << QString::fromStdString(reads[static_cast<size_t>(i)]) << '\n';
    }
    f.close();

    if (open_after) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::save_library_to_txt_named(const QString& filename,
                                           const std::vector<std::string>& lib,
                                           bool open_after) {
    save_reads_to_txt_named(filename, lib, "lib", open_after);
}

void MainWindow::start_phase(Phase p) {
    const int w = std::max(1, ui->labelScreen->width());
    const int h = std::max(1, ui->labelScreen->height());

    phase_ = p;

    if (p == Phase::left) {
        const int len = ui->spinLeftReadLen->value();
        if (len <= 0) return;
        sim_.set_library(last_reads_);
        sim_.set_read_length(len);
    } else if (p == Phase::right) {
        const int len = ui->spinRightReadLen->value();
        if (len <= 0) return;
        sim_.set_library(right_library_);
        sim_.set_read_length(len);
    } else {
        return;
    }

    sim_.init(w, h);
    if (simTimer_) simTimer_->start(200);
}

bool MainWindow::phase_finished() const {
    if (phase_ == Phase::left)  return sim_.cycle() >= ui->spinLeftReadLen->value();
    if (phase_ == Phase::right) return sim_.cycle() >= ui->spinRightReadLen->value();
    return true;
}

void MainWindow::on_btnStart_clicked() {
    try {
        if (simTimer_) simTimer_->stop();

        const LibraryParameters params = build_library_parameters();

        Library lib;
        last_reads_ = lib.generate(params);

        right_library_.clear();
        right_library_.reserve(last_reads_.size());
        for (const auto& s : last_reads_) right_library_.push_back(revcomp(s));

        left_called_.clear();
        right_called_.clear();

        phase_ = Phase::done;

        if (ui->spinLeftReadLen->value() > 0) {
            start_phase(Phase::left);
        } else if (ui->spinRightReadLen->value() > 0) {
            start_phase(Phase::right);
        } else {
            show_black_screen();
        }

    } catch (const std::exception& e) {
        QMessageBox::critical(this, "error", e.what());
    }
}

void MainWindow::on_sim_tick() {
    cv::Mat frame = sim_.step();

    const QImage img = mat_to_qimage_bgr(frame);
    ui->labelScreen->setPixmap(QPixmap::fromImage(img));

    if (!phase_finished()) return;

    if (simTimer_) simTimer_->stop();

    if (phase_ == Phase::left) {
        left_called_ = sim_.reads();
        save_reads_to_txt_named("Left_Reads.txt", left_called_, "Left_Read", true);

        if (ui->spinRightReadLen->value() > 0) {
            start_phase(Phase::right);
        } else {
            save_library_to_txt_named("library.txt", last_reads_, true);
            show_black_screen();
            phase_ = Phase::done;
        }
        return;
    }

    if (phase_ == Phase::right) {
        right_called_ = sim_.reads();
        save_reads_to_txt_named("Right_Reads.txt", right_called_, "Right_Read", true);

        save_library_to_txt_named("library.txt", last_reads_, true);

        show_black_screen();
        phase_ = Phase::done;
        return;
    }
}
