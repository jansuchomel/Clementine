/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "fileview.h"
#include "ui_fileview.h"
#include "core/deletefiles.h"
#include "core/filesystemmusicstorage.h"
#include "ui/iconloader.h"
#include "ui/organiseerrordialog.h"

#include <QFileSystemModel>
#include <QMessageBox>
#include <QScrollBar>

FileView::FileView(QWidget* parent)
    : QWidget(parent),
      ui_(new Ui_FileView),
      model_(NULL),
      undo_stack_(new QUndoStack(this)),
      task_manager_(NULL),
      storage_(new FilesystemMusicStorage("/"))
{
  ui_->setupUi(this);

  // Icons
  ui_->back->setIcon(IconLoader::Load("go-previous"));
  ui_->forward->setIcon(IconLoader::Load("go-next"));
  ui_->home->setIcon(IconLoader::Load("go-home"));
  ui_->up->setIcon(IconLoader::Load("go-up"));

  connect(ui_->back, SIGNAL(clicked()), undo_stack_, SLOT(undo()));
  connect(ui_->forward, SIGNAL(clicked()), undo_stack_, SLOT(redo()));
  connect(ui_->home, SIGNAL(clicked()), SLOT(FileHome()));
  connect(ui_->up, SIGNAL(clicked()), SLOT(FileUp()));
  connect(ui_->path, SIGNAL(textChanged(QString)), SLOT(ChangeFilePath(QString)));

  connect(undo_stack_, SIGNAL(canUndoChanged(bool)), ui_->back, SLOT(setEnabled(bool)));
  connect(undo_stack_, SIGNAL(canRedoChanged(bool)), ui_->forward, SLOT(setEnabled(bool)));

  connect(ui_->list, SIGNAL(activated(QModelIndex)), SLOT(ItemActivated(QModelIndex)));
  connect(ui_->list, SIGNAL(doubleClicked(QModelIndex)), SLOT(ItemDoubleClick(QModelIndex)));
  connect(ui_->list, SIGNAL(Load(QList<QUrl>)), SIGNAL(Load(QList<QUrl>)));
  connect(ui_->list, SIGNAL(AddToPlaylist(QList<QUrl>)), SIGNAL(AddToPlaylist(QList<QUrl>)));
  connect(ui_->list, SIGNAL(CopyToLibrary(QList<QUrl>)), SIGNAL(CopyToLibrary(QList<QUrl>)));
  connect(ui_->list, SIGNAL(MoveToLibrary(QList<QUrl>)), SIGNAL(MoveToLibrary(QList<QUrl>)));
  connect(ui_->list, SIGNAL(Delete(QStringList)), SLOT(Delete(QStringList)));
}

FileView::~FileView() {
  delete ui_;
}

void FileView::SetPath(const QString& path) {
  if (!model_)
    lazy_set_path_ = path;
  else
    ChangeFilePathWithoutUndo(path);
}

void FileView::SetTaskManager(TaskManager* task_manager) {
  task_manager_ = task_manager;
}

void FileView::FileUp() {
  QDir dir(model_->rootDirectory());
  dir.cdUp();

  // Is this the same as going back?  If so just go back, so we can keep the
  // view scroll position.
  if (undo_stack_->canUndo()) {
    const UndoCommand* last_dir = static_cast<const UndoCommand*>(
        undo_stack_->command(undo_stack_->index()-1));
    if (last_dir->undo_path() == dir.path()) {
      undo_stack_->undo();
      return;
    }
  }

  ChangeFilePath(dir.path());
}

void FileView::FileHome() {
  ChangeFilePath(QDir::homePath());
}

void FileView::ChangeFilePath(const QString& new_path_native) {
  QString new_path = QDir::fromNativeSeparators(new_path_native);

  QFileInfo info(new_path);
  if (!info.exists() || !info.isDir())
    return;

  QString old_path(model_->rootPath());
  if (old_path == new_path)
    return;

  undo_stack_->push(new UndoCommand(this, new_path));
}

void FileView::ChangeFilePathWithoutUndo(const QString& new_path) {
  ui_->list->setRootIndex(model_->setRootPath(new_path));
  ui_->path->setText(QDir::toNativeSeparators(new_path));

  QDir dir(new_path);
  ui_->up->setEnabled(dir.cdUp());

  emit PathChanged(new_path);
}

void FileView::ItemActivated(const QModelIndex& index) {
  if (model_->isDir(index))
    ChangeFilePath(model_->filePath(index));
}

void FileView::ItemDoubleClick(const QModelIndex& index) {
  if (model_->isDir(index))
    return;

  emit DoubleClicked(QList<QUrl>() << QUrl::fromLocalFile(model_->filePath(index)));
}


FileView::UndoCommand::UndoCommand(FileView* view, const QString& new_path)
  : view_(view)
{
  old_state_.path = view->model_->rootPath();
  old_state_.scroll_pos = view_->ui_->list->verticalScrollBar()->value();
  old_state_.index = view_->ui_->list->currentIndex();

  new_state_.path = new_path;
}

void FileView::UndoCommand::redo() {
  view_->ChangeFilePathWithoutUndo(new_state_.path);
  if (new_state_.scroll_pos != -1) {
    view_->ui_->list->setCurrentIndex(new_state_.index);
    view_->ui_->list->verticalScrollBar()->setValue(new_state_.scroll_pos);
  }
}

void FileView::UndoCommand::undo() {
  new_state_.scroll_pos = view_->ui_->list->verticalScrollBar()->value();
  new_state_.index = view_->ui_->list->currentIndex();

  view_->ChangeFilePathWithoutUndo(old_state_.path);
  view_->ui_->list->setCurrentIndex(old_state_.index);
  view_->ui_->list->verticalScrollBar()->setValue(old_state_.scroll_pos);
}

void FileView::Delete(const QStringList& filenames) {
  if (filenames.isEmpty())
    return;

  if (QMessageBox::question(this, tr("Delete files"),
        tr("These files will be deleted from disk, are you sure you want to continue?"),
        QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
    return;

  DeleteFiles* delete_files = new DeleteFiles(task_manager_, storage_);
  connect(delete_files, SIGNAL(Finished(SongList)), SLOT(DeleteFinished(SongList)));
  delete_files->Start(filenames);
}

void FileView::DeleteFinished(const SongList& songs_with_errors) {
  if (songs_with_errors.isEmpty())
    return;

  OrganiseErrorDialog* dialog = new OrganiseErrorDialog(this);
  dialog->Show(OrganiseErrorDialog::Type_Delete, songs_with_errors);
  // It deletes itself when the user closes it
}

void FileView::showEvent(QShowEvent* e) {
  QWidget::showEvent(e);

  if (model_)
    return;

  model_ = new QFileSystemModel(this);
  ui_->list->setModel(model_);
  ChangeFilePathWithoutUndo(QDir::homePath());

  if (!lazy_set_path_.isEmpty())
    ChangeFilePathWithoutUndo(lazy_set_path_);
}
