// Copyright 2020 Kukushkina Ksenia
#include "../../modules/task_3/kukushkina_k_segmentation_tbb/bin_image_segmentation.h"
#include <vector>
#include <random>
#include <ctime>
#include <iostream>

static int offset = 0;

std::vector<std::size_t> Generate_pic(std::size_t w, std::size_t h) {
  if (h <= 0 || w <= 0) throw "Trying to generate negative-dim pic";
  std::vector<std::size_t> pic(h * w);
  std::mt19937 gen;
  gen.seed(static_cast<unsigned>(time(0)) + offset++);
#pragma omp parallel for
  for (int i = 0; i < static_cast<int>(h * w); i++) {
    pic[i] = gen() % 2;
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
  tbb::task_scheduler_init init(tbb::task_scheduler_init::automatic);
  std::size_t grain = 100;
  std::vector<std::size_t> tnc;
  tnc.push_back(0);
  tnc.push_back(1);
  std::vector<std::size_t> res(source);
  std::size_t color = 1;
  Segmentation pic(&res, w, h, &color, &tnc);
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
