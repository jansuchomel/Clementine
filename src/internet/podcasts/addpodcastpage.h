/* This file is part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>
   Copyright 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>

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

#ifndef INTERNET_PODCASTS_ADDPODCASTPAGE_H_
#define INTERNET_PODCASTS_ADDPODCASTPAGE_H_

#include <QWidget>

class Application;
class PodcastDiscoveryModel;

class AddPodcastPage : public QWidget {
  Q_OBJECT

 public:
  explicit AddPodcastPage(Application* app, QWidget* parent = nullptr);

  PodcastDiscoveryModel* model() const { return model_; }

  virtual bool has_visible_widget() const { return true; }
  virtual void Show() {}

 signals:
  void Busy(bool busy);

 protected:
  void SetModel(PodcastDiscoveryModel* model);

 private:
  PodcastDiscoveryModel* model_;
};

#endif  // INTERNET_PODCASTS_ADDPODCASTPAGE_H_
