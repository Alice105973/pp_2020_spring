// Copyright 2020 Kukushkina Ksenia
#include "../../modules/task_3/kukushkina_k_segmentation_tbb/bin_image_segmentation.h"
#include <vector>
#include <random>
#include <ctime>
#include <iostream>

static int offset = 0;

std::vector<std::size_t> Generate_pic(std::size_t w, std::size_t h) {
  if (h <= 0 || w <= 0) throw "Trying to generate negative-dim pic";
  std::vector<std::size_t> pic(h * w, 0);
  std::mt19937 gen;
  gen.seed(static_cast<unsigned>(time(0)) + offset++);
  std::size_t segcount = gen() % w + 1;
  segcount *= 2;
  for (size_t i = 0; i < segcount; i++) {
    size_t curx, cury;
    curx = gen() % w;
    cury = gen() % h;
    size_t segsize = gen() % (h * w / segcount);
    for (size_t j = 0; j < segsize; j++) {
      pic[curx + w * cury] = 1;
      int dir = gen() % 4;
      switch (dir) {
      case 0:
        if (cury != 0) {
          if (pic[curx + (w - 1) * cury] == 0) {
            cury--;
            break;
          }
        }
      case 1:
        if (curx != 0) {
          if (pic[curx - 1 + w * cury] == 0) {
            curx--;
            break;
          }
        }
      case 2:
        if (cury != h - 1) {
          if (pic[curx + (w + 1) * cury] == 0) {
            cury++;
            break;
          }
        }
      case 3:
        if (curx != w - 1) {
          if (pic[curx + 1 + w * cury] == 0) {
            curx++;
            break;
          }
        }
      }
    }
  }
  return pic;
}

class Segmentation {
  std::vector<std::size_t>* result;
  std::size_t w;
  std::size_t* color;
  std::vector<std::size_t>* newColor;
 public:
  Segmentation(std::vector<std::size_t>* tresult,
    std::size_t tw, std::size_t th, std::size_t* tcolor,
    std::vector<std::size_t>* tnc) :
    result(tresult), w(tw), color(tcolor), newColor(tnc) {}

  void operator() (const tbb::blocked_range<std::size_t>& r) const;
};

class Recolor {
  std::vector<std::size_t>* result;
  std::size_t w;
  const std::vector<std::size_t>& newColor;
 public:
  Recolor(std::vector<std::size_t>* tresult, std::size_t tw,
    const std::vector<std::size_t>& tnc) :
    result(tresult), w(tw), newColor(tnc) {}

  void operator() (const tbb::blocked_range<std::size_t>& r) const;
};

void Segmentation::operator() (const tbb::blocked_range<std::size_t>& r) const {
  tbb::spin_mutex::scoped_lock lock;
  tbb::spin_mutex::scoped_lock lock1;
  size_t begin = w * r.begin();
  size_t end = w * r.end();

  if ((*result)[begin] == 1) {  // first elem
    lock.acquire(mut_new_seg);
    (*color)++;
    (*result)[begin] = *color;
    newColor->push_back(*color);
    lock.release();
  }

  for (std::size_t i = begin + 1; i < begin + w; i++) {  // first row
    if ((*result)[i] == 0)  // empty cell
      continue;
    if ((*result)[i - 1] == 0 || i % w == 0) {  // new seg
      lock.acquire(mut_new_seg);
      (*color)++;
      (*result)[i] = *color;
      newColor->push_back(*color);
      lock.release();
      continue;
    }
    (*result)[i] = (*result)[i - 1];
  }

  for (std::size_t i = begin + w; i < end; i++) {  // other rows
    if ((*result)[i] == 0)  // empty cell
      continue;
    if (((*result)[i - 1] == 0 && (*result)[i - w] == 0) || i % w == 0) {  // new seg
      lock.acquire(mut_new_seg);
      (*color)++;
      (*result)[i] = *color;
      newColor->push_back(*color);
      lock.release();
      continue;
    }
    if ((*result)[i - 1] == 0) {  // only upper is colored
      (*result)[i] = (*result)[i - w];
      continue;
    }
    if ((*result)[i - w] == 0) {  // only left is colored
      (*result)[i] = (*result)[i - 1];
      continue;
    }
    // both upper & left are colored
    std::size_t leftcolor = (*result)[i - 1];
    (*result)[i] = leftcolor;
  }
}

void Recolor::operator() (const tbb::blocked_range<std::size_t>& r) const {
  std::size_t begin = w * r.begin();
  std::size_t end = w * r.end();
  for (std::size_t i = begin; i < end; i++)
    (*result)[i] = newColor[(*result)[i]];
}


std::vector<std::size_t> Process(const std::vector<std::size_t>& source, std::size_t w, std::size_t h) {
  std::cout << "Start\n";
  tbb::task_scheduler_init init(tbb::task_scheduler_init::automatic);
  std::cout << "Init\n";
  std::size_t grain = 100;
  std::cout << "Grain\n";
  std::vector<std::size_t> tnc;
  std::cout << "new vec\n";
  tnc.push_back(0);
  std::cout << "pushback 0\n";
  tnc.push_back(1);
  std::cout << "pushback 1\n";
  std::vector<std::size_t> res(source);
  std::cout << "new res copy\n";
  std::size_t color = 1;
  Segmentation pic(&res, w, h, &color, &tnc);
  std::cout << "new segmentation\n";
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, h, grain), pic);
  for (std::size_t i = w; i < res.size(); i++) {
    if (res[i] == 0 || res[i - w] == 0)
      continue;
    std::size_t cur = tnc[res[i]];
    std::size_t up = tnc[res[i - w]];
    for (std::size_t j = 2; j < tnc.size(); j++) {
      if (tnc[j] == cur)
        tnc[j] = up;
    }
  }
  Recolor rec(&res, w, tnc);
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, h, grain), rec);
  init.terminate();
  return res;
}

void Output(const std::vector<std::size_t>& source, std::size_t w) {
  for (size_t i = 0; i < source.size(); i++) {
    if (i % w == 0)
      std::cout << std::endl;
    std::cout << source[i] << " ";
  }
  std::cout << std::endl;
}
